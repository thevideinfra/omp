#include <QtTest>

#include "fakesecrettool.h"
#include "fakesubsonic.h"
#include "library.h"
#include "secrets.h"
#include "settings.h"
#include "player.h"
#include "testutil.h"
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
        QCOMPARE(q.queryItemValue("c"), QString("omp"));
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

    void pingReportsWhichServerSoftwareAnswered()
    {
        FakeSubsonic fake;
        fake.setResult("ping", R"J("type":"navidrome","serverVersion":"0.64.2 (10114574)","openSubsonic":true)J");
        SubsonicClient c;
        c.setServer(fake.url(), "amy", "sesame");
        QSignalSpy info(&c, &SubsonicClient::serverInfo);
        c.ping();
        QTRY_COMPARE(info.count(), 1);
        QCOMPARE(info.at(0).at(0).toString(), QString("navidrome"));
        QCOMPARE(info.at(0).at(1).toString(), QString("0.64.2 (10114574)"));

        fake.setResult("ping", {});                              // classic Subsonic says nothing about itself
        c.ping();
        QTRY_COMPARE(info.count(), 2);
        QCOMPARE(info.at(1).at(0).toString(), QString());
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
            "starred":"2022-01-01T00:00:00Z","displayAlbumArtist":"Amy & Co","year":1999})").object();
        const Track t = SubsonicClient::parseSong(o);
        QCOMPARE(t.path, QString("subsonic:s1/42"));
        QCOMPARE(SubsonicClient::parseSong(o, "s7").path, QString("subsonic:s7/42"));   // the server id goes into the path
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
        QCOMPARE(t.year, 1999);
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

    void fetchesThePlaylistsWithTheirSongs()
    {
        FakeSubsonic fake;
        fake.setResult("getPlaylists", R"("playlists":{"playlist":[{"id":"p1","name":"Road"},{"id":"p2","name":"Empty"}]})");
        fake.setHandler("getPlaylist", [](const QUrlQuery &q) {
            return q.queryItemValue("id") == "p1"
                       ? QByteArray(R"("playlist":{"id":"p1","entry":[{"id":"7","title":"A"},{"id":"9","title":"B"}]})")
                       : QByteArray(R"("playlist":{"id":"p2"})");
        });
        SubsonicClient c;
        c.setServer(fake.url(), "amy", "sesame");
        QSignalSpy done(&c, &SubsonicClient::playlistsFetched);
        c.fetchPlaylists();
        QTRY_COMPARE(done.count(), 1);
        const QVariantList lists = done.at(0).at(0).toList();
        QCOMPARE(lists.size(), 2);
        QCOMPARE(lists.at(0).toMap().value("name").toString(), QString("Road"));
        QCOMPARE(lists.at(0).toMap().value("paths").toStringList(), QStringList({"subsonic:s1/7", "subsonic:s1/9"}));
        QCOMPARE(lists.at(0).toMap().value("id").toString(), QString("p1"));
        QCOMPARE(lists.at(1).toMap().value("paths").toStringList(), QStringList());
    }

    void mpvStreamsASongFromTheServerUrl()
    {
        QTemporaryDir dir;
        FakeSubsonic fake;
        QFile wav(writeWav(dir.filePath("s.wav"), 4.0));
        QVERIFY(wav.open(QIODevice::ReadOnly));
        fake.setBinary("stream", wav.readAll(), "audio/wav");
        SubsonicClient client;
        client.setServer(fake.url(), "amy", "sesame");
        Player p("mpv", {"--ao=null"}, dir.filePath("mpv.sock"));
        p.setPathResolver([&](const QString &path) { return client.streamUrl(Track::songIdOf(path)).toString(QUrl::FullyEncoded); });
        p.start();
        QTRY_VERIFY_WITH_TIMEOUT(p.ready(), 5000);
        QVariantMap row{{"path", "subsonic:42"}, {"title", "Far"}, {"artist", "Amy"}, {"album", "Beta"},
                        {"duration", 4.0}, {"subtitle", "Amy — Beta"}, {"meta", "0:04"}, {"cover", ""}};
        p.playTracks({row}, 0);
        QTRY_VERIFY_WITH_TIMEOUT(p.position() > 0.3, 8000);
        QVERIFY(fake.count("stream") >= 1);
        QCOMPARE(QUrlQuery(fake.requests.last()).queryItemValue("id"), QString("42"));
    }

};

QTEST_MAIN(TstSubsonic)
#include "tst_subsonic.moc"
