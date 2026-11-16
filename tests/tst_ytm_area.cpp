#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtTest>

#include "library.h"
#include "lookscope.h"
#include "playlists.h"
#include "secrets.h"
#include "settings.h"
#include "theme.h"
#include "track.h"
#include "ytmsource.h"

class AreaPlayer : public QObject
{
    Q_OBJECT
public:
    Q_PROPERTY(QVariantMap current MEMBER current NOTIFY currentChanged)
    Q_INVOKABLE void playTracks(const QVariantList &tracks, int start) { played = tracks; playedStart = start; }
    Q_INVOKABLE void enqueueTracks(const QVariantList &tracks) { queued = tracks; }
    Q_INVOKABLE void enqueue(const QVariantMap &track) { queued = {track}; }
    QVariantMap current;
    QVariantList played, queued;
    int playedStart = -1;
signals:
    void currentChanged();
};

static Track mk(const QString &path, const QString &title, const QString &artist, const QString &album, int n, int year = 0)
{
    Track t;
    t.path = path;
    t.title = title;
    t.artist = artist;
    t.albumArtist = artist;
    t.album = album;
    t.track = n;
    t.year = year;
    t.duration = 60;
    return t;
}

// The YouTube Music area: songs, albums and playlists of that source, with the window's search box filtering them.
class TstYtmArea : public QObject
{
    Q_OBJECT
    QTemporaryDir dir;
    Settings *settings = nullptr;
    LookScope *look = nullptr;
    Theme *theme = nullptr;
    Library *library = nullptr;
    Playlists *playlists = nullptr;
    AreaPlayer player;
    Secrets *secrets = nullptr;
    YtmSource *ytm = nullptr;
    QQmlEngine *engine = nullptr;
    QQuickWindow *window = nullptr;
    QQuickItem *area = nullptr;

private slots:
    void initTestCase()
    {
        settings = new Settings(dir.filePath("config.toml"), this);
        look = new LookScope(settings, false, this);
        theme = new Theme(dir.filePath("none.toml"), this);
        library = new Library({"/m"}, "/nonexistent-scanner", "/nonexistent/cache");
        playlists = new Playlists(dir.filePath("pl"), this);
        secrets = new Secrets(QStringLiteral("/nonexistent-secret-tool"), this);
        ytm = new YtmSource(*library, *playlists, *secrets, QStringLiteral("/nonexistent-helper"), this);

        engine = new QQmlEngine(this);
        auto *ctx = new QQmlContext(engine->rootContext(), engine);
        ctx->setContextProperty("settingsStore", settings);
        ctx->setContextProperty("theme", theme);
        ctx->setContextProperty("ui", look);
        ctx->setContextProperty("player", &player);
        ctx->setContextProperty("library", library);
        ctx->setContextProperty("playlists", playlists);
        ctx->setContextProperty("ytm", ytm);
        QQmlComponent comp(engine, QUrl::fromLocalFile(QStringLiteral(OMA_QML_DIR "/YtmArea.qml")));
        QObject *o = comp.create(ctx);
        QVERIFY2(o, qPrintable(comp.errorString()));
        area = qobject_cast<QQuickItem *>(o);
        window = new QQuickWindow;
        window->resize(1000, 600);
        area->setParentItem(window->contentItem());
        area->setSize(QSizeF(1000, 600));
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
        // The library fills after the window is up, as it does in the app (cache, then sync).
        library->setTracks({mk("/m/a.flac", "Local", "Amy", "Beta", 1)});
        library->setRemoteTracks("ytm", {mk("ytm:a", "Alpha", "Zed", "Gamma", 1, 2019), mk("ytm:b", "Bravo", "Zed", "Gamma", 2),
                                         mk("ytm:c", "Charlie", "Yan", "Omega", 1)});
        library->setSeparateSources({"ytm"});
        playlists->setServerLists("ytm", {{"Mix", {"ytm:c", "ytm:a"}, "PL1", "ytm", "YouTube Music"}});

    }

    void cleanupTestCase()
    {
        delete window;
        delete engine;
        delete library;
    }

    void itOpensOnTheLibraryAndHandsTheKeyboardTheListItShows()
    {
        QCOMPARE(area->property("section").toString(), QString("Library"));
        QCOMPARE(library->separateSongs()->rowCount(), 3);
        const auto list = [&] { return area->property("current").value<QObject *>(); };
        QVERIFY(list());
        QCOMPARE(list()->property("model").value<QObject *>(), qobject_cast<QObject *>(library->separateSongs()));
        area->setProperty("section", "Artists");
        QTRY_COMPARE(list()->property("model").value<QObject *>(), qobject_cast<QObject *>(library->separateArtists()));
        area->setProperty("section", "Albums");
        QTRY_COMPARE(list()->property("model").value<QObject *>(), qobject_cast<QObject *>(library->separateAlbums()));
        area->setProperty("section", "Playlists");
        QTRY_COMPARE(list()->property("model").value<QObject *>(), qobject_cast<QObject *>(playlists->listOf("ytm")));
        area->setProperty("section", "Search");
        QTRY_COMPARE(list()->property("model").value<QObject *>(), qobject_cast<QObject *>(ytm->searchSongs()));   // the keyboard works on the songs found
        area->setProperty("section", "Library");
    }

    void aHeaderClickSortsTheSongsAndTheSameOneAgainTurnsItRound()
    {
        QVERIFY(QMetaObject::invokeMethod(area, "sortBy", Q_ARG(QVariant, QString("artist"))));
        QCOMPARE(library->separateSort(), QString("artist"));
        QCOMPARE(library->separateSongs()->get(0).value("artist").toString(), QString("Yan"));
        QVERIFY(QMetaObject::invokeMethod(area, "sortBy", Q_ARG(QVariant, QString("artist"))));
        QVERIFY(library->separateDescending());
        QCOMPARE(library->separateSongs()->get(0).value("artist").toString(), QString("Zed"));
        library->setSeparateSort(QString());
        library->setSeparateDescending(false);
    }

    void anAlbumOfTheAreaOpensWithItsSongsAndPlaysFromTheFirst()
    {
        area->setProperty("section", "Albums");
        auto *list = qobject_cast<QQuickItem *>(area->property("current").value<QObject *>());
        QVERIFY(list);
        QTRY_COMPARE(list->property("rows").toList().size(), 2);              // Gamma: Alpha, Bravo
        QCOMPARE(list->property("rows").toList().at(0).toMap().value("title").toString(), QString("Alpha"));
        QCOMPARE(list->property("allTracks").toList().size(), 2);
        area->setProperty("section", "Library");
    }

    void anArtistOfTheAreaOpensOnItsFirstAlbumAndShowsItsSongs()
    {
        area->setProperty("section", "Artists");
        auto *list = qobject_cast<QQuickItem *>(area->property("current").value<QObject *>());
        QVERIFY(list);
        QTRY_COMPARE(list->property("albumGroups").toList().size(), 1);        // Yan: Omega
        QCOMPARE(list->property("rows").toList().at(0).toMap().value("title").toString(), QString("Charlie"));
        QCOMPARE(list->property("source").toString(), QString("ytm"));
        area->setProperty("section", "Library");
    }

    void aPlaylistOfTheAreaIsReadOnlyAndHasNoNewButton()
    {
        area->setProperty("section", "Playlists");
        auto *list = qobject_cast<QQuickItem *>(area->property("current").value<QObject *>());
        QTRY_COMPARE(list->property("rows").toList().size(), 2);              // Mix: Charlie, Alpha
        QVERIFY(list->property("openReadOnly").toBool());
        QVERIFY(!list->property("allowNew").toBool());
        area->setProperty("section", "Library");
    }
};

QTEST_MAIN(TstYtmArea)
#include "tst_ytm_area.moc"
