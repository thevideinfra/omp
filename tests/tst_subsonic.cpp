#include <QtTest>

#include "fakesecrettool.h"
#include "fakesubsonic.h"
#include "library.h"
#include "secrets.h"
#include "settings.h"
#include "subsonicsource.h"
#include "subsonicclient.h"
#include "track.h"

Q_DECLARE_METATYPE(QVector<Track>)

class TstSubsonic : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() { qRegisterMetaType<QVector<Track>>(); }

    void tokenIsTheMd5OfPasswordAndSalt()
    {
        // The example from the Subsonic API documentation.
        QCOMPARE(SubsonicClient::token("sesame", "c19b2d"), QString("26719a1196d2a940705a59634eb18eab"));
    }

    void endpointCarriesTheLoginAndNeverThePassword()
    {
        SubsonicClient c;
        c.setServer("https://music.example.com/", "amy", "sesame");
        const QUrl u = c.endpoint("getAlbum", {{"id", "al-1"}});
        QCOMPARE(u.path(), QString("/rest/getAlbum"));
        const QUrlQuery q(u);
        QCOMPARE(q.queryItemValue("u"), QString("amy"));
        QCOMPARE(q.queryItemValue("v"), QString("1.16.1"));
        QCOMPARE(q.queryItemValue("c"), QString("omamusic"));
        QCOMPARE(q.queryItemValue("f"), QString("json"));
        QCOMPARE(q.queryItemValue("id"), QString("al-1"));
        QCOMPARE(q.queryItemValue("t"), SubsonicClient::token("sesame", q.queryItemValue("s")));
        QVERIFY(!u.toString().contains("sesame"));
        QVERIFY(q.queryItemValue("s").size() >= 6);

        const QUrl s = c.streamUrl("42");
        QCOMPARE(s.path(), QString("/rest/stream"));
        QCOMPARE(QUrlQuery(s).queryItemValue("id"), QString("42"));
        const QUrl cv = c.coverUrl("al-1", 600);
        QCOMPARE(cv.path(), QString("/rest/getCoverArt"));
        QCOMPARE(QUrlQuery(cv).queryItemValue("size"), QString("600"));
    }

    void pingAnswersOkAndReportsAWrongPassword()
    {
        FakeSubsonic fake;
        fake.setResult("ping", {});
        SubsonicClient c;
        c.setServer(fake.url(), "amy", "sesame");
        QSignalSpy spy(&c, &SubsonicClient::pingDone);
        c.ping();
        QTRY_COMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toBool());

        c.setServer(fake.url(), "amy", "wrong");
        c.ping();
        QTRY_COMPARE(spy.count(), 2);
        QVERIFY(!spy.at(1).at(0).toBool());
        QVERIFY(spy.at(1).at(1).toString().contains("Wrong username or password"));
    }

    void anUnreachableOrSilentServerFailsInsteadOfHanging()
    {
        SubsonicClient c;
        c.setTimeoutMs(500);
        QSignalSpy spy(&c, &SubsonicClient::pingDone);
        c.setServer("http://127.0.0.1:1", "amy", "x");   // nothing listens there
        c.ping();
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
        QVERIFY(!spy.at(0).at(0).toBool());

        FakeSubsonic fake;
        fake.ignore("ping");
        c.setServer(fake.url(), "amy", "sesame");
        c.ping();
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 5000);
        QVERIFY(!spy.at(1).at(0).toBool());
    }

    void parsesASongAndFillsGaps()
    {
        const QJsonObject o = QJsonDocument::fromJson(R"({
            "id":"42","title":"Far","artist":"Amy","album":"Beta","track":3,"discNumber":2,
            "duration":201,"size":5000000,"created":"2021-03-04T05:06:07Z","coverArt":"al-1",
            "starred":"2022-01-01T00:00:00Z","displayAlbumArtist":"Amy & Co"})").object();
        const Track t = SubsonicClient::parseSong(o);
        QCOMPARE(t.path, QString("subsonic:42"));
        QCOMPARE(t.title, QString("Far"));
        QCOMPARE(t.artist, QString("Amy"));
        QCOMPARE(t.albumArtist, QString("Amy & Co"));
        QCOMPARE(t.album, QString("Beta"));
        QCOMPARE(t.track, 3);
        QCOMPARE(t.disc, 2);
        QCOMPARE(t.duration, 201.0);
        QCOMPARE(t.size, qint64(5000000));
        QCOMPARE(t.mtime, QDateTime(QDate(2021, 3, 4), QTime(5, 6, 7), QTimeZone::UTC).toSecsSinceEpoch());
        QCOMPARE(t.coverId, QString("al-1"));
        QVERIFY(t.starred);
        QVERIFY(t.isRemote());

        const Track bare = SubsonicClient::parseSong(QJsonDocument::fromJson(R"({"id":"7","title":"X"})").object());
        QCOMPARE(bare.artist, QString("Unknown Artist"));
        QCOMPARE(bare.albumArtist, QString("Unknown Artist"));
        QCOMPARE(bare.album, QString("Unknown Album"));
        QVERIFY(!bare.starred);
        QCOMPARE(bare.mtime, qint64(0));
    }

    void fetchesTheWholeLibraryAlbumByAlbum()
    {
        FakeSubsonic fake;
        // 3 albums (paged two at a time), 5 songs.
        const QStringList albums{"a1", "a2", "a3"};
        fake.setHandler("getAlbumList2", [&](const QUrlQuery &q) {
            const int off = q.queryItemValue("offset").toInt(), size = q.queryItemValue("size").toInt();
            QStringList items;
            for (int i = off; i < qMin(int(albums.size()), off + size); ++i)
                items << QStringLiteral("{\"id\":\"%1\"}").arg(albums.at(i));
            return QByteArray("\"albumList2\":{\"album\":[") + items.join(',').toUtf8() + "]}";
        });
        fake.setHandler("getAlbum", [&](const QUrlQuery &q) {
            const QString id = q.queryItemValue("id");
            const int n = id == "a1" ? 2 : id == "a2" ? 2 : 1;
            QStringList songs;
            for (int i = 1; i <= n; ++i)
                songs << QStringLiteral("{\"id\":\"%1-%2\",\"title\":\"T%2\",\"artist\":\"Amy\",\"album\":\"%1\",\"track\":%2}").arg(id).arg(i);
            return QByteArray("\"album\":{\"id\":\"") + id.toUtf8() + "\",\"song\":[" + songs.join(',').toUtf8() + "]}";
        });
        SubsonicClient c;
        c.setServer(fake.url(), "amy", "sesame");
        c.setPageSize(2);
        QSignalSpy done(&c, &SubsonicClient::libraryFetched);
        QSignalSpy progress(&c, &SubsonicClient::libraryProgress);
        c.fetchLibrary();
        QTRY_COMPARE(done.count(), 1);
        const auto tracks = done.at(0).at(0).value<QVector<Track>>();
        QCOMPARE(tracks.size(), 5);
        QCOMPARE(fake.count("getAlbumList2"), 2);   // pages of two: 2 albums, then 1
        QCOMPARE(fake.count("getAlbum"), 3);
        QVERIFY(progress.count() >= 1);
        QCOMPARE(progress.last().at(0).toInt(), 3);
        QCOMPARE(progress.last().at(1).toInt(), 3);
    }

    void aFailingAlbumGivesNoPartialLibrary()
    {
        FakeSubsonic fake;
        fake.setResult("getAlbumList2", R"("albumList2":{"album":[{"id":"a1"},{"id":"a2"}]})");
        fake.setHandler("getAlbum", [](const QUrlQuery &q) {
            return q.queryItemValue("id") == "a2" ? QByteArray("ERR:Album gone")
                                                  : QByteArray(R"("album":{"song":[{"id":"1","title":"T"}]})");
        });
        SubsonicClient c;
        c.setServer(fake.url(), "amy", "sesame");
        QSignalSpy done(&c, &SubsonicClient::libraryFetched);
        QSignalSpy failed(&c, &SubsonicClient::libraryFailed);
        c.fetchLibrary();
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(failed.at(0).at(0).toString().contains("Album gone"));
        QCOMPARE(done.count(), 0);
    }

    void theSourceSyncsTheServerIntoTheLibraryAndKeepsThePasswordOutOfTheConfig()
    {
        QTemporaryDir dir;
        qputenv("FAKE_SECRET_DIR", dir.path().toUtf8());
        FakeSubsonic fake;
        fake.serveAlbums({{"a1", 2}, {"a2", 3}});
        Settings settings(dir.filePath("config.toml"));
        Library library({"/m"}, "/nonexistent-scanner", dir.filePath("index.jsonl"));
        Secrets secrets(writeFake(dir.path()));
        SubsonicClient client;
        SubsonicSource source(settings, library, secrets, client);

        source.saveServer(fake.url(), "amy", "sesame", true);
        QTRY_COMPARE(library.songs()->rowCount(), 5);
        QVERIFY(source.status().startsWith("Connected"));
        QVERIFY(QFileInfo::exists(dir.filePath("subsonic.jsonl")));
        QVERIFY(settings.serverEnabled());
        settings.save();
        QFile cfg(dir.filePath("config.toml"));
        QVERIFY(cfg.open(QIODevice::ReadOnly));
        const QByteArray text = cfg.readAll();
        QVERIFY(text.contains("server_user"));
        QVERIFY(!text.contains("sesame"));                       // the keyring has it, not the file

        // A new run reads the cache first and the password from the keyring.
        Library again({"/m"}, "/nonexistent-scanner", dir.filePath("index.jsonl"));
        SubsonicClient client2;
        SubsonicSource source2(settings, again, secrets, client2);
        source2.start();
        QCOMPARE(again.songs()->rowCount(), 5);                  // at once, from the cache
        QTRY_VERIFY(source2.status().startsWith("Connected"));
    }

    void aDeadServerKeepsTheCachedSongs()
    {
        QTemporaryDir dir;
        qputenv("FAKE_SECRET_DIR", dir.path().toUtf8());
        Settings settings(dir.filePath("config.toml"));
        Library library({"/m"}, "/nonexistent-scanner", dir.filePath("index.jsonl"));
        Track t;
        t.path = "subsonic:9"; t.title = "Kept"; t.artist = "Amy"; t.album = "Beta"; t.albumArtist = "Amy";
        library.setRemoteTracks({t});
        library.writeRemoteCache();
        Secrets secrets(writeFake(dir.path()));
        QSignalSpy stored(&secrets, &Secrets::stored);
        secrets.store("amy@http://127.0.0.1:1", "sesame");
        QTRY_COMPARE(stored.count(), 1);
        settings.set("serverUrl", "http://127.0.0.1:1");
        settings.set("serverUser", "amy");
        settings.set("serverEnabled", true);

        Library fresh({"/m"}, "/nonexistent-scanner", dir.filePath("index.jsonl"));
        SubsonicClient client;
        client.setTimeoutMs(500);
        SubsonicSource source(settings, fresh, secrets, client);
        source.start();
        QCOMPARE(fresh.songs()->rowCount(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(source.status().startsWith("Server offline"), 5000);
        QCOMPARE(fresh.songs()->rowCount(), 1);                  // still there
        QVERIFY(!source.syncing());
    }

    void removingTheServerForgetsEverything()
    {
        QTemporaryDir dir;
        qputenv("FAKE_SECRET_DIR", dir.path().toUtf8());
        FakeSubsonic fake;
        fake.serveAlbums({{"a1", 2}});
        Settings settings(dir.filePath("config.toml"));
        Library library({"/m"}, "/nonexistent-scanner", dir.filePath("index.jsonl"));
        Secrets secrets(writeFake(dir.path()));
        SubsonicClient client;
        SubsonicSource source(settings, library, secrets, client);
        source.saveServer(fake.url(), "amy", "sesame", true);
        QTRY_COMPARE(library.songs()->rowCount(), 2);

        QSignalSpy cleared(&secrets, &Secrets::cleared);
        source.remove();
        QCOMPARE(library.songs()->rowCount(), 0);
        QVERIFY(!settings.serverEnabled());
        QCOMPARE(settings.serverUrl(), QString());
        QCOMPARE(source.status(), QString());
        Library reloaded({"/m"}, "/nonexistent-scanner", dir.filePath("index.jsonl"));
        reloaded.loadRemoteCache();
        QCOMPARE(reloaded.songs()->rowCount(), 0);                // the cache is gone too
        QTRY_COMPARE(cleared.count(), 1);                         // the keyring call is asynchronous
        QSignalSpy looked(&secrets, &Secrets::looked);
        secrets.lookup(QString("amy@") + fake.url());
        QTRY_COMPARE(looked.count(), 1);
        QVERIFY(!looked.at(0).at(1).toBool());                    // and the keyring entry
    }
};

QTEST_MAIN(TstSubsonic)
#include "tst_subsonic.moc"
