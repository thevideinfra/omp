#include <QtTest>

#include "library.h"

static Track mk(const QString &path, const QString &title, const QString &artist,
                const QString &album, int n)
{
    Track t;
    t.path = path;
    t.title = title;
    t.artist = artist;
    t.albumArtist = artist;
    t.album = album;
    t.track = n;
    t.duration = 60;
    return t;
}

// A flac with a red 64x64 jpeg attached as cover art (or none when withArt is false).
static bool makeFlac(const QString &path, bool withArt)
{
    QStringList args{"-v", "error", "-f", "lavfi", "-i", "sine=d=0.3"};
    if (withArt) {
        args << "-f" << "lavfi" << "-i" << "color=c=red:s=64x64:d=1" << "-map" << "0:a" << "-map" << "1:v"
             << "-c:v" << "mjpeg" << "-frames:v" << "1" << "-disposition:v" << "attached_pic";
    }
    args << "-y" << path;
    QProcess ff;
    ff.start("ffmpeg", args);
    return ff.waitForFinished(10000) && ff.exitCode() == 0;
}

// The tests that run ffmpeg are skipped where it is not installed.
#define NEED_FFMPEG() \
    if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) \
        QSKIP("ffmpeg is not installed")

static QVector<Track> fixture()
{
    return {
        mk("/m/zed/alpha/02.flac", "Two", "Zed", "Alpha", 2),
        mk("/m/zed/alpha/01.flac", "One", "Zed", "Alpha", 1),
        mk("/m/amy/beta/01.flac", "Solo", "Amy", "Beta", 1),
        mk("/m/top.flac", "Top", "Amy", "Singles", 1),
    };
}

class TstLibrary : public QObject
{
    Q_OBJECT
private slots:
    void albumsSortedByArtistThenAlbum()
    {
        Library lib({"/m"}, "unused", "unused");
        lib.setTracks(fixture());
        QCOMPARE(lib.trackCount(), 4);
        QCOMPARE(lib.albums()->rowCount(), 3);
        QCOMPARE(lib.albums()->get(0).value("title").toString(), QString("Beta"));
        QCOMPARE(lib.albums()->get(0).value("subtitle").toString(), QString("Amy"));
        QCOMPARE(lib.albums()->get(2).value("title").toString(), QString("Alpha"));
        QCOMPARE(lib.albums()->get(2).value("meta").toString(), QString("2 tracks"));
    }

    void albumTracksOrderedByTrackNumber()
    {
        Library lib({"/m"}, "unused", "unused");
        lib.setTracks(fixture());
        const QString key = lib.albums()->get(2).value("key").toString();
        const QVariantList tracks = lib.albumTracks(key);
        QCOMPARE(tracks.size(), 2);
        QCOMPARE(tracks.at(0).toMap().value("title").toString(), QString("One"));
        QCOMPARE(tracks.at(1).toMap().value("title").toString(), QString("Two"));
    }

    void artistsAndTheirTracks()
    {
        Library lib({"/m"}, "unused", "unused");
        lib.setTracks(fixture());
        QCOMPARE(lib.artists()->rowCount(), 2);
        QCOMPARE(lib.artists()->get(0).value("title").toString(), QString("Amy"));
        QCOMPARE(lib.artists()->get(0).value("subtitle").toString(), QString("2 albums"));
        QCOMPARE(lib.artistTracks("Amy").size(), 2);
        QCOMPARE(lib.artistTracks("Zed").size(), 2);
    }

    void browseWalksTheIndex()
    {
        Library lib({"/m"}, "unused", "unused");
        lib.setTracks(fixture());
        lib.browse("");
        QCOMPARE(lib.folder()->rowCount(), 1);
        QCOMPARE(lib.folder()->get(0).value("path").toString(), QString("/m"));
        QVERIFY(lib.folder()->get(0).value("isDir").toBool());

        lib.browse("/m");
        QCOMPARE(lib.folder()->rowCount(), 3);  // amy/, zed/, top.flac
        QCOMPARE(lib.folder()->get(0).value("title").toString(), QString("amy"));
        QCOMPARE(lib.folder()->get(1).value("title").toString(), QString("zed"));
        QVERIFY(!lib.folder()->get(2).value("isDir").toBool());
        QCOMPARE(lib.folder()->get(2).value("title").toString(), QString("Top"));

        lib.browse("/m/zed/alpha");
        QCOMPARE(lib.folder()->rowCount(), 2);
        QCOMPARE(lib.folder()->get(0).value("title").toString(), QString("One"));

        lib.up();
        QCOMPARE(lib.folderPath(), QString("/m/zed"));
        lib.browse("/m");
        lib.up();
        QCOMPARE(lib.folderPath(), QString());
    }

    void searchMatchesAllTerms()
    {
        Library lib({"/m"}, "unused", "unused");
        lib.setTracks(fixture());
        lib.setQuery("zed alp");
        QCOMPARE(lib.results()->rowCount(), 2);
        lib.setQuery("solo");
        QCOMPARE(lib.results()->rowCount(), 1);
        lib.setQuery("");
        QCOMPARE(lib.results()->rowCount(), 0);
    }

    void coverFoundCaseInsensitively()
    {
        QTemporaryDir dir;
        QFile(dir.filePath("Cover.JPG")).open(QIODevice::WriteOnly);
        Library lib({dir.path()}, "unused", "unused");
        lib.assumeReachable();
        QVERIFY(lib.coverFor(dir.filePath("x.flac")).endsWith("/Cover.JPG"));
        QTemporaryDir bare;
        QCOMPARE(lib.coverFor(bare.filePath("x.flac")), QString());
    }

    void scanFillsIndexAndCache()
    {
        NEED_FFMPEG();
        QTemporaryDir music;
        QTemporaryDir state;
        QDir(music.path()).mkdir("A");
        for (const char *name : {"one.flac", "two.flac"}) {
            QProcess ff;
            ff.start("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "sine=d=0.3",
                                "-metadata", "artist=Amy", "-metadata", "album=Beta",
                                music.filePath(QString("A/") + name)});
            QVERIFY(ff.waitForFinished(10000));
            QCOMPARE(ff.exitCode(), 0);
        }
        const QString cache = state.filePath("index.jsonl");

        Library lib({music.path()}, OMA_SCAN_PROGRAM, cache);
        lib.rescan();
        QTRY_COMPARE_WITH_TIMEOUT(lib.trackCount(), 2, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(!lib.scanning(), 5000);
        QVERIFY(QFileInfo::exists(cache));

        Library again({music.path()}, "/nonexistent-scanner", cache);
        again.start();
        QCOMPARE(again.trackCount(), 2);  // loaded from cache, scanner failed to start
    }

    void unwritableCacheWarns()
    {
        NEED_FFMPEG();
        QTemporaryDir music;
        QTemporaryDir state;
        QProcess ff;
        ff.start("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "sine=d=0.3",
                            "-metadata", "artist=Amy", "-metadata", "album=Beta",
                            music.filePath("one.flac")});
        QVERIFY(ff.waitForFinished(10000));
        QCOMPARE(ff.exitCode(), 0);
        QFile blocker(state.filePath("blocker"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        const QString cache = state.filePath("blocker/sub/index.jsonl");

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("cannot (create index cache|write index cache)"));
        Library lib({music.path()}, OMA_SCAN_PROGRAM, cache);
        lib.rescan();
        QTRY_COMPARE_WITH_TIMEOUT(lib.trackCount(), 1, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(!lib.scanning(), 5000);
    }

    void emptyScanKeepsOldIndex()
    {
        NEED_FFMPEG();
        QTemporaryDir music;
        QTemporaryDir state;
        QProcess ff;
        ff.start("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "sine=d=0.3",
                            "-metadata", "artist=Amy", "-metadata", "album=Beta",
                            music.filePath("one.flac")});
        QVERIFY(ff.waitForFinished(10000));
        QCOMPARE(ff.exitCode(), 0);
        const QString cache = state.filePath("index.jsonl");
        Library lib({music.path()}, OMA_SCAN_PROGRAM, cache);
        lib.rescan();
        QTRY_COMPARE_WITH_TIMEOUT(lib.trackCount(), 1, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(!lib.scanning(), 5000);

        QVERIFY(QFile::remove(music.filePath("one.flac")));   // like an unmounted share
        lib.rescan();
        QTRY_VERIFY_WITH_TIMEOUT(!lib.scanning(), 20000);
        QCOMPARE(lib.trackCount(), 1);
        QCOMPARE(lib.offlineRoots(), QStringList({music.path()}));
        Library again({music.path()}, "/nonexistent-scanner", cache);
        again.start();
        QCOMPARE(again.trackCount(), 1);   // the cache was not wiped either
    }

    void anOfflineFolderKeepsItsSongsAndTheOthersStayLive()
    {
        NEED_FFMPEG();
        QTemporaryDir a, b, state;
        for (const auto &f : {a.filePath("a.flac"), b.filePath("b.flac")}) {
            QProcess ff;
            ff.start("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "sine=d=0.3", f});
            QVERIFY(ff.waitForFinished(10000));
        }
        Library lib({a.path(), b.path()}, OMA_SCAN_PROGRAM, state.filePath("index.jsonl"));
        lib.rescan();
        QTRY_COMPARE_WITH_TIMEOUT(lib.trackCount(), 2, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(!lib.scanning(), 5000);
        QVERIFY(lib.offlineRoots().isEmpty());

        QVERIFY(QFile::remove(b.filePath("b.flac")));   // b "unmounts"
        QProcess ff;
        ff.start("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "sine=d=0.3", a.filePath("a2.flac")});
        QVERIFY(ff.waitForFinished(10000));
        lib.rescan();
        QTRY_COMPARE_WITH_TIMEOUT(lib.trackCount(), 3, 20000);   // a2 added, b kept
        QTRY_VERIFY_WITH_TIMEOUT(!lib.scanning(), 5000);
        QCOMPARE(lib.offlineRoots(), QStringList({b.path()}));
    }

    void coversWaitForAFinishedScan()
    {
        QTemporaryDir dir, state;
        QFile c(dir.filePath("cover.jpg"));
        QVERIFY(c.open(QIODevice::WriteOnly));
        c.close();
        QFile idx(state.filePath("index.jsonl"));
        QVERIFY(idx.open(QIODevice::WriteOnly));
        idx.write(QString("{\"path\":\"%1\",\"size\":1,\"mtime\":1,\"title\":\"t\"}\n")
                      .arg(dir.filePath("t.flac")).toUtf8());
        idx.close();
        Library lib({dir.path()}, "/nonexistent-scanner", state.filePath("index.jsonl"));
        lib.start();   // scanner cannot start: the folder is never confirmed reachable
        QCOMPARE(lib.coverFor(dir.filePath("t.flac")), QString());
    }

    void songsCanBeSortedByEachKeyAndReversed()
    {
        Library lib({"/m"}, "/nonexistent-scanner", "/nonexistent/cache");
        QVector<Track> tracks = fixture();   // One, Two (Zed/Alpha), Solo (Amy/Beta), Top (Amy/Singles)
        tracks[0].duration = 30;             // Two
        tracks[1].duration = 10;             // One
        tracks[2].duration = 20;             // Solo
        tracks[3].duration = 40;             // Top
        tracks[0].mtime = 4; tracks[1].mtime = 3; tracks[2].mtime = 2; tracks[3].mtime = 1;
        lib.setTracks(tracks);
        const auto titles = [&] {
            QStringList out;
            for (int i = 0; i < lib.songs()->rowCount(); ++i)
                out << lib.songs()->get(i).value("title").toString();
            return out;
        };
        QCOMPARE(lib.songSort(), QString("title"));
        QCOMPARE(titles(), QStringList({"One", "Solo", "Top", "Two"}));
        lib.setSongSort("artist");   // Amy: Solo, Top; Zed: One, Two (ties by title)
        QCOMPARE(titles(), QStringList({"Solo", "Top", "One", "Two"}));
        lib.setSongSort("album");    // Alpha: One, Two; Beta: Solo; Singles: Top
        QCOMPARE(titles(), QStringList({"One", "Two", "Solo", "Top"}));
        lib.setSongSort("length");
        QCOMPARE(titles(), QStringList({"One", "Solo", "Two", "Top"}));
        lib.setSongSort("added");    // newest first
        QCOMPARE(titles(), QStringList({"Two", "One", "Solo", "Top"}));
        lib.setSongDescending(true); // flips whatever the key's natural order is
        QCOMPARE(titles(), QStringList({"Top", "Solo", "One", "Two"}));
        lib.setSongSort("bogus");    // ignored
        QCOMPARE(lib.songSort(), QString("added"));
    }

    void songsSortByYearAndByFavorite()
    {
        Library lib({"/m"}, "/nonexistent-scanner", "/nonexistent/cache");
        Track a = mk("/m/a.flac", "Alpha", "Amy", "X", 1); a.year = 2010;
        Track b = mk("/m/b.flac", "Bravo", "Amy", "X", 2); b.year = 1990;
        Track c = mk("/m/c.flac", "Charlie", "Amy", "X", 3);                 // no year
        Track r = mk("subsonic:1", "Delta", "Amy", "Y", 1); r.year = 2000; r.starred = true;
        Track r2 = mk("subsonic:2", "Echo", "Amy", "Y", 2); r2.starred = true;
        lib.setTracks({a, b, c});
        lib.setRemoteTracks({r, r2});
        const auto titles = [&] {
            QStringList out;
            for (int i = 0; i < lib.songs()->rowCount(); ++i)
                out << lib.songs()->get(i).value("title").toString();
            return out;
        };
        lib.setSongSort("year");
        QCOMPARE(titles(), QStringList({"Bravo", "Delta", "Alpha", "Charlie", "Echo"}));   // unknown years last, by title
        lib.setSongDescending(true);
        QCOMPARE(titles(), QStringList({"Echo", "Charlie", "Alpha", "Delta", "Bravo"}));
        lib.setSongDescending(false);
        lib.setSongSort("favorite");
        QCOMPARE(titles(), QStringList({"Delta", "Echo", "Alpha", "Bravo", "Charlie"}));   // starred first, then by title
        lib.setStarred("subsonic:2", false);
        lib.setSongSort("title");                                                           // re-sort after a change
        lib.setSongSort("favorite");
        QCOMPARE(titles().first(), QString("Delta"));
        QCOMPARE(titles().at(1), QString("Alpha"));
    }

    void moreSongSortKeysAndPerSongInfo()
    {
        Library lib({"/m"}, "/nonexistent-scanner", "/nonexistent/cache");
        Track a = mk("/m/a.flac", "Alpha", "Zed", "X", 2); a.albumArtist = "Zed"; a.size = 300; a.disc = 1;
        Track b = mk("/m/b.mp3", "Bravo", "Amy", "X", 1); b.albumArtist = "Amy"; b.size = 100; b.disc = 1;
        Track c = mk("/m/c.ogg", "Charlie", "Amy", "Y", 5); c.albumArtist = "Amy"; c.size = 200; c.disc = 2; c.year = 2001;
        lib.setTracks({a, b, c});
        const auto titles = [&] {
            QStringList out;
            for (int i = 0; i < lib.songs()->rowCount(); ++i)
                out << lib.songs()->get(i).value("title").toString();
            return out;
        };
        lib.setSongSort("albumartist");   // Amy: Bravo, Charlie (by title); Zed: Alpha
        QCOMPARE(titles(), QStringList({"Bravo", "Charlie", "Alpha"}));
        lib.setSongSort("track");          // disc, then track number: b (1/1), a (1/2), c (2/5)
        QCOMPARE(titles(), QStringList({"Bravo", "Alpha", "Charlie"}));
        lib.setSongSort("size");
        QCOMPARE(titles(), QStringList({"Bravo", "Charlie", "Alpha"}));
        lib.setSongSort("format");         // flac, mp3, ogg
        QCOMPARE(titles(), QStringList({"Alpha", "Bravo", "Charlie"}));

        const QVariantMap info = lib.trackInfo("/m/c.ogg");
        QCOMPARE(info.value("albumArtist").toString(), QString("Amy"));
        QCOMPARE(info.value("track").toInt(), 5);
        QCOMPARE(info.value("year").toInt(), 2001);
        QCOMPARE(info.value("size").toLongLong(), qint64(200));
        QCOMPARE(info.value("format").toString(), QString("OGG"));
        QVERIFY(lib.trackInfo("/nope").isEmpty());

        // The keys that open a song's artist and album in their tabs are the row keys.
        QStringList albumKeys, artistKeys;
        for (int i = 0; i < lib.albums()->rowCount(); ++i) albumKeys << lib.albums()->get(i).value("key").toString();
        for (int i = 0; i < lib.artists()->rowCount(); ++i) artistKeys << lib.artists()->get(i).value("key").toString();
        QVERIFY(albumKeys.contains(lib.albumKeyFor("/m/c.ogg")));
        QVERIFY(artistKeys.contains(lib.artistKeyFor("/m/c.ogg")));
        QCOMPARE(lib.artistKeyFor("/m/c.ogg"), QString("Amy"));
        QCOMPARE(lib.artistKeyFor("/nope"), QString());
    }

    void tracksUnderAFolderIncludeItsSubfoldersInPathOrder()
    {
        Library lib({"/m"}, "/nonexistent-scanner", "/nonexistent/cache");
        lib.setTracks({mk("/m/zed/b.flac", "B", "Zed", "X", 1), mk("/m/zed/sub/c.flac", "C", "Zed", "X", 2),
                       mk("/m/amy/a.flac", "A", "Amy", "Y", 1), mk("/m/zedx/d.flac", "D", "Q", "Z", 1)});
        lib.setRemoteTracks({mk("subsonic:1", "R", "Amy", "Y", 1)});
        const QVariantList under = lib.tracksUnder("/m/zed");
        QCOMPARE(under.size(), 2);                                        // not /m/zedx, not the server's songs
        QCOMPARE(under.at(0).toMap().value("title").toString(), QString("B"));
        QCOMPARE(under.at(1).toMap().value("title").toString(), QString("C"));
        QCOMPARE(lib.tracksUnder("/m").size(), 4);
        QVERIFY(lib.tracksUnder("/nope").isEmpty());
    }

    void anAlbumRowShowsItsYearNextToTheArtist()
    {
        Library lib({"/m"}, "/nonexistent-scanner", "/nonexistent/cache");
        Track a = mk("/m/a/1.flac", "One", "Amy", "Beta", 1); a.year = 2004;
        Track b = mk("/m/a/2.flac", "Two", "Amy", "Beta", 2); b.year = 2003;      // the earliest year of the album
        Track c = mk("/m/b/1.flac", "Solo", "Zed", "Gamma", 1);                  // no year known
        lib.setTracks({a, b, c});
        QStringList subs;
        for (int i = 0; i < lib.albums()->rowCount(); ++i)
            subs << lib.albums()->get(i).value("subtitle").toString();
        QCOMPARE(subs, QStringList({"Amy  ·  2003", "Zed"}));
    }

    void albumsAndArtistsCanBeSorted()
    {
        Library lib({"/m"}, "/nonexistent-scanner", "/nonexistent/cache");
        QVector<Track> tracks = fixture();   // Zed/Alpha (Two, One), Amy/Beta (Solo), Amy/Singles (Top)
        tracks[0].duration = 100; tracks[0].mtime = 1;   // Alpha: 110 s, newest file 1
        tracks[1].duration = 10;  tracks[1].mtime = 1;
        tracks[2].duration = 50;  tracks[2].mtime = 9;   // Beta
        tracks[3].duration = 300; tracks[3].mtime = 5;   // Singles
        lib.setTracks(tracks);
        const auto titles = [](RowsModel *m) {
            QStringList out;
            for (int i = 0; i < m->rowCount(); ++i)
                out << m->get(i).value("title").toString();
            return out;
        };
        QCOMPARE(titles(lib.albums()), QStringList({"Beta", "Singles", "Alpha"}));   // artist, then album
        QCOMPARE(lib.albumSort(), QString("artist"));
        lib.setAlbumSort("title");
        QCOMPARE(titles(lib.albums()), QStringList({"Alpha", "Beta", "Singles"}));
        lib.setAlbumSort("length");
        QCOMPARE(titles(lib.albums()), QStringList({"Beta", "Alpha", "Singles"}));
        lib.setAlbumSort("added");
        QCOMPARE(titles(lib.albums()), QStringList({"Beta", "Singles", "Alpha"}));
        lib.setAlbumDescending(true);
        QCOMPARE(titles(lib.albums()), QStringList({"Alpha", "Singles", "Beta"}));
        lib.setAlbumSort("album");   // not an album key: ignored
        QCOMPARE(lib.albumSort(), QString("added"));

        QCOMPARE(titles(lib.artists()), QStringList({"Amy", "Zed"}));
        QCOMPARE(lib.artists()->get(0).value("cover").toString(), QString("/m/amy/beta/01.flac"));   // the artist's first song
        QCOMPARE(lib.artists()->get(0).value("initial").toString(), QString("A"));   // the A-Z sections
        QCOMPARE(lib.artists()->get(1).value("initial").toString(), QString("Z"));
        lib.setArtistSort("length");
        QCOMPARE(lib.artists()->get(0).value("initial").toString(), QString());      // none unless sorted by name   // Zed 110 s, Amy 350 s
        QCOMPARE(titles(lib.artists()), QStringList({"Zed", "Amy"}));
        lib.setArtistSort("added");    // Amy newest 9
        QCOMPARE(titles(lib.artists()), QStringList({"Amy", "Zed"}));
        lib.setArtistDescending(true);
        QCOMPARE(titles(lib.artists()), QStringList({"Zed", "Amy"}));
    }

    void sortChoicesRoundTripThroughAState()
    {
        Library lib({"/m"}, "/nonexistent-scanner", "/nonexistent/cache");
        QCOMPARE(lib.sortState("songs"), QString("title,asc"));
        QCOMPARE(lib.sortState("albums"), QString("artist,asc"));
        QCOMPARE(lib.sortState("artists"), QString("title,asc"));
        lib.restoreSortState("songs", "length,desc");
        lib.restoreSortState("albums", "added,asc");
        lib.restoreSortState("artists", "length,desc");
        QCOMPARE(lib.songSort(), QString("length"));
        QVERIFY(lib.songDescending());
        QCOMPARE(lib.sortState("songs"), QString("length,desc"));
        QCOMPARE(lib.sortState("albums"), QString("added,asc"));
        QCOMPARE(lib.sortState("artists"), QString("length,desc"));
        lib.restoreSortState("songs", "bogus,desc");   // a bad key leaves it alone, the direction applies
        lib.restoreSortState("albums", "");            // nothing stored
        lib.restoreSortState("nope", "title,asc");
        QCOMPARE(lib.songSort(), QString("length"));
        QCOMPARE(lib.albumSort(), QString("added"));
    }

    void serverSongsMergeIntoTheListsButNotTheFolders()
    {
        QTemporaryDir state;
        Library lib({"/m"}, "/nonexistent-scanner", state.filePath("index.jsonl"));
        lib.setTracks(fixture());   // Zed/Alpha, Amy/Beta, Amy/Singles
        Track r = mk("subsonic:7", "Far", "Amy", "Beta", 1);   // same album name as a local one
        r.coverId = "al-1";
        Track r2 = mk("subsonic:8", "Away", "Amy", "Remote Only", 1);
        lib.setRemoteTracks({r, r2});

        QCOMPARE(lib.songs()->rowCount(), 6);                     // 4 local + 2 server
        QStringList albums;
        for (int i = 0; i < lib.albums()->rowCount(); ++i)
            albums << lib.albums()->get(i).value("title").toString();
        QCOMPARE(albums.count("Beta"), 2);                       // never merged with the local Beta
        QCOMPARE(albums.size(), 5);
        QCOMPARE(lib.artists()->rowCount(), 2);                  // Amy and Zed: one Amy for both sources
        QCOMPARE(lib.tracksForPaths({"subsonic:7"}).size(), 1);
        lib.setQuery("away");
        QCOMPARE(lib.results()->rowCount(), 1);
        lib.setQuery("");

        lib.browse("/m");                                        // the folder tree is local only
        for (int i = 0; i < lib.folder()->rowCount(); ++i)
            QVERIFY(!lib.folder()->get(i).value("path").toString().startsWith("subsonic:"));

        lib.setRemoteTracks({});                                 // removing the server
        QCOMPARE(lib.songs()->rowCount(), 4);
    }

    void favoritesCanBeToggledWithoutRebuildingTheLists()
    {
        Library lib({"/m"}, "/nonexistent-scanner", "/nonexistent/cache");
        Track a = mk("subsonic:1", "Apple", "Amy", "Beta", 1);
        Track b = mk("subsonic:2", "Berry", "Amy", "Beta", 2);
        lib.setRemoteTracks({a, b});
        QSignalSpy spy(&lib, &Library::starredChanged);
        QSignalSpy index(&lib, &Library::indexChanged);
        QVERIFY(!lib.isStarred("subsonic:1"));
        lib.setStarred("subsonic:2", true);
        lib.setStarred("subsonic:1", true);
        lib.setStarred("subsonic:1", true);                       // no change: no signal
        QCOMPARE(spy.count(), 2);
        QCOMPARE(index.count(), 0);                               // the lists stay as they are
        QVERIFY(lib.isStarred("subsonic:1"));
        QCOMPARE(lib.starredRemotePaths(), QStringList({"subsonic:s1/1", "subsonic:s1/2"}));   // by title
        lib.setStarred("subsonic:2", false);
        QCOMPARE(lib.starredRemotePaths(), QStringList({"subsonic:s1/1"}));
        QVERIFY(lib.tracksForPaths({"subsonic:1"}).first().toMap().value("starred").toBool());
        lib.setStarred("/local/x.flac", true);                    // only server songs can be favorites
        QCOMPARE(spy.count(), 3);
    }

    void eachServerKeepsItsOwnSongsAndCache()
    {
        QTemporaryDir state;
        const QString cache = state.filePath("index.jsonl");
        Library lib({"/m"}, "/nonexistent-scanner", cache);
        lib.setTracks({mk("/m/a.flac", "Local", "Amy", "X", 1)});
        Track one = mk("subsonic:s1/1", "FromOne", "Amy", "Beta", 1);
        Track two = mk("subsonic:s2/1", "FromTwo", "Amy", "Beta", 1);    // the same album name on another server
        lib.setRemoteTracks("s1", {one});
        lib.setRemoteTracks("s2", {two});
        QCOMPARE(lib.songs()->rowCount(), 3);
        QStringList albums;
        for (int i = 0; i < lib.albums()->rowCount(); ++i)
            albums << lib.albums()->get(i).value("title").toString();
        QCOMPARE(albums.count("Beta"), 2);                             // never merged across servers

        lib.writeRemoteCache("s1");
        lib.writeRemoteCache("s2");
        QVERIFY(QFileInfo::exists(state.filePath("subsonic-s1.jsonl")));
        QVERIFY(QFileInfo::exists(state.filePath("subsonic-s2.jsonl")));

        lib.setRemoteTracks("s1", {});                                 // dropping one server leaves the other
        QCOMPARE(lib.songs()->rowCount(), 2);

        Library again({"/m"}, "/nonexistent-scanner", cache);
        again.loadRemoteCache("s2");
        QCOMPARE(again.songs()->rowCount(), 1);
        QCOMPARE(again.songs()->get(0).value("title").toString(), QString("FromTwo"));
    }

    void songsFromBeforeSeveralServersBelongToTheFirstOne()
    {
        QTemporaryDir state;
        const QString cache = state.filePath("index.jsonl");
        {   // the old single cache file, with the old path style
            QFile f(state.filePath("subsonic.jsonl"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(R"({"path":"subsonic:77","title":"Old","artist":"Amy","album":"Beta","cover_id":"c1","starred":true})" "\n");
        }
        Library lib({"/m"}, "/nonexistent-scanner", cache);
        lib.loadRemoteCache("s1");
        QCOMPARE(lib.songs()->rowCount(), 1);
        QCOMPARE(lib.songs()->get(0).value("path").toString(), QString("subsonic:s1/77"));   // upgraded as it was read
        QVERIFY(lib.isStarred("subsonic:77"));                         // and the old name still finds it
        QCOMPARE(lib.tracksForPaths({"subsonic:77"}).size(), 1);       // so old playlists keep working
        QCOMPARE(lib.tracksForPaths({"subsonic:s1/77"}).size(), 1);
        lib.loadRemoteCache("s2");                                     // the old file is not another server's
        QCOMPARE(lib.songs()->rowCount(), 1);
    }

    void serverSongsSurviveARestartThroughTheirOwnCache()
    {
        QTemporaryDir state;
        const QString cache = state.filePath("index.jsonl");
        {
            Library lib({"/m"}, "/nonexistent-scanner", cache);
            Track r = mk("subsonic:7", "Far", "Amy", "Beta", 1);
            r.coverId = "al-1";
            r.starred = true;
            lib.setRemoteTracks({r});
            lib.writeRemoteCache();
            QVERIFY(QFileInfo::exists(state.filePath("subsonic-s1.jsonl")));
            QVERIFY(!QFileInfo::exists(cache));                  // the local index is not touched
        }
        Library again({"/m"}, "/nonexistent-scanner", cache);
        again.loadRemoteCache();
        QCOMPARE(again.songs()->rowCount(), 1);
        QCOMPARE(again.songs()->get(0).value("coverId").toString(), QString("al-1"));
        QVERIFY(again.songs()->get(0).value("starred").toBool());
    }

    void albumRowsDoNotTouchTheDiskForCovers()
    {
        QTemporaryDir dir;
        QFile c(dir.filePath("cover.jpg"));
        QVERIFY(c.open(QIODevice::WriteOnly));
        c.close();
        Library lib({dir.path()}, "/nonexistent-scanner", "/nonexistent/cache");
        const Track t = mk(dir.filePath("x.flac"), "T", "Ar", "Al", 1);
        lib.setTracks({t});
        // The row carries the first track's path; QML resolves the file lazily.
        int role = -1;
        const auto names = lib.albums()->roleNames();
        for (auto it = names.cbegin(); it != names.cend(); ++it) {
            if (it.value() == "cover")
                role = it.key();
        }
        QVERIFY(role >= 0);
        QCOMPARE(lib.albums()->data(lib.albums()->index(0, 0), role).toString(), t.path);
    }

    void changingTheFoldersRescansThem()
    {
        NEED_FFMPEG();
        QTemporaryDir a, b, state;
        QVERIFY(makeFlac(a.filePath("one.flac"), false));
        QVERIFY(makeFlac(b.filePath("two.flac"), false));
        QVERIFY(makeFlac(b.filePath("three.flac"), false));
        Library lib({a.path()}, OMA_SCAN_PROGRAM, state.filePath("index.jsonl"));
        lib.start();
        QTRY_COMPARE_WITH_TIMEOUT(lib.trackCount(), 1, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(!lib.scanning(), 5000);

        lib.setRoots({a.path(), b.path()});
        QTRY_COMPARE_WITH_TIMEOUT(lib.trackCount(), 3, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(!lib.scanning(), 5000);
        lib.setRoots({b.path()});
        QTRY_COMPARE_WITH_TIMEOUT(lib.trackCount(), 2, 20000);
    }

    void tracksForPathsKeepsOrderSkipsUnknownAndNumbersTheInput()
    {
        Library lib({"/m"}, "/nonexistent-scanner", "/nonexistent/cache");
        lib.setTracks(fixture());
        const QVariantList got = lib.tracksForPaths({"/m/top.flac", "/gone.flac", "/m/zed/alpha/01.flac", "/m/top.flac"});
        QCOMPARE(got.size(), 3);
        QCOMPARE(got.at(0).toMap().value("title").toString(), QString("Top"));
        QCOMPARE(got.at(0).toMap().value("playlistIndex").toInt(), 0);
        QCOMPARE(got.at(1).toMap().value("title").toString(), QString("One"));
        QCOMPARE(got.at(1).toMap().value("playlistIndex").toInt(), 2);   // index in the input, gaps kept
        QCOMPARE(got.at(2).toMap().value("playlistIndex").toInt(), 3);
    }

    void songsAreAllTracksSortedByTitle()
    {
        Library lib({"/m"}, "/nonexistent-scanner", "/nonexistent/cache");
        lib.setTracks(fixture());
        QCOMPARE(lib.songs()->rowCount(), 4);
        QStringList titles;
        for (int i = 0; i < 4; ++i)
            titles << lib.songs()->get(i).value("title").toString();
        QCOMPARE(titles, QStringList({"One", "Solo", "Top", "Two"}));
        QCOMPARE(lib.songs()->get(1).value("path").toString(), QString("/m/amy/beta/01.flac"));
    }

    void artistAndAlbumNamesDifferingInCaseAreOne()
    {
        Library lib({"/m"}, "/nonexistent-scanner", "/nonexistent/cache");
        lib.setTracks({
            mk("/m/a/01.flac", "One", "Fit for a King", "Dark Skies", 1),
            mk("/m/a/02.flac", "Two", "Fit for a King", "Dark Skies", 2),
            mk("/m/b/01.flac", "Three", "Fit For A King", "dark skies", 3),
        });
        QCOMPARE(lib.artists()->rowCount(), 1);
        // The most common spelling is shown.
        QCOMPARE(lib.artists()->get(0).value("title").toString(), QString("Fit for a King"));
        QCOMPARE(lib.artists()->get(0).value("subtitle").toString(), QString("1 albums"));
        QCOMPARE(lib.artistTracks("Fit for a King").size(), 3);
        QCOMPARE(lib.albums()->rowCount(), 1);
        QCOMPARE(lib.albums()->get(0).value("subtitle").toString(), QString("Fit for a King"));
        QCOMPARE(lib.albumTracks(lib.albums()->get(0).value("key").toString()).size(), 3);
    }

    void newFilesAppearWithoutAManualRescan()
    {
        NEED_FFMPEG();
        QTemporaryDir music;
        QTemporaryDir state;
        QDir(music.path()).mkdir("A");
        QVERIFY(makeFlac(music.filePath("A/one.flac"), false));
        Library lib({music.path()}, OMA_SCAN_PROGRAM, state.filePath("index.jsonl"));
        lib.setWatchDelay(200);
        lib.start();
        QTRY_COMPARE_WITH_TIMEOUT(lib.trackCount(), 1, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(!lib.scanning(), 5000);
        QVERIFY(lib.watching());

        // A new file in a known folder, then one in a brand new folder.
        QVERIFY(makeFlac(music.filePath("A/two.flac"), false));
        QTRY_COMPARE_WITH_TIMEOUT(lib.trackCount(), 2, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(!lib.scanning(), 5000);
        QDir(music.path()).mkdir("B");
        QVERIFY(makeFlac(music.filePath("B/three.flac"), false));
        QTRY_COMPARE_WITH_TIMEOUT(lib.trackCount(), 3, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(!lib.scanning(), 5000);

        // A file added inside the new folder is seen too: it was watched after the rescan.
        QVERIFY(makeFlac(music.filePath("B/four.flac"), false));
        QTRY_COMPARE_WITH_TIMEOUT(lib.trackCount(), 4, 20000);
    }

    void tooManyDirectoriesFallBackToPolling()
    {
        QTemporaryDir music;
        for (int i = 0; i < 5; ++i)
            QDir(music.path()).mkdir(QString::number(i));
        Library lib({music.path()}, "/nonexistent-scanner", "/nonexistent/cache");
        lib.setMaxWatchedDirs(3);
        lib.start();
        QVERIFY(!lib.watching());
    }

    void embeddedCoverIsExtractedAndCached()
    {
        NEED_FFMPEG();
        QTemporaryDir music;
        QTemporaryDir state;
        const QString track = music.filePath("one.flac");
        QVERIFY(makeFlac(track, true));
        const QString cache = state.filePath("index.jsonl");

        Library lib({music.path()}, "/nonexistent-scanner", cache);
        lib.assumeReachable();
        QSignalSpy spy(&lib, &Library::coversChanged);
        QCOMPARE(lib.coverFor(track), QString());   // first call only queues the extraction
        QCOMPARE(lib.coverRevision(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(!lib.coverFor(track).isEmpty(), 10000);
        const QString url = lib.coverFor(track);
        QVERIFY(url.startsWith("file://"));
        QVERIFY(url.contains("/covers/"));
        QVERIFY(url.endsWith(".jpg"));
        QVERIFY(QFileInfo::exists(QUrl(url).toLocalFile()));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(lib.coverRevision(), 1);

        Library again({music.path()}, "/nonexistent-scanner", cache);
        again.assumeReachable();
        QCOMPARE(again.coverFor(track), url);   // found in the cache, nothing queued
        QVERIFY(!again.extractingCovers());
    }

    void folderImageBeatsEmbeddedArt()
    {
        NEED_FFMPEG();
        QTemporaryDir music;
        QTemporaryDir state;
        const QString track = music.filePath("one.flac");
        QVERIFY(makeFlac(track, true));
        QFile c(music.filePath("cover.jpg"));
        QVERIFY(c.open(QIODevice::WriteOnly));
        c.close();
        Library lib({music.path()}, "/nonexistent-scanner", state.filePath("index.jsonl"));
        lib.assumeReachable();
        QVERIFY(lib.coverFor(track).endsWith("/cover.jpg"));
        QVERIFY(!lib.extractingCovers());
    }

    void trackWithoutArtIsNotRetried()
    {
        NEED_FFMPEG();
        QTemporaryDir music;
        QTemporaryDir state;
        const QString track = music.filePath("plain.flac");
        QVERIFY(makeFlac(track, false));
        Library lib({music.path()}, "/nonexistent-scanner", state.filePath("index.jsonl"));
        QSignalSpy spy(&lib, &Library::coversChanged);
        QCOMPARE(lib.coverFor(track), QString());
        QTRY_VERIFY_WITH_TIMEOUT(!lib.extractingCovers(), 10000);
        QCOMPARE(lib.coverFor(track), QString());
        QVERIFY(!lib.extractingCovers());   // not queued a second time
        QCOMPARE(spy.count(), 0);
        QCOMPARE(lib.coverRevision(), 0);
    }

    void failedScanKeepsOldIndex()
    {
        Library lib({"/m"}, "/nonexistent-scanner", "/nonexistent/cache");
        lib.setTracks(fixture());
        lib.rescan();
        QTest::qWait(200);
        QCOMPARE(lib.trackCount(), 4);
        QVERIFY(!lib.scanning());
    }

    void destroyedWhileScanningKillsScanner()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString script = dir.filePath("slowscan");
        const QString pidFile = dir.filePath("pid");
        QFile f(script);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QStringLiteral("#!/bin/sh\necho $$ > '%1'\nexec sleep 30\n").arg(pidFile).toUtf8());
        f.close();
        f.setPermissions(f.permissions() | QFileDevice::ExeOwner);

        {
            Library lib({dir.path()}, script, dir.filePath("cache"));
            lib.rescan();
            QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(pidFile).size() > 0, 5000);
            QVERIFY(lib.scanning());
        }   // destructor must not crash

        QFile pf(pidFile);
        QVERIFY(pf.open(QIODevice::ReadOnly));
        const qint64 pid = pf.readAll().trimmed().toLongLong();
        QVERIFY(pid > 0);
        QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(QStringLiteral("/proc/%1").arg(pid)), 3000);
    }

    void coverForEmptyPathIsEmpty()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile c(dir.filePath("cover.jpg"));
        QVERIFY(c.open(QIODevice::WriteOnly));
        c.close();
        const QString old = QDir::currentPath();
        QVERIFY(QDir::setCurrent(dir.path()));
        Library lib({"/m"}, "unused", "unused");
        const QString got = lib.coverFor(QString());
        QDir::setCurrent(old);
        QCOMPARE(got, QString());
    }
};

QTEST_GUILESS_MAIN(TstLibrary)
#include "tst_library.moc"
