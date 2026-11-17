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
        qunsetenv("FAKE_YTM_EDITS");
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
        QCOMPARE(e.playlists.list()->rowCount(), 2);
        QCOMPARE(e.playlists.list()->get(0).value("title").toString(), QString("Mix"));
        QCOMPARE(e.playlists.list()->get(0).value("meta").toString(), QString("YouTube Music"));
        QVERIFY(!e.playlists.list()->get(0).value("readOnly").toBool());                    // yours: it can be changed
        QVERIFY(e.playlists.list()->get(1).value("readOnly").toBool());                     // Liked Music is changed by liking
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

    void aSearchFillsSongsAndAlbumsAndTheSongsAreKnownByPath()
    {
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.login("curl x");
        QTRY_COMPARE(e.source.state(), QString("connected"));
        e.source.search("gamma ray");
        QVERIFY(e.source.searching());
        QTRY_VERIFY(!e.source.searching());
        QFile q(dir.path() + "/query");
        QVERIFY(q.open(QIODevice::ReadOnly));
        QCOMPARE(q.readAll().trimmed(), QByteArray("gamma ray"));              // the words reach the helper as one argument
        QCOMPARE(e.source.searchSongs()->rowCount(), 2);
        QCOMPARE(e.source.searchSongs()->get(0).value("title").toString(), QString("Found One"));
        QCOMPARE(e.source.searchSongs()->get(0).value("path").toString(), QString("ytm:s1"));
        QCOMPARE(e.source.searchAlbums()->rowCount(), 2);
        QCOMPARE(e.source.searchAlbums()->get(0).value("key").toString(), QString("MPREb_g"));
        QCOMPARE(e.source.searchAlbums()->get(0).value("subtitle").toString(), QString("Zed  ·  2019"));
        QCOMPARE(e.source.searchAlbums()->get(0).value("meta").toString(), QString("Album"));
        QCOMPARE(e.source.searchAlbums()->get(1).value("subtitle").toString(), QString("Yan"));   // no year known
        QCOMPARE(e.source.searchAlbums()->get(1).value("meta").toString(), QString("Single"));
        QCOMPARE(e.library.tracksForPaths({"ytm:s1"}).size(), 1);              // so a queue, a cover or a playlist can find it
        const QString cover = e.source.searchAlbums()->get(0).value("cover").toString();
        QVERIFY(!cover.isEmpty());
        QCOMPARE(e.library.trackInfo(cover).value("albumArtist").toString(), QString("Zed"));   // the album's picture resolves through it
        QCOMPARE(e.library.remoteTracksOf("ytm").size(), 2);                   // the library itself is untouched
    }

    void asksForFortyResultsAndOffersMoreOnlyWhenThereCouldBeMore()
    {
        const auto limit = [&] {
            QFile f(dir.path() + "/limit");
            return f.open(QIODevice::ReadOnly) ? f.readAll().trimmed() : QByteArray();
        };
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.login("curl x");
        QTRY_COMPARE(e.source.state(), QString("connected"));
        e.source.search("gamma");
        QTRY_VERIFY(!e.source.searching());
        QCOMPARE(limit(), QByteArray("40"));
        QVERIFY(!e.source.canShowMore());                                       // 2 songs found, far fewer than asked for
        e.source.search("gamma", 2);                                            // as many as asked for: there may be more
        QTRY_VERIFY(!e.source.searching());
        QCOMPARE(limit(), QByteArray("2"));
        QVERIFY(e.source.canShowMore());
        e.source.showMore();                                                    // the same words, up to 120
        QTRY_VERIFY(!e.source.searching());
        QCOMPARE(limit(), QByteArray("120"));
        QVERIFY(!e.source.canShowMore());                                       // nothing beyond that
    }

    void aFailedOrEmptySearchSaysSoAndAnExpiredLoginIsNoticed()
    {
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.search("anything");                                           // not logged in
        QCOMPARE(e.source.searchStatus(), QString("Log in first"));
        e.source.login("curl x");
        QTRY_COMPARE(e.source.state(), QString("connected"));
        qputenv("FAKE_YTM_MODE", "crash");
        e.source.search("anything");
        QTRY_VERIFY(!e.source.searching());
        QVERIFY(e.source.searchStatus().contains("boom"));
        qputenv("FAKE_YTM_MODE", "auth");
        e.source.search("anything");
        QTRY_COMPARE(e.source.state(), QString("expired"));
        QVERIFY(!e.source.searching());
        e.source.search("   ");                                                 // nothing to look for: nothing is asked
        QVERIFY(!e.source.searching());
    }

    void anAlbumIsFetchedForAPurposeAndItsSongsAreKnownByPath()
    {
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.login("curl x");
        QTRY_COMPARE(e.source.state(), QString("connected"));
        QSignalSpy spy(&e.source, &YtmSource::albumFetched);
        e.source.fetchAlbum("MPREb_g", "play");
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("MPREb_g"));
        QCOMPARE(spy.at(0).at(1).toString(), QString("play"));
        const QVariantList tracks = spy.at(0).at(2).toList();
        QCOMPARE(tracks.size(), 2);
        QCOMPARE(tracks.at(1).toMap().value("title").toString(), QString("Gamma Two"));
        QCOMPARE(e.library.tracksForPaths({"ytm:g1"}).size(), 1);
        QFile id(dir.path() + "/albumid");
        QVERIFY(id.open(QIODevice::ReadOnly));
        QCOMPARE(id.readAll().trimmed(), QByteArray("MPREb_g"));
        qputenv("FAKE_YTM_MODE", "crash");
        e.source.fetchAlbum("MPREb_x", "queue");
        QTRY_COMPARE(spy.count(), 2);
        QVERIFY(spy.at(1).at(2).toList().isEmpty());                            // a failure answers with no songs
        QVERIFY(e.source.message().contains("Cannot read the album"));          // and says so, so a click is never silent
    }

    // ----- editing -----

    static QStringList edits(const QString &dir)
    {
        QFile f(dir + "/edits");
        return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts) : QStringList();
    }
    static QString titleOf(Env &e, int row) { return e.playlists.list()->get(row).value("title").toString(); }
    static QStringList pathsOf(Env &e, int row) { return e.playlists.paths(e.playlists.list()->get(row).value("key").toString()); }

    void yourPlaylistsAreEditableAndOnlyThoseAreOfferedToAddTo()
    {
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.login("curl x");
        QTRY_COMPARE(e.source.state(), QString("connected"));
        const QVariantList editable = e.playlists.editableServerLists();
        QCOMPARE(editable.size(), 1);
        QCOMPARE(editable.at(0).toMap().value("name").toString(), QString("Mix"));
        QCOMPARE(editable.at(0).toMap().value("serverId").toString(), QString("ytm"));
        QCOMPARE(editable.at(0).toMap().value("playlistId").toString(), QString("PL1"));
        QVERIFY(e.source.loggedIn());
    }

    void aNewPlaylistTakesOnlyYoutubeMusicSongsAndShowsUpWhenYouTubeHasMadeIt()
    {
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.login("curl x");
        QTRY_COMPARE(e.source.state(), QString("connected"));
        e.source.createPlaylist("ytm", "Road trip", {"ytm:aaa", "/m/local.flac", "ytm:album:MPREb_x", "subsonic:s1/4"});
        QTRY_COMPARE(e.playlists.list()->rowCount(), 3);
        QCOMPARE(edits(dir.path()), QStringList{"playlist-create Road trip aaa"});          // the other songs never leave the computer
        QCOMPARE(titleOf(e, 2), QString("Road trip"));
        QCOMPARE(pathsOf(e, 2), QStringList{"ytm:aaa"});
        QVERIFY(!e.playlists.list()->get(2).value("readOnly").toBool());
        QCOMPARE(e.playlists.list()->get(2).value("playlistId").toString(), QString("PLnew"));
        // its songs can be taken out again: the id YouTube gave each one was kept
        e.source.removeFromPlaylist("ytm", "PLnew", 0);
        QCOMPARE(pathsOf(e, 2), QStringList());                                                // at once
        QTRY_COMPARE(edits(dir.path()).size(), 2);
        QCOMPARE(edits(dir.path()).at(1), QString("playlist-remove PLnew aaa/set-aaa"));
    }

    void songsAddedToAPlaylistKeepTheirIdsSoTheyCanBeRemoved()
    {
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.login("curl x");
        QTRY_COMPARE(e.source.state(), QString("connected"));
        e.source.addToPlaylist("ytm", "PL1", {"/m/local.flac", "ytm:album:Z"});             // nothing of YouTube Music's: nothing asked
        QTest::qWait(150);
        QVERIFY(edits(dir.path()).isEmpty());
        e.source.addToPlaylist("ytm", "PL1", {"ytm:aaa"});
        QTRY_COMPARE(pathsOf(e, 0).size(), 3);                                                // Mix had two songs
        QCOMPARE(edits(dir.path()), QStringList{"playlist-add PL1 aaa"});
        QCOMPARE(pathsOf(e, 0).last(), QString("ytm:aaa"));
        e.source.removeFromPlaylist("ytm", "PL1", 2);
        QTRY_COMPARE(edits(dir.path()).size(), 2);
        QCOMPARE(edits(dir.path()).at(1), QString("playlist-remove PL1 aaa/set-aaa"));
        e.source.removeFromPlaylist("ytm", "PL1", 0);                                         // a song of the first sync
        QTRY_COMPARE(edits(dir.path()).size(), 3);
        QCOMPARE(edits(dir.path()).at(2), QString("playlist-remove PL1 bbb/sv-b"));
        QCOMPARE(pathsOf(e, 0), QStringList{"ytm:aaa"});
    }

    void aPlaylistIsRenamedAndDeletedAtOnceAndTheWordsReachYouTube()
    {
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.login("curl x");
        QTRY_COMPARE(e.source.state(), QString("connected"));
        e.source.renamePlaylist("ytm", "PL1", "Better name");
        QCOMPARE(titleOf(e, 0), QString("Better name"));
        QTRY_COMPARE(edits(dir.path()), QStringList{"playlist-rename PL1 Better name"});
        e.source.deletePlaylist("ytm", "PL1");
        QCOMPARE(e.playlists.list()->rowCount(), 1);                                           // only Liked Music is left
        QTRY_COMPARE(edits(dir.path()).size(), 2);
        QCOMPARE(edits(dir.path()).at(1), QString("playlist-delete PL1"));
    }

    void aRefusedEditIsReportedAndTheListsComeBackFromYouTube()
    {
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.login("curl x");
        QTRY_COMPARE(e.source.state(), QString("connected"));
        qputenv("FAKE_YTM_EDITS", "fail");
        e.source.deletePlaylist("ytm", "PL1");
        QCOMPARE(e.playlists.list()->rowCount(), 1);                                           // optimistic, as it is shown at once
        QTRY_VERIFY(e.source.message().contains("refused"));
        QTRY_COMPARE(e.playlists.list()->rowCount(), 2);                                       // the sync that follows a failure restores it
        QCOMPARE(titleOf(e, 0), QString("Mix"));
        qunsetenv("FAKE_YTM_EDITS");
    }

    void editsDoNothingWithoutALogin()
    {
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.createPlaylist("ytm", "X", {"ytm:aaa"});
        e.source.addToPlaylist("ytm", "PL1", {"ytm:aaa"});
        e.source.toggleLike("ytm:aaa");
        QCOMPARE(e.source.message(), QString("Log in first"));
        QTest::qWait(150);
        QVERIFY(edits(dir.path()).isEmpty());
    }

    void likingASongIsShownAtOnceAndUndoneWhenYouTubeRefuses()
    {
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.login("curl x");
        QTRY_COMPARE(e.source.state(), QString("connected"));
        QVERIFY(!e.library.isStarred("ytm:bbb"));
        e.source.toggleLike("ytm:bbb");
        QVERIFY(e.library.isStarred("ytm:bbb"));                                               // the heart at once
        QTRY_COMPARE(edits(dir.path()), QStringList{"like bbb"});
        e.source.toggleLike("ytm:bbb");
        QVERIFY(!e.library.isStarred("ytm:bbb"));
        QTRY_COMPARE(edits(dir.path()).size(), 2);
        QCOMPARE(edits(dir.path()).at(1), QString("unlike bbb"));
        qputenv("FAKE_YTM_EDITS", "fail");
        e.source.toggleLike("ytm:bbb");
        QTRY_VERIFY(e.source.message().contains("refused"));
        QVERIFY(!e.library.isStarred("ytm:bbb"));                                              // put back
        qunsetenv("FAKE_YTM_EDITS");
        e.source.toggleLike("/m/local.flac");                                                  // not a YouTube Music song: ignored
        e.source.toggleLike("ytm:album:MPREb_x");
        QTest::qWait(150);
        QCOMPARE(edits(dir.path()).size(), 2);
    }

    void anAlbumIsAddedToTheLibrary()
    {
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.login("curl x");
        QTRY_COMPARE(e.source.state(), QString("connected"));
        e.source.likeAlbum("MPREb_g");
        QTRY_COMPARE(edits(dir.path()), QStringList{"like-album MPREb_g"});
        QTRY_VERIFY(e.source.message().contains("library"));
    }

    void whatWasPlayedLatelyIsFetchedOnceInAWhileAndTheSongsAreKnownByPath()
    {
        const auto calls = [&] {
            QFile f(dir.path() + "/history-calls");
            return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()).count('\n') : 0;
        };
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.refreshHistory();                                                             // no login: nothing happens
        QTest::qWait(100);
        QCOMPARE(calls(), 0);
        e.source.login("curl x");
        QTRY_COMPARE(e.source.state(), QString("connected"));
        QTRY_COMPARE(e.source.history()->rowCount(), 2);                                       // fetched after the first sync
        QCOMPARE(calls(), 1);
        QCOMPARE(e.source.history()->get(0).value("title").toString(), QString("Heard Today"));
        QCOMPARE(e.source.history()->get(0).value("path").toString(), QString("ytm:h1"));
        QCOMPARE(e.library.tracksForPaths({"ytm:h1"}).size(), 1);                              // its cover and queue entry resolve
        QCOMPARE(e.library.remoteTracksOf("ytm").size(), 2);                                   // the library is untouched
        e.source.refreshHistory();                                                             // fresh: not asked again
        QTest::qWait(150);
        QCOMPARE(calls(), 1);
        e.source.refreshHistory(true);                                                         // unless forced
        QTRY_COMPARE(calls(), 2);
        qputenv("FAKE_YTM_MODE", "crash");
        e.source.refreshHistory(true);
        QTest::qWait(250);
        QCOMPARE(e.source.history()->rowCount(), 2);                                           // a failure keeps what was there
    }

    void anAlbumAlreadyReadIsAnsweredAtOnceFromMemory()
    {
        const auto calls = [&] {
            QFile f(dir.path() + "/album-calls");
            return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()).count('\n') : 0;
        };
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.login("curl x");
        QTRY_COMPARE(e.source.state(), QString("connected"));
        QSignalSpy spy(&e.source, &YtmSource::albumFetched);
        e.source.fetchAlbum("MPREb_g", "open");
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(calls(), 1);
        e.source.fetchAlbum("MPREb_g", "play");
        QCOMPARE(spy.count(), 2);                                                  // answered before the call returns
        QCOMPARE(spy.at(1).at(1).toString(), QString("play"));
        QCOMPARE(spy.at(1).at(2).toList().size(), 2);
        QCOMPARE(calls(), 1);                                                      // YouTube was not asked again
        e.source.logout();
        e.source.login("curl x");
        QTRY_COMPARE(e.source.state(), QString("connected"));
        e.source.fetchAlbum("MPREb_g", "open");
        QTRY_COMPARE(spy.count(), 3);
        QCOMPARE(calls(), 2);                                                      // a new login starts with an empty memory
    }

    void anArtistPageArrivesAsTopSongsAlbumsAndSingles()
    {
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.login("curl x");
        QTRY_COMPARE(e.source.state(), QString("connected"));
        QSignalSpy spy(&e.source, &YtmSource::artistFetched);
        e.source.fetchArtist("UCzed");
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("UCzed"));
        const QVariantMap page = spy.at(0).at(1).toMap();
        QCOMPARE(page.value("name").toString(), QString("Zed"));
        QCOMPARE(page.value("description").toString(), QString("Plays things."));
        QCOMPARE(page.value("tracks").toList().size(), 2);
        QCOMPARE(page.value("tracks").toList().at(0).toMap().value("artistId").toString(), QString("UCzed"));
        const QVariantList albums = page.value("albums").toList(), singles = page.value("singles").toList();
        QCOMPARE(albums.size(), 1);
        QCOMPARE(singles.size(), 1);
        QCOMPARE(albums.at(0).toMap().value("key").toString(), QString("MPREb_g"));
        QCOMPARE(albums.at(0).toMap().value("subtitle").toString(), QString("2019"));
        QCOMPARE(singles.at(0).toMap().value("meta").toString(), QString("Single"));
        QVERIFY(!albums.at(0).toMap().value("cover").toString().isEmpty());
        QCOMPARE(e.library.trackInfo(albums.at(0).toMap().value("cover").toString()).value("albumArtist").toString(), QString("Zed"));   // its picture resolves
        QCOMPARE(e.library.tracksForPaths({"ytm:t1"}).size(), 1);
        QFile f(dir.path() + "/artistid");
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll().trimmed(), QByteArray("UCzed"));
        qputenv("FAKE_YTM_MODE", "crash");
        e.source.fetchArtist("UCzed");
        QTRY_COMPARE(spy.count(), 2);
        QVERIFY(spy.at(1).at(1).toMap().isEmpty());                                        // a failure answers with nothing
    }

    void aRadioStartsFromASongAndComesBackForAPurpose()
    {
        Env e(dir.path(), dir.path() + "/omp-ytm");
        e.source.login("curl x");
        QTRY_COMPARE(e.source.state(), QString("connected"));
        QSignalSpy spy(&e.source, &YtmSource::radioFetched);
        e.source.startRadio("ytm:seed9");
        QTRY_COMPARE(spy.count(), 1);
        const QVariantList tracks = spy.at(0).at(0).toList();
        QCOMPARE(tracks.size(), 2);
        QCOMPARE(tracks.at(0).toMap().value("path").toString(), QString("ytm:seed9"));      // the song itself starts it
        QFile f(dir.path() + "/radio");
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll().trimmed(), QByteArray("seed9 50"));
        QCOMPARE(e.library.tracksForPaths({"ytm:r1"}).size(), 1);
        QVERIFY(e.source.message().contains("Radio: 2 songs"));
        e.source.startRadio("/m/local.flac");                                                // not a YouTube Music song: nothing
        e.source.startRadio("ytm:album:MPREb_x");
        QTest::qWait(150);
        QCOMPARE(spy.count(), 1);
        qputenv("FAKE_YTM_MODE", "crash");
        e.source.startRadio("ytm:seed9");
        QTRY_VERIFY(e.source.message().contains("boom"));
        QCOMPARE(spy.count(), 1);                                                            // a failure plays nothing
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
