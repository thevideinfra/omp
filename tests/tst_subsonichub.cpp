#include <QtTest>

#include "fakesecrettool.h"
#include "fakesubsonic.h"
#include "library.h"
#include "playlists.h"
#include "secrets.h"
#include "servers.h"
#include "settings.h"
#include "subsonichub.h"
#include "subsonicsource.h"

// Everything a hub needs, in a temporary folder with a stand-in keyring.
struct Env
{
    QTemporaryDir dir;
    Settings settings;
    Servers servers;
    Library library;
    Playlists playlists;
    Secrets secrets;
    SubsonicHub hub;

    Env()
        : settings(dir.filePath("config.toml"))
        , servers(dir.filePath("servers.json"))
        , library({"/m"}, "/nonexistent-scanner", dir.filePath("index.jsonl"))
        , playlists(dir.filePath("pl"))
        , secrets((qputenv("FAKE_SECRET_DIR", dir.path().toUtf8()), writeFake(dir.path())))
        , hub(servers, library, playlists, secrets)
    {
        hub.start();
    }
    QString state(const QString &id) const { return hub.source(id)->state(); }
    QString status(const QString &id) const { return hub.source(id)->status(); }
};

// A server that really keeps playlists, so that edits can be seen to take effect.
struct PlaylistBackend
{
    struct List { QString name; QStringList songs; };
    QMap<QString, List> lists;
    int next = 1;
    void attach(FakeSubsonic &fake)
    {
        fake.setHandler("getPlaylists", [this](const QUrlQuery &) {
            QStringList items;
            for (auto it = lists.cbegin(); it != lists.cend(); ++it)
                items << QStringLiteral("{\"id\":\"%1\",\"name\":\"%2\"}").arg(it.key(), it.value().name);
            return QByteArray("\"playlists\":{\"playlist\":[") + items.join(',').toUtf8() + "]}";
        });
        fake.setHandler("getPlaylist", [this](const QUrlQuery &q) {
            const List l = lists.value(q.queryItemValue("id"));
            QStringList entries;
            for (const QString &s : l.songs)
                entries << QStringLiteral("{\"id\":\"%1\",\"title\":\"T\"}").arg(s);
            return QByteArray("\"playlist\":{\"id\":\"") + q.queryItemValue("id").toUtf8() + "\",\"entry\":["
                   + entries.join(',').toUtf8() + "]}";
        });
        fake.setHandler("createPlaylist", [this](const QUrlQuery &q) {
            const QString id = QStringLiteral("p%1").arg(next++);
            lists[id] = {q.queryItemValue("name"), q.allQueryItemValues("songId")};
            return QByteArray("\"playlist\":{\"id\":\"") + id.toUtf8() + "\"}";
        });
        fake.setHandler("updatePlaylist", [this](const QUrlQuery &q) {
            const QString id = q.queryItemValue("playlistId");
            if (!lists.contains(id))
                return QByteArray("ERR:No such playlist");
            List &l = lists[id];
            if (q.hasQueryItem("name"))
                l.name = q.queryItemValue("name");
            QList<int> drop;
            for (const QString &i : q.allQueryItemValues("songIndexToRemove"))
                drop << i.toInt();
            std::sort(drop.begin(), drop.end(), std::greater<int>());
            for (int i : drop)
                if (i >= 0 && i < l.songs.size())
                    l.songs.removeAt(i);
            l.songs += q.allQueryItemValues("songIdToAdd");
            return QByteArray();
        });
        fake.setHandler("deletePlaylist", [this](const QUrlQuery &q) {
            return lists.remove(q.queryItemValue("id")) ? QByteArray() : QByteArray("ERR:No such playlist");
        });
    }
};


class TstSubsonicHub : public QObject
{
    Q_OBJECT
private slots:
    void addingAServerSyncsItAndKeepsThePasswordOutOfTheFiles()
    {
        FakeSubsonic fake;
        fake.serveAlbums({{"a1", 2}, {"a2", 3}});
        fake.setResult("ping", R"("type":"navidrome","serverVersion":"0.64.2")");
        Env e;
        QCOMPARE(e.hub.servers()->rowCount(), 0);
        const QString id = e.hub.addServer(fake.url(), "amy", "sesame", true);
        QCOMPARE(id, QString("s1"));
        QTRY_COMPARE(e.library.songs()->rowCount(), 5);
        QVERIFY(e.status(id).startsWith("Connected"));
        QCOMPARE(e.state(id), QString("connected"));
        QTRY_COMPARE(e.servers.get(id).type, QString("navidrome"));   // remembered for when the server is down
        QVERIFY(QFileInfo::exists(e.dir.filePath("subsonic-s1.jsonl")));
        QCOMPARE(e.hub.servers()->get(0).value("title").toString(), QString("Navidrome"));
        QCOMPARE(e.hub.servers()->get(0).value("state").toString(), QString("connected"));

        QFile f(e.dir.filePath("servers.json"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(!f.readAll().contains("sesame"));                    // the keyring has it, not the file

        // A new run shows the cached songs at once and signs in with the keyring's password.
        Library again({"/m"}, "/nonexistent-scanner", e.dir.filePath("index.jsonl"));
        Playlists pl2(e.dir.filePath("pl2"));
        SubsonicHub hub2(e.servers, again, pl2, e.secrets);
        hub2.start();
        QCOMPARE(again.songs()->rowCount(), 5);
        QTRY_VERIFY(hub2.source(id)->status().startsWith("Connected"));
    }

    void twoServersMergeStayApartAndKeepTheirOwnThings()
    {
        FakeSubsonic one, two;
        one.serveAlbums({{"a1", 2}});
        two.serveAlbums({{"a1", 3}});                              // the same album id on another server
        one.setResult("scrobble", {});
        two.setResult("scrobble", {});
        one.setLogin("amy", "pw-one");
        two.setLogin("bob", "pw-two");
        Env e;
        const QString s1 = e.hub.addServer(one.url(), "amy", "pw-one", true);
        const QString s2 = e.hub.addServer(two.url(), "bob", "pw-two", true);
        QVERIFY(s1 != s2);
        QTRY_COMPARE(e.library.songs()->rowCount(), 5);            // 2 + 3, merged
        QVERIFY(e.library.tracksForPaths({"subsonic:" + s1 + "/a1-1", "subsonic:" + s2 + "/a1-1"}).size() == 2);
        QCOMPARE(e.hub.servers()->rowCount(), 2);

        // Each one's passwords and stream addresses are its own.
        QVERIFY(e.hub.streamUrl("subsonic:" + s1 + "/a1-1").startsWith(one.url()));
        QVERIFY(e.hub.streamUrl("subsonic:" + s2 + "/a1-1").startsWith(two.url()));
        QVERIFY(e.hub.streamUrl("/local/a.flac").isEmpty());
        QVERIFY(e.hub.streamUrl("subsonic:s9/1").isEmpty());

        // Scrobbles go to the server the song belongs to.
        e.hub.source(s1)->noteProgress("subsonic:" + s2 + "/a1-1", 0.1, 200);   // not this server's song
        e.hub.source(s2)->noteProgress("subsonic:" + s2 + "/a1-1", 0.1, 200);
        QTRY_COMPARE(two.count("scrobble"), 1);
        QCOMPARE(one.count("scrobble"), 0);

        // Each server's playlists and favorites are kept apart too.
        QTRY_VERIFY(e.playlists.list()->rowCount() >= 2);
        QStringList ids;
        for (int i = 0; i < e.playlists.list()->rowCount(); ++i)
            ids << e.playlists.list()->get(i).value("serverId").toString();
        QVERIFY(ids.contains(s1) && ids.contains(s2));

        e.hub.removeServer(s1);                                     // removing one keeps the other
        QCOMPARE(e.library.songs()->rowCount(), 3);
        QCOMPARE(e.hub.servers()->rowCount(), 1);
        QVERIFY(!e.hub.source(s1));
        QVERIFY(e.hub.source(s2));
        QVERIFY(!QFileInfo::exists(e.dir.filePath("subsonic-" + s1 + ".jsonl")));
        QVERIFY(QFileInfo::exists(e.dir.filePath("subsonic-" + s2 + ".jsonl")));
    }

    void changingTheAddressKeepsTheSavedPassword()
    {
        FakeSubsonic old, moved;
        old.serveAlbums({{"a1", 1}});
        moved.serveAlbums({{"b1", 4}});
        Env e;
        const QString id = e.hub.addServer(old.url(), "amy", "sesame", true);
        QTRY_COMPARE(e.library.songs()->rowCount(), 1);

        QVERIFY(e.hub.updateServer(id, moved.url(), "amy", QString(), true));   // no new password
        QTRY_COMPARE(e.library.songs()->rowCount(), 4);                          // it signed in to the new address
        QVERIFY(e.status(id).startsWith("Connected"));
        QCOMPARE(e.servers.get(id).url, moved.url());
    }

    void switchingAServerOffStopsItAndOnStartsItAgain()
    {
        FakeSubsonic fake;
        fake.serveAlbums({{"a1", 2}});
        Env e;
        const QString id = e.hub.addServer(fake.url(), "amy", "sesame", true);
        QTRY_VERIFY(e.status(id).startsWith("Connected"));
        e.hub.setServerEnabled(id, false);
        QCOMPARE(e.state(id), QString("off"));
        const int syncs = fake.count("getAlbumList2");
        e.hub.sync();
        QTest::qWait(300);
        QCOMPARE(fake.count("getAlbumList2"), syncs);                // a server that is off is not asked
        e.hub.setServerEnabled(id, true);
        QTRY_VERIFY(fake.count("getAlbumList2") > syncs);
    }

    void aSyncPublishesServerPlaylistsAndFavorites()
    {
        FakeSubsonic fake;
        fake.serveAlbums({{"a1", 2}, {"a2", 2}});                   // the first song of each album is starred
        fake.setResult("getPlaylists", R"("playlists":{"playlist":[{"id":"p1","name":"Road"}]})");
        fake.setResult("getPlaylist", R"("playlist":{"id":"p1","entry":[{"id":"a1-2","title":"T2"}]})");
        Env e;
        const QString id = e.hub.addServer(fake.url(), "amy", "sesame", true);
        QTRY_COMPARE(e.playlists.list()->rowCount(), 2);
        QCOMPARE(e.playlists.list()->get(0).value("title").toString(), QString("Road"));
        QCOMPARE(e.playlists.list()->get(0).value("playlistId").toString(), QString("p1"));
        QCOMPARE(e.playlists.list()->get(1).value("title").toString(), QString("Favorites"));
        QCOMPARE(e.playlists.paths(e.playlists.list()->get(1).value("key").toString()),
                 QStringList({"subsonic:" + id + "/a1-1", "subsonic:" + id + "/a2-1"}));
        QVERIFY(e.playlists.list()->get(1).value("readOnly").toBool());
    }

    void serverPlaylistsCanBeCreatedRenamedChangedAndDeleted()
    {
        FakeSubsonic fake;
        fake.serveAlbums({{"a1", 3}});
        PlaylistBackend backend;
        backend.attach(fake);
        Env e;
        const QString id = e.hub.addServer(fake.url(), "amy", "sesame", true);
        QTRY_COMPARE(e.library.songs()->rowCount(), 3);
        const auto serverRows = [&] {
            QStringList titles;
            for (int i = 0; i < e.playlists.list()->rowCount(); ++i)
                if (e.playlists.list()->get(i).value("serverId").toString() == id)
                    titles << e.playlists.list()->get(i).value("title").toString();
            return titles;
        };
        const auto rowFor = [&](const QString &title) {
            for (int i = 0; i < e.playlists.list()->rowCount(); ++i)
                if (e.playlists.list()->get(i).value("title").toString() == title)
                    return e.playlists.list()->get(i);
            return QVariantMap();
        };
        const QString s1 = "subsonic:" + id + "/a1-1", s2 = "subsonic:" + id + "/a1-2", s3 = "subsonic:" + id + "/a1-3";

        e.hub.createPlaylist(id, "Road", {s1, s2, "/local/not-on-the-server.flac"});     // only its own songs go in
        QTRY_VERIFY(serverRows().contains("Road"));
        QVERIFY(!rowFor("Road").value("readOnly").toBool());                              // a real playlist can be edited
        QCOMPARE(backend.lists.value("p1").songs, QStringList({"a1-1", "a1-2"}));
        const QString pid = rowFor("Road").value("playlistId").toString();
        QCOMPARE(pid, QString("p1"));
        QVERIFY(rowFor("Favorites").value("readOnly").toBool());                          // Favorites is not one to edit

        e.hub.addToPlaylist(id, pid, {s3});
        QTRY_COMPARE(e.playlists.paths(rowFor("Road").value("key").toString()).size(), 3);
        e.hub.removeFromPlaylist(id, pid, 0);                                              // the first song
        QTRY_COMPARE(e.playlists.paths(rowFor("Road").value("key").toString()).size(), 2);
        QCOMPARE(backend.lists.value("p1").songs, QStringList({"a1-2", "a1-3"}));

        e.hub.renamePlaylist(id, pid, "Highway");
        QTRY_VERIFY(serverRows().contains("Highway"));
        QVERIFY(!serverRows().contains("Road"));

        e.hub.deletePlaylist(id, pid);
        QTRY_VERIFY(!serverRows().contains("Highway"));
        QVERIFY(backend.lists.isEmpty());
    }

    void aRefusedPlaylistEditIsReportedAndChangesNothing()
    {
        FakeSubsonic fake;
        fake.serveAlbums({{"a1", 1}});
        PlaylistBackend backend;
        backend.lists["p1"] = {"Road", {"a1-1"}};
        backend.attach(fake);
        Env e;
        const QString id = e.hub.addServer(fake.url(), "amy", "sesame", true);
        QTRY_VERIFY(e.playlists.list()->rowCount() >= 1);
        fake.setHandler("deletePlaylist", [](const QUrlQuery &) { return QByteArray("ERR:Not yours"); });
        e.hub.deletePlaylist(id, "p1");
        QTRY_VERIFY(e.status(id).contains("Not yours"));
        QCOMPARE(e.state(id), QString("connected"));                                      // the server is fine, the edit failed
        QVERIFY(backend.lists.contains("p1"));

        e.hub.addToPlaylist(id, "p1", {"/local/x.flac"});                                  // nothing of this server's to add
        QVERIFY(e.status(id).contains("songs"));
    }

    void playedSongsAreScrobbledAndOnlyWhileTheServerIsOn()
    {
        FakeSubsonic fake;
        fake.serveAlbums({{"a1", 1}});
        fake.setResult("scrobble", {});
        Env e;
        const QString id = e.hub.addServer(fake.url(), "amy", "sesame", true);
        QTRY_VERIFY(e.status(id).startsWith("Connected"));
        const QString path = "subsonic:" + id + "/a1-1";

        e.hub.source(id)->noteProgress(path, 0.1, 200);
        QTRY_COMPARE(fake.count("scrobble"), 1);
        QCOMPARE(QUrlQuery(fake.requests.last()).queryItemValue("id"), QString("a1-1"));
        QCOMPARE(QUrlQuery(fake.requests.last()).queryItemValue("submission"), QString("false"));
        e.hub.source(id)->noteProgress(path, 120, 200);
        QTRY_COMPARE(fake.count("scrobble"), 2);
        QCOMPARE(QUrlQuery(fake.requests.last()).queryItemValue("submission"), QString("true"));
        e.hub.source(id)->noteProgress("/local/song.flac", 0.1, 200);   // local songs are not reported
        QTest::qWait(200);
        QCOMPARE(fake.count("scrobble"), 2);

        e.hub.setServerEnabled(id, false);                              // and nothing goes out when it is off
        e.hub.source(id)->noteProgress(path, 0.1, 200);
        e.hub.source(id)->noteProgress("subsonic:" + id + "/a2-1", 0.1, 200);
        QTest::qWait(200);
        QCOMPARE(fake.count("scrobble"), 2);
    }

    void togglingAFavoriteStarsThenUnstarsAndRollsBackOnFailure()
    {
        FakeSubsonic fake;
        fake.serveAlbums({{"a1", 2}});                                  // a1-1 starts out starred, a1-2 not
        fake.setResult("star", {});
        fake.setResult("unstar", {});
        Env e;
        const QString id = e.hub.addServer(fake.url(), "amy", "sesame", true);
        QTRY_COMPARE(e.library.songs()->rowCount(), 2);
        QTRY_VERIFY(e.playlists.list()->rowCount() >= 1);               // Favorites is there (a1-1)
        const QString two = "subsonic:" + id + "/a1-2", one = "subsonic:" + id + "/a1-1";

        e.hub.toggleStar(two);
        QVERIFY(e.library.isStarred(two));                              // at once, before the server answers
        QTRY_COMPARE(fake.count("star"), 1);
        QCOMPARE(QUrlQuery(fake.requests.last()).queryItemValue("id"), QString("a1-2"));
        QCOMPARE(e.playlists.paths(e.playlists.list()->get(0).value("key").toString()).size(), 2);   // Favorites grew

        e.hub.toggleStar(one);
        QTRY_COMPARE(fake.count("unstar"), 1);
        QVERIFY(!e.library.isStarred(one));

        fake.setHandler("star", [](const QUrlQuery &) { return QByteArray("ERR:No can do"); });
        e.hub.toggleStar(one);                                          // the server refuses: back to unstarred
        QVERIFY(e.library.isStarred(one));
        QTRY_VERIFY(!e.library.isStarred(one));
        QVERIFY(e.status(id).contains("No can do"));

        e.hub.toggleStar("/local/x.flac");                              // a local song: nothing happens
        QCOMPARE(fake.count("star"), 2);
    }

    void serverCoversDownloadOncePerCoverIdAndShowUpAsFileUrls()
    {
        FakeSubsonic fake;
        fake.serveAlbums({{"a1", 2}});                                  // both songs carry the cover id "a1"
        fake.setBinary("getCoverArt", QByteArray("\xff\xd8\xff\xe0 fake jpeg"), "image/jpeg");
        Env e;
        const QString id = e.hub.addServer(fake.url(), "amy", "sesame", true);
        QTRY_COMPARE(e.library.songs()->rowCount(), 2);
        QSignalSpy changed(&e.library, &Library::coversChanged);
        const QString p1 = "subsonic:" + id + "/a1-1", p2 = "subsonic:" + id + "/a1-2";

        QCOMPARE(e.library.coverFor(p1), QString());                    // queued, not there yet
        QCOMPARE(e.library.coverFor(p2), QString());                    // same cover id: no second request
        QTRY_VERIFY(changed.count() >= 1);
        const QString url = e.library.coverFor(p2);
        QVERIFY(url.startsWith("file://"));
        QVERIFY(QFileInfo::exists(QUrl(url).toLocalFile()));
        QCOMPARE(e.library.coverFor(p1), url);
        QCOMPARE(fake.count("getCoverArt"), 1);
        QCOMPARE(QUrlQuery(fake.requests.last()).queryItemValue("size"), QString("600"));
    }

    void aCoverThatIsNotAnImageIsNotKept()
    {
        FakeSubsonic fake;
        fake.serveAlbums({{"a1", 1}});
        fake.setBinary("getCoverArt", QByteArray("\x00\x00\rIHDR broken png without its signature"), "image/png");
        Env e;
        const QString id = e.hub.addServer(fake.url(), "amy", "sesame", true);
        QTRY_COMPARE(e.library.songs()->rowCount(), 1);
        QSignalSpy changed(&e.library, &Library::coversChanged);
        QCOMPARE(e.library.coverFor("subsonic:" + id + "/a1-1"), QString());
        QTRY_COMPARE(fake.count("getCoverArt"), 1);
        QTest::qWait(300);
        QCOMPARE(changed.count(), 0);                                   // nothing was announced
        QVERIFY(QDir(e.dir.filePath("covers")).entryList({"remote-*.jpg"}).isEmpty());
    }

    void anEmptyServerLibraryIsSaidSoInsteadOfConnectedWithZeroSongs()
    {
        FakeSubsonic fake;
        fake.serveAlbums({});
        Env e;
        const QString id = e.hub.addServer(fake.url(), "amy", "sesame", true);
        QTRY_VERIFY(e.status(id).contains("no songs"));
        QCOMPARE(e.library.songs()->rowCount(), 0);
    }

    void aDeadServerKeepsItsCachedSongsAndShowsInTheStatus()
    {
        QTemporaryDir dir;
        qputenv("FAKE_SECRET_DIR", dir.path().toUtf8());
        Servers servers(dir.filePath("servers.json"));
        const QString id = servers.add("http://127.0.0.1:1", "amy", true);   // nothing listens there
        {
            Library library({"/m"}, "/nonexistent-scanner", dir.filePath("index.jsonl"));
            Track t;
            t.path = Track::remotePath(id, "9"); t.title = "Kept"; t.artist = t.albumArtist = "Amy"; t.album = "Beta";
            library.setRemoteTracks(id, {t});
            library.writeRemoteCache(id);
        }
        Secrets secrets(writeFake(dir.path()));
        QSignalSpy stored(&secrets, &Secrets::stored);
        secrets.store("amy@http://127.0.0.1:1", "sesame");
        QTRY_COMPARE(stored.count(), 1);

        Library fresh({"/m"}, "/nonexistent-scanner", dir.filePath("index.jsonl"));
        Playlists playlists(dir.filePath("pl"));
        SubsonicHub hub(servers, fresh, playlists, secrets);
        hub.start();
        QCOMPARE(fresh.songs()->rowCount(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(hub.source(id)->status().startsWith("Server offline"), 20000);
        QCOMPARE(hub.source(id)->state(), QString("offline"));
        QCOMPARE(fresh.songs()->rowCount(), 1);                         // still there
        QVERIFY(hub.status().startsWith("Server offline"));             // and the player bar says so
    }

    void removingAServerForgetsItsPasswordSongsAndCache()
    {
        FakeSubsonic fake;
        fake.serveAlbums({{"a1", 2}});
        Env e;
        const QString id = e.hub.addServer(fake.url(), "amy", "sesame", true);
        QTRY_COMPARE(e.library.songs()->rowCount(), 2);

        QSignalSpy cleared(&e.secrets, &Secrets::cleared);
        e.hub.removeServer(id);
        QCOMPARE(e.library.songs()->rowCount(), 0);
        QCOMPARE(e.hub.servers()->rowCount(), 0);
        QVERIFY(e.servers.list().isEmpty());
        QVERIFY(!QFileInfo::exists(e.dir.filePath("subsonic-" + id + ".jsonl")));
        QTRY_COMPARE(cleared.count(), 1);
        QSignalSpy looked(&e.secrets, &Secrets::looked);
        e.secrets.lookup(QString("amy@") + fake.url());
        QTRY_COMPARE(looked.count(), 1);
        QVERIFY(!looked.at(0).at(1).toBool());                          // the keyring entry is gone too
    }

    void theSingleServerOfAnOlderVersionCarriesOver()
    {
        FakeSubsonic fake;
        fake.serveAlbums({{"a1", 3}});
        QTemporaryDir dir;
        qputenv("FAKE_SECRET_DIR", dir.path().toUtf8());
        Settings settings(dir.filePath("config.toml"));
        settings.set("serverUrl", fake.url());
        settings.set("serverUser", "amy");
        settings.set("serverEnabled", true);
        {   // what that version left: its cache with old-style paths, and the password under the old keyring name
            QFile c(dir.filePath("subsonic.jsonl"));
            QVERIFY(c.open(QIODevice::WriteOnly));
            c.write(R"({"path":"subsonic:a1-1","title":"Old","artist":"Amy","album":"Beta"})" "\n");
            QFile k(dir.filePath("omamusic-amy_" + QString(fake.url()).replace(QRegularExpression("[/:@]"), "_")));
            QVERIFY(k.open(QIODevice::WriteOnly));
            k.write("sesame");
        }
        Servers servers(dir.filePath("servers.json"));
        QVERIFY(servers.migrateFrom(settings));
        Library library({"/m"}, "/nonexistent-scanner", dir.filePath("index.jsonl"));
        Playlists playlists(dir.filePath("pl"));
        Secrets secrets(writeFake(dir.path()));
        SubsonicHub hub(servers, library, playlists, secrets);
        hub.start();
        QCOMPARE(library.songs()->rowCount(), 1);                       // the old cache, as the first server's
        QCOMPARE(library.songs()->get(0).value("path").toString(), QString("subsonic:s1/a1-1"));
        QTRY_COMPARE(library.songs()->rowCount(), 3);                   // then a real sync, with the old password
        QVERIFY(hub.source("s1")->status().startsWith("Connected"));
    }
};

QTEST_MAIN(TstSubsonicHub)
#include "tst_subsonichub.moc"
