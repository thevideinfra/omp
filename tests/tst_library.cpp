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
