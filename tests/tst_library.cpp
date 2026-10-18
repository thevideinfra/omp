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
        QVERIFY(lib.coverFor(dir.filePath("x.flac")).endsWith("/Cover.JPG"));
        QTemporaryDir bare;
        QCOMPARE(lib.coverFor(bare.filePath("x.flac")), QString());
    }

    void scanFillsIndexAndCache()
    {
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
};

QTEST_GUILESS_MAIN(TstLibrary)
#include "tst_library.moc"
