#include <QtTest>

#include "migrate.h"

static void write(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

static QByteArray read(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

class TstMigrate : public QObject
{
    Q_OBJECT
private slots:
    void theOldFoldersAreMovedToTheNewName()
    {
        QTemporaryDir root;
        const QString cfg = root.filePath("config"), data = root.filePath("data"), state = root.filePath("state");
        write(cfg + "/omamusic/config.toml", "volume = 40\n");
        write(data + "/omamusic/playlists/Road.m3u", "#EXTM3U\n/m/a.flac\n");
        write(state + "/omamusic/index.jsonl", "{}\n");

        QVERIFY(Migrate::fromOmamusic(cfg, data, state, "/opt/omp/omp"));
        QCOMPARE(read(cfg + "/omp/config.toml"), QByteArray("volume = 40\n"));
        QCOMPARE(read(data + "/omp/playlists/Road.m3u"), QByteArray("#EXTM3U\n/m/a.flac\n"));
        QCOMPARE(read(state + "/omp/index.jsonl"), QByteArray("{}\n"));
        QVERIFY(!QFileInfo::exists(cfg + "/omamusic"));
        QVERIFY(!QFileInfo::exists(data + "/omamusic"));

        QVERIFY(!Migrate::fromOmamusic(cfg, data, state, "/opt/omp/omp"));   // nothing left to do
    }

    void aNewFolderIsNeverOverwritten()
    {
        QTemporaryDir root;
        const QString cfg = root.filePath("config"), data = root.filePath("data"), state = root.filePath("state");
        write(cfg + "/omamusic/config.toml", "volume = 40\n");
        write(cfg + "/omp/config.toml", "volume = 90\n");
        QVERIFY(!Migrate::fromOmamusic(cfg, data, state, "/opt/omp/omp"));
        QCOMPARE(read(cfg + "/omp/config.toml"), QByteArray("volume = 90\n"));
        QCOMPARE(read(cfg + "/omamusic/config.toml"), QByteArray("volume = 40\n"));   // the old one stays, untouched
    }

    void startAtLoginMovesToTheNewEntry()
    {
        QTemporaryDir root;
        const QString cfg = root.filePath("config"), data = root.filePath("data"), state = root.filePath("state");
        write(cfg + "/autostart/omamusic.desktop", "[Desktop Entry]\nExec=\"/old/omamusic\" tray\n");
        QVERIFY(Migrate::fromOmamusic(cfg, data, state, "/opt/my apps/omp"));
        QVERIFY(!QFileInfo::exists(cfg + "/autostart/omamusic.desktop"));
        const QByteArray text = read(cfg + "/autostart/omp.desktop");
        QVERIFY(text.contains("Name=OMP"));
        QVERIFY(text.contains("Exec=\"/opt/my apps/omp\" tray\n"));
    }

    void nothingToMigrateIsFine()
    {
        QTemporaryDir root;
        QVERIFY(!Migrate::fromOmamusic(root.filePath("c"), root.filePath("d"), root.filePath("s"), "/x/omp"));
    }
};

QTEST_MAIN(TstMigrate)
#include "tst_migrate.moc"
