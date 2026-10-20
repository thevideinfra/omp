#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest>

#include "lookscope.h"
#include "settings.h"
#include "theme.h"

class StubPlayer : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE void playTracks(const QVariantList &t, int start) { played = t; playedStart = start; }
    Q_INVOKABLE void enqueueTracks(const QVariantList &t) { queued = t; }
    Q_INVOKABLE void playNext(const QVariantList &t) { next = t; }
    Q_INVOKABLE void removeAt(int i) { removed << i; }
    Q_PROPERTY(int sleepRemaining MEMBER sleepRemaining NOTIFY sleepChanged)
    Q_PROPERTY(bool sleepAfterSong MEMBER sleepAfterSong NOTIFY sleepChanged)
    Q_INVOKABLE void startSleepTimer(int minutes) { sleepMinutes = minutes; }
    Q_INVOKABLE void sleepAfterCurrentSong() { afterSongAsked = true; }
    Q_INVOKABLE void cancelSleep() { cancelled = true; }
    int sleepRemaining = -1, sleepMinutes = 0;
    bool sleepAfterSong = false, afterSongAsked = false, cancelled = false;
    Q_INVOKABLE void moveRow(int from, int to) { moves << QPoint(from, to); }
    Q_PROPERTY(int queueCount MEMBER queueCount)
    QList<int> removed;
    QList<QPoint> moves;
    int queueCount = 4;
    QVariantList played, queued, next;
    int playedStart = -1;
signals:
    void sleepChanged();
};

class StubLibrary : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int starredRevision MEMBER revision NOTIFY changed)
public:
    Q_INVOKABLE bool isStarred(const QString &path) const { return starred.contains(path); }
    QSet<QString> starred;
    int revision = 0;
signals:
    void changed();
};

class StubPlaylists : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int revision MEMBER revision NOTIFY changed)
public:
    Q_INVOKABLE QStringList names() const { return list; }
    QStringList list{"Chill", "Road"};
    int revision = 0;
signals:
    void changed();
};

class StubSubsonic : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE void toggleStar(const QString &path) { toggled << path; }
    QStringList toggled;
};

class TstContextMenu : public QObject
{
    Q_OBJECT
    QTemporaryDir dir;
    Settings *settings = nullptr;
    LookScope *look = nullptr;
    Theme *theme = nullptr;
    StubPlayer player;
    StubLibrary library;
    StubSubsonic subsonic;
    StubPlaylists playlists;
    QQmlEngine *engine = nullptr;
    QQmlContext *context = nullptr;
    QObject *menu = nullptr;
    QQuickWindow *window = nullptr;

    QObject *entry(const char *name) const { return menu->findChild<QObject *>(name); }
    static QVariantMap song(const QString &path) { return {{"path", path}, {"title", "T"}}; }

private slots:
    void initTestCase()
    {
        settings = new Settings(dir.filePath("config.toml"), this);
        look = new LookScope(settings, false, this);
        theme = new Theme(dir.filePath("none.toml"), this);
        engine = new QQmlEngine(this);
        auto *ctx = context = new QQmlContext(engine->rootContext(), engine);
        ctx->setContextProperty("settingsStore", settings);
        ctx->setContextProperty("theme", theme);
        ctx->setContextProperty("ui", look);
        ctx->setContextProperty("player", &player);
        ctx->setContextProperty("library", &library);
        ctx->setContextProperty("subsonic", &subsonic);
        ctx->setContextProperty("playlists", &playlists);
        // A window to hold the menu, as Main.qml does.
        QQmlComponent holder(engine);
        holder.setData("import QtQuick\nimport QtQuick.Controls\nApplicationWindow { width: 400; height: 300; visible: true }", QUrl());
        window = qobject_cast<QQuickWindow *>(holder.create(ctx));
        QVERIFY(window);
        QQmlComponent comp(engine, QUrl::fromLocalFile(QStringLiteral(OMA_QML_DIR "/SongMenu.qml")));
        menu = comp.create(ctx);
        QVERIFY2(menu, qPrintable(comp.errorString()));
        menu->setParent(window);
        menu->setProperty("parent", QVariant::fromValue(window->contentItem()));
        QVERIFY(QTest::qWaitForWindowExposed(window));
    }

    void cleanupTestCase()
    {
        delete window;
        delete engine;
    }

    void aLocalSongOffersPlayQueueAndPlaylistButNoFavorite()
    {
        menu->setProperty("tracks", QVariantList{song("/m/a.flac")});
        menu->setProperty("canRemove", false);
        QVERIFY(entry("play")->property("enabled").toBool());
        QVERIFY(!entry("favorite")->property("wanted").toBool());
        QVERIFY(!entry("remove")->property("wanted").toBool());

        QVERIFY(QMetaObject::invokeMethod(menu, "play"));
        QCOMPARE(player.played.size(), 1);
        QCOMPARE(player.playedStart, 0);
        QVERIFY(QMetaObject::invokeMethod(menu, "enqueue"));
        QCOMPARE(player.queued.size(), 1);
    }

    void playNextAndGoToArtistAndAlbum()
    {
        menu->setProperty("tracks", QVariantList{song("/m/a.flac")});
        QVERIFY(entry("next")->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(menu, "playNext"));
        QCOMPARE(player.next.size(), 1);

        QVERIFY(entry("artist")->property("wanted").toBool());
        QSignalSpy artist(menu, SIGNAL(goToArtist(QString))), album(menu, SIGNAL(goToAlbum(QString)));
        QVERIFY(QMetaObject::invokeMethod(entry("artist"), "triggered"));
        QVERIFY(QMetaObject::invokeMethod(entry("album"), "triggered"));
        QCOMPARE(artist.at(0).at(0).toString(), QString("/m/a.flac"));
        QCOMPARE(album.at(0).at(0).toString(), QString("/m/a.flac"));

        menu->setProperty("tracks", QVariantList{song("/m/a.flac"), song("/m/b.flac")});   // an album row: no single artist
        QVERIFY(!entry("artist")->property("wanted").toBool());
        QVERIFY(!entry("album")->property("wanted").toBool());
    }

    void addToPlaylistOpensASubmenuOfNewAndExistingPlaylists()
    {
        menu->setProperty("tracks", QVariantList{song("/m/a.flac")});
        QObject *sub = menu->findChild<QObject *>("playlistMenu");
        QVERIFY(sub);
        const auto item = [&](int i) {
            QQuickItem *o = nullptr;
            QMetaObject::invokeMethod(sub, "itemAt", Q_RETURN_ARG(QQuickItem *, o), Q_ARG(int, i));
            return static_cast<QObject *>(o);
        };
        const auto texts = [&] {
            QStringList out;
            for (int i = 0; i < sub->property("count").toInt(); ++i)
                out << (item(i) ? item(i)->property("text").toString() : QString());
            return out;
        };
        // New playlist, a separator, then the local playlists by name.
        QCOMPARE(sub->property("count").toInt(), 4);
        QCOMPARE(texts().value(0), QString("New playlist…"));
        QCOMPARE(texts().value(2), QString("Chill"));
        QCOMPARE(texts().value(3), QString("Road"));

        QSignalSpy created(menu, SIGNAL(newPlaylist(QVariant)));
        QVERIFY(QMetaObject::invokeMethod(item(0), "triggered"));
        QCOMPARE(created.count(), 1);
        QCOMPARE(created.at(0).at(0).value<QVariantList>().size(), 1);

        QSignalSpy added(menu, SIGNAL(addToPlaylistNamed(QString, QVariant)));
        QVERIFY(QMetaObject::invokeMethod(item(3), "triggered"));
        QCOMPARE(added.count(), 1);
        QCOMPARE(added.at(0).at(0).toString(), QString("Road"));
        QCOMPARE(added.at(0).at(1).value<QVariantList>().size(), 1);

        playlists.list << "Zed";                                 // a playlist made later shows up
        ++playlists.revision;
        emit playlists.changed();
        QTRY_COMPARE(sub->property("count").toInt(), 5);
        QCOMPARE(texts().value(4), QString("Zed"));
        playlists.list.removeLast();
        ++playlists.revision;
        emit playlists.changed();
        QTRY_COMPARE(sub->property("count").toInt(), 4);
    }

    void aSingleServerSongCanBeFavoritedOrUnfavorited()
    {
        menu->setProperty("tracks", QVariantList{song("subsonic:7")});
        QVERIFY(entry("favorite")->property("wanted").toBool());
        QCOMPARE(entry("favorite")->property("text").toString(), QString("Add to favorites"));
        QVERIFY(QMetaObject::invokeMethod(menu, "toggleFavorite"));
        QCOMPARE(subsonic.toggled, QStringList({"subsonic:7"}));

        library.starred.insert("subsonic:7");
        ++library.revision;
        emit library.changed();
        QCOMPARE(entry("favorite")->property("text").toString(), QString("Remove from favorites"));
    }

    void severalSongsHaveNoFavoriteAndAPlaylistRowCanBeRemoved()
    {
        menu->setProperty("tracks", QVariantList{song("subsonic:1"), song("subsonic:2")});
        QVERIFY(!entry("favorite")->property("wanted").toBool());

        menu->setProperty("canRemove", true);
        QVERIFY(entry("remove")->property("wanted").toBool());
        QSignalSpy removed(menu, SIGNAL(removeFromPlaylist()));
        QVERIFY(QMetaObject::invokeMethod(entry("remove"), "triggered"));
        QCOMPARE(removed.count(), 1);
    }

    void queueRowsCanBeRemovedAndMoved()
    {
        menu->setProperty("tracks", QVariantList{song("/m/a.flac")});
        menu->setProperty("queueRow", -1);
        QVERIFY(!entry("removeQueue")->property("wanted").toBool());   // only for a row of the Queue tab
        menu->setProperty("queueRow", 2);
        menu->setProperty("queueCount", 4);
        QVERIFY(entry("removeQueue")->property("wanted").toBool());
        QVERIFY(entry("moveUp")->property("enabled").toBool());
        QVERIFY(entry("moveDown")->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(entry("removeQueue"), "triggered"));
        QCOMPARE(player.removed, QList<int>({2}));
        QVERIFY(QMetaObject::invokeMethod(entry("moveUp"), "triggered"));
        QVERIFY(QMetaObject::invokeMethod(entry("moveDown"), "triggered"));
        QCOMPARE(player.moves, QList<QPoint>({QPoint(2, 1), QPoint(2, 3)}));

        menu->setProperty("queueRow", 0);
        QVERIFY(!entry("moveUp")->property("enabled").toBool());       // the first cannot go up
        menu->setProperty("queueRow", 3);
        QVERIFY(!entry("moveDown")->property("enabled").toBool());     // nor the last down
        menu->setProperty("queueRow", -1);
    }

    void theSleepMenuStartsAndCancelsTheTimer()
    {
        QQmlComponent comp(engine, QUrl::fromLocalFile(QStringLiteral(OMA_QML_DIR "/SleepMenu.qml")));
        QObject *sleep = comp.create(context);
        QVERIFY2(sleep, qPrintable(comp.errorString()));
        sleep->setParent(window);
        sleep->setProperty("parent", QVariant::fromValue(window->contentItem()));
        const auto item = [&](int i) {
            QQuickItem *o = nullptr;
            QMetaObject::invokeMethod(sleep, "itemAt", Q_RETURN_ARG(QQuickItem *, o), Q_ARG(int, i));
            return static_cast<QObject *>(o);
        };
        QCOMPARE(sleep->property("count").toInt(), 7);              // five times, after this song, cancel
        QVERIFY(!sleep->property("active").toBool());
        QVERIFY(!item(6)->property("wanted").toBool());              // nothing to cancel yet

        QVERIFY(QMetaObject::invokeMethod(item(1), "triggered"));    // 30 minutes
        QCOMPARE(player.sleepMinutes, 30);
        QVERIFY(QMetaObject::invokeMethod(item(5), "triggered"));
        QVERIFY(player.afterSongAsked);

        player.sleepRemaining = 600;                                 // a timer is running
        emit player.sleepChanged();
        QTRY_VERIFY(sleep->property("active").toBool());
        QVERIFY(item(6)->property("wanted").toBool());
        QVERIFY(QMetaObject::invokeMethod(item(6), "triggered"));
        QVERIFY(player.cancelled);
        delete sleep;
    }

    void aMusicFolderRowCanBeRemovedFromTheLibrary()
    {
        menu->setProperty("tracks", QVariantList{});
        menu->setProperty("folderPath", QString());
        QVERIFY(!entry("removeFolder")->property("wanted").toBool());
        menu->setProperty("folderPath", QString("/music/more"));
        QVERIFY(entry("removeFolder")->property("wanted").toBool());
        QVERIFY(!entry("play")->property("enabled").toBool());          // an empty folder has nothing to play
        QSignalSpy removed(menu, SIGNAL(removeFolder(QString)));
        QVERIFY(QMetaObject::invokeMethod(entry("removeFolder"), "triggered"));
        QCOMPARE(removed.at(0).at(0).toString(), QString("/music/more"));
        menu->setProperty("folderPath", QString());
    }

    void escapeClosesItAndItOpensWhereAsked()
    {
        menu->setProperty("tracks", QVariantList{song("/m/a.flac")});
        QVERIFY(QMetaObject::invokeMethod(menu, "openAt", Q_ARG(QVariant, 120), Q_ARG(QVariant, 80)));
        QTRY_VERIFY(menu->property("visible").toBool());
        QCOMPARE(menu->property("x").toInt(), 120);
        QCOMPARE(menu->property("y").toInt(), 80);
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!menu->property("visible").toBool());
    }
};

QTEST_MAIN(TstContextMenu)
#include "tst_context_menu.moc"
