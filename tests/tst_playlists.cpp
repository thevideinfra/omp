#include <QtTest>

#include "playlists.h"

class TstPlaylists : public QObject
{
    Q_OBJECT
private slots:
    void createListRenameRemove()
    {
        QTemporaryDir dir;
        Playlists p(dir.filePath("pl"));          // the directory does not exist yet
        QCOMPARE(p.list()->rowCount(), 0);
        QSignalSpy spy(&p, &Playlists::changed);

        QVERIFY(p.create("Road trip"));
        QVERIFY(!p.create("Road trip"));          // exists
        QVERIFY(!p.create("..."));                // nothing usable left
        QVERIFY(p.create("Chill"));
        QCOMPARE(p.names(), QStringList({"Chill", "Road trip"}));
        QCOMPARE(p.list()->rowCount(), 2);
        QCOMPARE(p.list()->get(0).value("title").toString(), QString("Chill"));
        QCOMPARE(p.list()->get(0).value("subtitle").toString(), QString("0 songs"));
        QVERIFY(spy.count() >= 2);

        QVERIFY(p.rename("Chill", "Evening"));
        QVERIFY(!p.rename("Evening", "Road trip"));   // taken
        QCOMPARE(p.names(), QStringList({"Evening", "Road trip"}));
        QVERIFY(p.remove("Evening"));
        QVERIFY(!p.remove("Evening"));
        QCOMPARE(p.names(), QStringList({"Road trip"}));
    }

    void addAndRemoveSongsKeepsOrderAndSurvivesReload()
    {
        QTemporaryDir dir;
        Playlists p(dir.path());
        QVERIFY(p.create("Mix"));
        QVERIFY(p.addPaths("Mix", {"/m/a.flac", "/m/b flac.flac"}));
        QVERIFY(p.addPaths("Mix", {"/m/a.flac"}));          // a song may appear twice
        QVERIFY(!p.addPaths("Missing", {"/m/x.flac"}));
        QCOMPARE(p.paths("Mix"), QStringList({"/m/a.flac", "/m/b flac.flac", "/m/a.flac"}));
        QCOMPARE(p.list()->get(0).value("subtitle").toString(), QString("3 songs"));

        QVERIFY(p.removeAt("Mix", 0));
        QVERIFY(!p.removeAt("Mix", 5));
        QCOMPARE(p.paths("Mix"), QStringList({"/m/b flac.flac", "/m/a.flac"}));

        Playlists again(dir.path());
        QCOMPARE(again.paths("Mix"), QStringList({"/m/b flac.flac", "/m/a.flac"}));
        QCOMPARE(again.list()->rowCount(), 1);
    }

    void extendedM3uLinesSurviveEdits()
    {
        QTemporaryDir dir;
        Playlists p(dir.path());
        QFile f(dir.filePath("Imported.m3u"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("#EXTM3U\n#EXTINF:200,A - One\n/m/one.flac\n#EXTINF:100,B - Two\n/m/two.flac\n");
        f.close();
        QCOMPARE(p.paths("Imported"), QStringList({"/m/one.flac", "/m/two.flac"}));

        QVERIFY(p.addPaths("Imported", {"/m/three.flac"}));
        QVERIFY(p.removeAt("Imported", 0));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("#EXTM3U\n#EXTINF:100,B - Two\n/m/two.flac\n/m/three.flac\n"));
    }

    void createWithPicksAUniqueName()
    {
        QTemporaryDir dir;
        Playlists p(dir.path());
        QCOMPARE(p.createWith("Queue", {"/m/a.flac"}), QString("Queue"));
        QCOMPARE(p.createWith("Queue", {"/m/b.flac"}), QString("Queue (2)"));
        QCOMPARE(p.createWith("   ", {}), QString());
        QCOMPARE(p.paths("Queue (2)"), QStringList({"/m/b.flac"}));
    }

    void namesAreMadeSafeForFiles()
    {
        QCOMPARE(Playlists::cleanName("  a/b\\c "), QString("a-b-c"));
        QCOMPARE(Playlists::cleanName("..hidden"), QString("hidden"));
        QCOMPARE(Playlists::cleanName("..."), QString());
    }
};

QTEST_GUILESS_MAIN(TstPlaylists)
#include "tst_playlists.moc"
