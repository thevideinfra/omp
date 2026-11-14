#include <QtTest>

#include "fakesecrettool.h"
#include "fakeytm.h"
#include "library.h"
#include "playlists.h"
#include "secrets.h"
#include "ytmsource.h"

class TstYtm : public QObject
{
    Q_OBJECT
    QTemporaryDir dir;

    struct Env
    {
        Library library;
        Playlists playlists;
        Secrets secrets;
        YtmSource source;
        Env(const QString &d, const QString &helper)
            : library({"/m"}, "/nonexistent-scanner", d + "/index.jsonl")
            , playlists(d + "/pl")
            , secrets(d + "/secret-tool")
            , source(library, playlists, secrets, helper)
        {
        }
    };

    void useFreshFolder()
    {
        QDir(dir.path()).removeRecursively();
        QDir().mkpath(dir.path());
        qputenv("FAKE_SECRET_DIR", dir.path().toUtf8());
        qputenv("FAKE_YTM_DIR", dir.path().toUtf8());
        qputenv("FAKE_YTM_MODE", "ok");
        writeFake(dir.path());
        writeHelper(dir.path());
    }

private slots:
    void init() { useFreshFolder(); }

    void withoutALoginItStaysOffAndNothingRuns()
    {
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.start();
        QTRY_COMPARE(e.source.state(), QString("off"));
        QVERIFY(!QFileInfo::exists(dir.path() + "/seen"));
    }

    void aPastedLoginIsKeptAndTheSongsAndPlaylistsArrive()
    {
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.start();
        e.source.login("curl 'https://music.youtube.com/x' -b 'SID=1'");
        QTRY_COMPARE(e.source.state(), QString("connected"));
        QCOMPARE(e.source.status(), QString("Connected: 2 songs"));
        QCOMPARE(e.library.remoteTracksOf("ytm").size(), 2);
        QVERIFY(QFileInfo::exists(dir.path() + "/subsonic-ytm.jsonl"));
        QCOMPARE(e.playlists.list()->rowCount(), 1);
        QCOMPARE(e.playlists.list()->get(0).value("title").toString(), QString("Mix"));
        QCOMPARE(e.playlists.list()->get(0).value("meta").toString(), QString("YouTube Music"));
        QVERIFY(e.playlists.list()->get(0).value("readOnly").toBool());
        QCOMPARE(e.playlists.paths(e.playlists.list()->get(0).value("key").toString()), (QStringList{"ytm:bbb", "ytm:aaa"}));
        QCOMPARE(e.library.tracksForPaths({"ytm:aaa"}).size(), 1);
        QFile seen(dir.path() + "/seen");                                      // the helper got the login on stdin
        QVERIFY(seen.open(QIODevice::ReadOnly));
        QVERIFY(seen.readAll().contains("c1"));
    }

    void aTextThatIsNotALoginIsRefusedWithTheHelpersMessage()
    {
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.login("hello");
        QTRY_COMPARE(e.source.state(), QString("off"));
        QCOMPARE(e.source.status(), QString("no cookie"));
    }

    void theNextRunShowsTheCachedSongsAtOnceAndSyncsWithTheKeptLogin()
    {
        {
            Env e(dir.path(), dir.path() + "/omp-ytm");
            e.source.login("curl x");
            QTRY_COMPARE(e.source.state(), QString("connected"));
        }
        QFile::remove(dir.path() + "/seen");
        qputenv("FAKE_YTM_MODE", "auth");
        Env again(dir.path(), dir.path() + "/omp-ytm");
        again.source.start();
        QCOMPARE(again.library.remoteTracksOf("ytm").size(), 2);               // from the cache, before the helper answers
        QTRY_COMPARE(again.source.state(), QString("expired"));
        QCOMPARE(again.library.remoteTracksOf("ytm").size(), 2);               // and they stay
    }

    void anExpiredLoginAndAMissingHelperAreToldApart()
    {
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.login("curl x");
        QTRY_COMPARE(e.source.state(), QString("connected"));
        qputenv("FAKE_YTM_MODE", "missing");
        e.source.sync();
        QTRY_COMPARE(e.source.state(), QString("missing"));
        QVERIFY(e.source.status().contains("python-ytmusicapi"));
        qputenv("FAKE_YTM_MODE", "crash");
        e.source.sync();
        QTRY_COMPARE(e.source.state(), QString("offline"));
        qputenv("FAKE_YTM_MODE", "ok");
        e.source.sync();
        QTRY_COMPARE(e.source.state(), QString("connected"));

        Env none(dir.path() + "/x", dir.path() + "/no-such-helper");
        none.source.login("curl x");
        QTRY_COMPARE(none.source.state(), QString("missing"));
    }

    void loggingOutForgetsEverything()
    {
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.login("curl x");
        QTRY_COMPARE(e.source.state(), QString("connected"));
        e.source.logout();
        QCOMPARE(e.source.state(), QString("off"));
        QCOMPARE(e.library.remoteTracksOf("ytm").size(), 0);
        QVERIFY(!QFileInfo::exists(dir.path() + "/subsonic-ytm.jsonl"));
        QCOMPARE(e.playlists.list()->rowCount(), 0);
        Env again(dir.path(), dir.path() + "/omp-ytm");                        // the keyring is empty too
        again.source.start();
        QTRY_COMPARE(again.source.state(), QString("off"));
        QVERIFY(!QFileInfo::exists(dir.path() + "/seen") || QFile::remove(dir.path() + "/seen"));
    }

    void theBrowsersOfThisComputerAreListed()
    {
        Env e(dir.path(), dir.path() + "/omp-ytm");
        QSignalSpy spy(&e.source, &YtmSource::browsersChanged);
        e.source.findBrowsers();
        QTRY_COMPARE(spy.count(), 1);
        const QVariantList b = e.source.browsers();
        QCOMPARE(b.size(), 2);
        QCOMPARE(b.at(0).toMap().value("label").toString(), QString("Brave Origin"));
        QCOMPARE(b.at(0).toMap().value("spec").toString(), QString("browser:brave:/p/Default"));
    }

    void aBrowserLoginIsKeptOnceASyncHasWorkedAndTheHelperGetsTheSpecEveryTime()
    {
        {
            Env e(dir.path(), dir.path() + "/omp-ytm");
            e.source.loginWithBrowser("browser:brave:/p/Default");
            QTRY_COMPARE(e.source.state(), QString("connected"));
            QCOMPARE(e.library.remoteTracksOf("ytm").size(), 2);
        }
        QFile::remove(dir.path() + "/seen");
        Env again(dir.path(), dir.path() + "/omp-ytm");                        // the next run: the spec comes from the keyring
        again.source.start();
        QTRY_COMPARE(again.source.state(), QString("connected"));
        QFile seen(dir.path() + "/seen");
        QVERIFY(seen.open(QIODevice::ReadOnly));
        QVERIFY(seen.readAll().contains("browser:brave:/p/Default"));
        again.source.loginWithBrowser("not a spec");                           // only specs from the list are taken
        QCOMPARE(again.source.state(), QString("connected"));
    }

    void aBrowserThatIsNotSignedInIsNotKept()
    {
        qputenv("FAKE_YTM_MODE", "auth");
        {
            Env e(dir.path(), dir.path() + "/omp-ytm");
            e.source.loginWithBrowser("browser:brave:/p/Default");
            QTRY_COMPARE(e.source.state(), QString("off"));
            QCOMPARE(e.source.status(), QString("signed out"));                // the helper's own words
        }
        qputenv("FAKE_YTM_MODE", "ok");
        Env again(dir.path(), dir.path() + "/omp-ytm");                        // nothing was kept
        again.source.start();
        QTRY_COMPARE(again.source.state(), QString("off"));
    }

    void anExpiredBrowserLoginSaysWhatToDo()
    {
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.loginWithBrowser("browser:brave:/p/Default");
        QTRY_COMPARE(e.source.state(), QString("connected"));
        qputenv("FAKE_YTM_MODE", "auth");
        e.source.sync();
        QTRY_COMPARE(e.source.state(), QString("expired"));
        QVERIFY(e.source.status().contains("sign in"));
        QCOMPARE(e.library.remoteTracksOf("ytm").size(), 2);                   // the cached songs stay
    }

    void aSongPlaysFromItsWatchAddress()
    {
        QCOMPARE(YtmSource::streamUrl("ytm:lKVZKG7CcMU"), QString("https://music.youtube.com/watch?v=lKVZKG7CcMU"));
        QCOMPARE(YtmSource::streamUrl("/music/a.flac"), QString());
        QCOMPARE(YtmSource::streamUrl("subsonic:s1/4"), QString());
    }
};

QTEST_MAIN(TstYtm)
#include "tst_ytm.moc"
