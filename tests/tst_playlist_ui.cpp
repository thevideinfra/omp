#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtTest>

#include "library.h"
#include "lookscope.h"
#include "playlists.h"
#include "settings.h"
#include "theme.h"
#include "track.h"

class UiPlayer : public QObject
{
    Q_OBJECT
public:
    Q_PROPERTY(QVariantMap current MEMBER current NOTIFY currentChanged)
    Q_INVOKABLE void playTracks(const QVariantList &tracks, int start) { played = tracks; playedStart = start; }
    Q_INVOKABLE void enqueueTracks(const QVariantList &tracks) { queued = tracks; }
    QVariantMap current;
    QVariantList played, queued;
    int playedStart = -1;
signals:
    void currentChanged();
};

// The Subsonic hub's playlist calls, recorded.
class StubHub : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE void renamePlaylist(const QString &server, const QString &id, const QString &name) { calls << "rename " + server + " " + id + " " + name; }
    Q_INVOKABLE void deletePlaylist(const QString &server, const QString &id) { calls << "delete " + server + " " + id; }
    Q_INVOKABLE void removeFromPlaylist(const QString &server, const QString &id, int index) { calls << "remove " + server + " " + id + " " + QString::number(index); }
    QStringList calls;
};

static Track mk(const QString &path, const QString &title)
{
    Track t;
    t.path = path;
    t.title = title;
    t.artist = "Amy";
    t.albumArtist = "Amy";
    t.album = "Beta";
    t.duration = 60;
    return t;
}

// The playlist tab (DrillList in "playlist" mode) against real Playlists and Library.
class TstPlaylistUi : public QObject
{
    Q_OBJECT
    QTemporaryDir dir;
    Settings *settings = nullptr;
    LookScope *look = nullptr;
    Theme *theme = nullptr;
    Library *library = nullptr;
    Playlists *playlists = nullptr;
    UiPlayer player;
    StubHub hub;
    QQmlEngine *engine = nullptr;
    QQuickWindow *window = nullptr;
    QQuickItem *tab = nullptr;

    QVariantList rows() const { return tab->property("rows").toList(); }

private slots:
    void initTestCase()
    {
        settings = new Settings(dir.filePath("config.toml"), this);
        look = new LookScope(settings, false, this);
        theme = new Theme(dir.filePath("none.toml"), this);
        library = new Library({"/m"}, "/nonexistent-scanner", "/nonexistent/cache");
        library->setTracks({mk("/m/a.flac", "Aye"), mk("/m/b.flac", "Bee"), mk("/m/c.flac", "Sea")});
        playlists = new Playlists(dir.filePath("pl"), this);
        QVERIFY(playlists->create("Mix"));
        QVERIFY(playlists->addPaths("Mix", {"/m/a.flac", "/m/gone.flac", "/m/b.flac", "/m/c.flac"}));

        engine = new QQmlEngine(this);
        auto *ctx = new QQmlContext(engine->rootContext(), engine);
        ctx->setContextProperty("settingsStore", settings);
        ctx->setContextProperty("theme", theme);
        ctx->setContextProperty("ui", look);
        ctx->setContextProperty("player", &player);
        ctx->setContextProperty("library", library);
        ctx->setContextProperty("playlists", playlists);
        ctx->setContextProperty("subsonic", &hub);
        QQmlComponent comp(engine, QUrl::fromLocalFile(QStringLiteral(OMA_QML_DIR "/DrillList.qml")));
        QObject *o = comp.create(ctx);
        QVERIFY2(o, qPrintable(comp.errorString()));
        tab = qobject_cast<QQuickItem *>(o);
        window = new QQuickWindow;
        window->resize(900, 600);
        tab->setParentItem(window->contentItem());
        tab->setSize(QSizeF(900, 600));
        tab->setProperty("kind", "playlist");
        tab->setProperty("model", QVariant::fromValue(playlists->list()));
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
    }

    void cleanupTestCase()
    {
        delete window;   // owns the tab
        delete engine;   // before the objects its context points at
        delete library;
    }

    void opensThePlaylistAndSkipsMissingSongs()
    {
        QTRY_COMPARE(rows().size(), 3);   // /m/gone.flac is not in the library
        QCOMPARE(rows().at(0).toMap().value("title").toString(), QString("Aye"));
        QCOMPARE(rows().at(0).toMap().value("plIndex").toInt(), 0);
        QCOMPARE(rows().at(1).toMap().value("plIndex").toInt(), 2);   // index in the file, gap kept
    }

    void enterPlaysThePlaylistAndRightEnterPlaysFromTheSong()
    {
        QVERIFY(QMetaObject::invokeMethod(tab, "activate"));
        QCOMPARE(player.played.size(), 3);
        QCOMPARE(player.playedStart, 0);

        QVariant ok;
        QVERIFY(QMetaObject::invokeMethod(tab, "focusRight", Q_RETURN_ARG(QVariant, ok)));
        QVERIFY(ok.toBool());
        QVERIFY(QMetaObject::invokeMethod(tab, "move", Q_ARG(QVariant, 1)));
        QVERIFY(QMetaObject::invokeMethod(tab, "activate"));
        QCOMPARE(player.playedStart, 1);
    }

    void pickAsksForTheSelectedSong()
    {
        QSignalSpy spy(tab, SIGNAL(pickRequested(QVariant)));
        QVERIFY(QMetaObject::invokeMethod(tab, "pick"));
        QCOMPARE(spy.count(), 1);
        const QVariantList t = spy.first().first().value<QVariantList>();
        QCOMPARE(t.size(), 1);
        QCOMPARE(t.first().toMap().value("path").toString(), QString("/m/b.flac"));   // row 1 is selected
    }

    void deleteRemovesTheSelectedSongFromTheFile()
    {
        QVERIFY(QMetaObject::invokeMethod(tab, "removeSelected"));   // row 1 = /m/b.flac
        QCOMPARE(playlists->paths("Mix"), QStringList({"/m/a.flac", "/m/gone.flac", "/m/c.flac"}));
        QTRY_COMPARE(rows().size(), 2);
        QCOMPARE(rows().at(1).toMap().value("title").toString(), QString("Sea"));
        QCOMPARE(rows().at(1).toMap().value("plIndex").toInt(), 2);
    }

    void selectingAnotherPlaylistCancelsRenameAndDelete()
    {
        tab->setProperty("confirmDelete", true);
        tab->setProperty("renaming", true);
        QVERIFY(QMetaObject::invokeMethod(tab, "showRow", Q_ARG(QVariant, 0)));
        QVERIFY(!tab->property("confirmDelete").toBool());
        QVERIFY(!tab->property("renaming").toBool());
    }

    void renamingKeepsThePlaylistSelected()
    {
        QVERIFY(playlists->create("Another"));
        QTRY_COMPARE(playlists->list()->rowCount(), 2);   // Another, Mix
        QVERIFY(QMetaObject::invokeMethod(tab, "showRow", Q_ARG(QVariant, 1)));
        QCOMPARE(tab->property("openTitle").toString(), QString("Mix"));
        QVERIFY(QMetaObject::invokeMethod(tab, "doRename", Q_ARG(QVariant, QString("Zed"))));
        // Zed sorts after Another, Mix sorted after it too, but a rename could reorder: stay on it.
        QTRY_COMPARE(tab->property("openTitle").toString(), QString("Zed"));
        QVERIFY(QMetaObject::invokeMethod(tab, "doRename", Q_ARG(QVariant, QString("Aaa"))));   // now sorts first
        QTRY_COMPARE(tab->property("openTitle").toString(), QString("Aaa"));
        QCOMPARE(rows().size(), 2);   // the songs came with it
    }

    void aNewPlaylistAppearsInTheList()
    {
        QVERIFY(playlists->create("Yet another"));
        QTRY_COMPARE(playlists->list()->rowCount(), 3);
    }

    void anEditableServerPlaylistRenamesDeletesAndDropsSongsOnTheServer()
    {
        Playlists::ServerList mine{"Road", {"/m/a.flac", "/m/b.flac"}, "p7", "s2", "Work", false};
        Playlists::ServerList fav{"Favorites", {"/m/c.flac"}, "", "s2", "Work", true};
        playlists->setServerLists("s2", {mine, fav});
        QTRY_COMPARE(playlists->list()->rowCount(), 5);
        int road = -1;
        for (int i = 0; i < playlists->list()->rowCount(); ++i)
            if (playlists->list()->get(i).value("title").toString() == "Road")
                road = i;
        QVERIFY(road >= 0);
        QVERIFY(QMetaObject::invokeMethod(tab, "showRow", Q_ARG(QVariant, road)));
        QVERIFY(!tab->property("openReadOnly").toBool());              // a server playlist of its own can be edited
        QCOMPARE(tab->property("openServerId").toString(), QString("s2"));
        QCOMPARE(tab->property("openPlaylistId").toString(), QString("p7"));

        QVERIFY(QMetaObject::invokeMethod(tab, "doRename", Q_ARG(QVariant, QString("Highway"))));
        QCOMPARE(hub.calls.last(), QString("rename s2 p7 Highway"));    // sent to the server, not to a local file
        QVariant ok;
        QVERIFY(QMetaObject::invokeMethod(tab, "focusRight", Q_RETURN_ARG(QVariant, ok)));
        QVERIFY(QMetaObject::invokeMethod(tab, "move", Q_ARG(QVariant, 1)));
        QVERIFY(QMetaObject::invokeMethod(tab, "removeSelected"));
        QCOMPARE(hub.calls.last(), QString("remove s2 p7 1"));          // the second song, by its place in the playlist
        QCOMPARE(playlists->paths(playlists->list()->get(road).value("key").toString()).size(), 2);   // nothing local changed

        int fav2 = -1;
        for (int i = 0; i < playlists->list()->rowCount(); ++i)
            if (playlists->list()->get(i).value("title").toString() == "Favorites")
                fav2 = i;
        QVERIFY(QMetaObject::invokeMethod(tab, "showRow", Q_ARG(QVariant, fav2)));
        QVERIFY(tab->property("openReadOnly").toBool());                // Favorites stays read-only
        playlists->setServerLists("s2", {});
    }

    void aServerPlaylistIsReadOnly()
    {
        playlists->setServerLists({{"Road", {"/m/a.flac", "/m/b.flac"}}});   // songs the library knows
        QTRY_COMPARE(playlists->list()->rowCount(), 4);
        QVERIFY(!tab->property("openReadOnly").toBool());                // a local playlist is open
        QVERIFY(QMetaObject::invokeMethod(tab, "showRow", Q_ARG(QVariant, 3)));
        QCOMPARE(tab->property("openTitle").toString(), QString("Road"));
        QVERIFY(tab->property("openReadOnly").toBool());
        QCOMPARE(rows().size(), 2);                                       // its songs are listed and playable

        QVariant ok;
        QVERIFY(QMetaObject::invokeMethod(tab, "focusRight", Q_RETURN_ARG(QVariant, ok)));
        QVERIFY(QMetaObject::invokeMethod(tab, "removeSelected"));        // Delete does nothing here
        QCOMPARE(playlists->paths(playlists->list()->get(3).value("key").toString()).size(), 2);
        playlists->setServerLists({});
    }
};

QTEST_MAIN(TstPlaylistUi)
#include "tst_playlist_ui.moc"
