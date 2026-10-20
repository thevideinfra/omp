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

    void serverPlaylistsListAfterTheLocalOnesAndStayReadOnly()
    {
        QTemporaryDir dir;
        Playlists p(dir.path());
        QVERIFY(p.create("Zed local"));
        p.setServerLists({{"Road", {"subsonic:1", "subsonic:2"}}, {"Favorites", {"subsonic:2"}}});
        QCOMPARE(p.list()->rowCount(), 3);
        QCOMPARE(p.list()->get(0).value("title").toString(), QString("Zed local"));
        QVERIFY(!p.list()->get(0).value("readOnly").toBool());
        QCOMPARE(p.list()->get(1).value("title").toString(), QString("Road"));
        QVERIFY(p.list()->get(1).value("readOnly").toBool());
        QCOMPARE(p.list()->get(1).value("subtitle").toString(), QString("2 songs"));
        QCOMPARE(p.list()->get(1).value("meta").toString(), QString("server"));
        QCOMPARE(p.list()->get(0).value("meta").toString(), QString("local"));
        const QString key = p.list()->get(1).value("key").toString();
        QCOMPARE(p.paths(key), QStringList({"subsonic:1", "subsonic:2"}));
        QCOMPARE(p.names(), QStringList({"Zed local"}));          // the picker offers local playlists only

        QVERIFY(!p.remove(key));                                 // none of the edits touch a server list
        QVERIFY(!p.rename(key, "Other"));
        QVERIFY(!p.addPaths(key, {"/m/a.flac"}));
        QVERIFY(!p.removeAt(key, 0));
        QCOMPARE(p.paths(key).size(), 2);

        QVERIFY(p.create("Another"));                            // a local change keeps the server rows
        QCOMPARE(p.list()->rowCount(), 4);
        p.setServerLists({});
        QCOMPARE(p.list()->rowCount(), 2);
    }

    void eachServerHasItsOwnPlaylistsEvenWithTheSameName()
    {
        QTemporaryDir dir;
        Playlists p(dir.path());
        QVERIFY(p.create("Mine"));
        p.setServerLists("s1", {{"Road", {"subsonic:s1/1"}, "p1", "s1", "Home"}});
        p.setServerLists("s2", {{"Road", {"subsonic:s2/9", "subsonic:s2/8"}, "p7", "s2", "Work"}, {"Chill", {}, "p8", "s2", "Work"}});
        QCOMPARE(p.list()->rowCount(), 4);                          // Mine, then each server's lists
        QCOMPARE(p.list()->get(1).value("meta").toString(), QString("Home"));
        QCOMPARE(p.list()->get(2).value("meta").toString(), QString("Work"));
        const QString k1 = p.list()->get(1).value("key").toString(), k2 = p.list()->get(2).value("key").toString();
        QVERIFY(k1 != k2);                                         // the same name, two playlists
        QCOMPARE(p.paths(k1), QStringList({"subsonic:s1/1"}));
        QCOMPARE(p.paths(k2), QStringList({"subsonic:s2/9", "subsonic:s2/8"}));
        QCOMPARE(p.list()->get(2).value("serverId").toString(), QString("s2"));
        QCOMPARE(p.list()->get(2).value("playlistId").toString(), QString("p7"));
        QCOMPARE(p.namesFor("subsonic:s2/9"), QStringList({"Road"}));

        p.setServerLists("s1", {});                                // dropping a server keeps the other's lists
        QCOMPARE(p.list()->rowCount(), 3);
        QCOMPARE(p.list()->get(1).value("serverId").toString(), QString("s2"));
    }

    void editableServerPlaylistsAreListedForAddingSongsTo()
    {
        QTemporaryDir dir;
        Playlists p(dir.path());
        Playlists::ServerList mine{"Road", {"subsonic:s1/1"}, "p1", "s1", "Home", false};
        Playlists::ServerList fav{"Favorites", {"subsonic:s1/2"}, "", "s1", "Home", true};
        Playlists::ServerList other{"Mix", {}, "p9", "s2", "Work", false};
        p.setServerLists("s1", {mine, fav});
        p.setServerLists("s2", {other});
        const QVariantList e = p.editableServerLists();
        QCOMPARE(e.size(), 2);                                       // Favorites is not one of them
        QCOMPARE(e.at(0).toMap().value("name").toString(), QString("Road"));
        QCOMPARE(e.at(0).toMap().value("serverId").toString(), QString("s1"));
        QCOMPARE(e.at(0).toMap().value("playlistId").toString(), QString("p1"));
        QCOMPARE(e.at(0).toMap().value("source").toString(), QString("Home"));
        QCOMPARE(e.at(1).toMap().value("serverId").toString(), QString("s2"));
        // The row of an editable list says so.
        bool sawEditable = false;
        for (int i = 0; i < p.list()->rowCount(); ++i)
            if (p.list()->get(i).value("title").toString() == "Road")
                sawEditable = !p.list()->get(i).value("readOnly").toBool();
        QVERIFY(sawEditable);
    }

    void aSongKnowsWhichPlaylistsItIsIn()
    {
        QTemporaryDir dir;
        Playlists p(dir.path());
        QVERIFY(p.create("Road"));
        QVERIFY(p.create("Chill"));
        QVERIFY(p.addPaths("Road", {"/m/a.flac", "/m/b.flac"}));
        QVERIFY(p.addPaths("Chill", {"/m/a.flac"}));
        p.setServerLists({{"Navidrome mix", {"subsonic:1", "/m/a.flac"}}});
        QCOMPARE(p.namesFor("/m/a.flac"), QStringList({"Chill", "Road", "Navidrome mix"}));
        QCOMPARE(p.namesFor("/m/b.flac"), QStringList({"Road"}));
        QCOMPARE(p.namesFor("subsonic:1"), QStringList({"Navidrome mix"}));
        QCOMPARE(p.namesFor("/m/none.flac"), QStringList());
        const int before = p.revision();
        QVERIFY(p.removeAt("Road", 0));
        QVERIFY(p.revision() > before);                       // lists bound to it update
        QCOMPARE(p.namesFor("/m/a.flac"), QStringList({"Chill", "Navidrome mix"}));
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
