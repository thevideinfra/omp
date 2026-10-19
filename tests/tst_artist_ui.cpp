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

static Track mk(const QString &path, const QString &title, const QString &album)
{
    Track t;
    t.path = path;
    t.title = title;
    t.artist = "Amy";
    t.albumArtist = "Amy";
    t.album = album;
    t.duration = 60;
    return t;
}

// The artists tab (DrillList in "artist" mode): artists, then the artist's albums, then the open album's songs.
class TstArtistUi : public QObject
{
    Q_OBJECT
    QTemporaryDir dir;
    Settings *settings = nullptr;
    LookScope *look = nullptr;
    Theme *theme = nullptr;
    Library *library = nullptr;
    Playlists *playlists = nullptr;
    UiPlayer player;
    QQmlEngine *engine = nullptr;
    QQuickWindow *window = nullptr;
    QQuickItem *tab = nullptr;

    QVariantList rows() const { return tab->property("rows").toList(); }
    QString title(int i) const { return rows().at(i).toMap().value("title").toString(); }

private slots:
    void initTestCase()
    {
        settings = new Settings(dir.filePath("config.toml"), this);
        look = new LookScope(settings, false, this);
        theme = new Theme(dir.filePath("none.toml"), this);
        library = new Library({"/m"}, "/nonexistent-scanner", "/nonexistent/cache");
        library->setTracks({mk("/m/b1.flac", "B one", "Beta"), mk("/m/b2.flac", "B two", "Beta"),
                            mk("/m/g1.flac", "G one", "Gamma")});
        playlists = new Playlists(dir.filePath("pl"), this);

        engine = new QQmlEngine(this);
        auto *ctx = new QQmlContext(engine->rootContext(), engine);
        ctx->setContextProperty("theme", theme);
        ctx->setContextProperty("ui", look);
        ctx->setContextProperty("player", &player);
        ctx->setContextProperty("library", library);
        ctx->setContextProperty("playlists", playlists);
        QQmlComponent comp(engine, QUrl::fromLocalFile(QStringLiteral(OMA_QML_DIR "/DrillList.qml")));
        QObject *o = comp.create(ctx);
        QVERIFY2(o, qPrintable(comp.errorString()));
        tab = qobject_cast<QQuickItem *>(o);
        window = new QQuickWindow;
        window->resize(1000, 600);
        tab->setParentItem(window->contentItem());
        tab->setSize(QSizeF(1000, 600));
        tab->setProperty("kind", "artist");
        tab->setProperty("model", QVariant::fromValue(library->artists()));
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
    }

    void cleanupTestCase()
    {
        delete window;
        delete engine;
        delete library;
    }

    void theArtistOpensOnItsFirstAlbum()
    {
        QTRY_COMPARE(rows().size(), 2);   // Beta: B one, B two
        QCOMPARE(title(0), QString("B one"));
        QCOMPARE(tab->property("albumIndex").toInt(), 0);
    }

    void pickingAnAlbumShowsItsSongsAndPlayAllPlaysIt()
    {
        QVERIFY(QMetaObject::invokeMethod(tab, "selectAlbum", Q_ARG(QVariant, 1)));   // Gamma
        QCOMPARE(rows().size(), 1);
        QCOMPARE(title(0), QString("G one"));
        QCOMPARE(tab->property("allTracks").toList().size(), 1);
        QVERIFY(QMetaObject::invokeMethod(tab, "selectAlbum", Q_ARG(QVariant, 0)));
        QCOMPARE(rows().size(), 2);
    }

    void rightArrowWalksArtistsAlbumsSongsAndBack()
    {
        QVariant ok;
        QVERIFY(QMetaObject::invokeMethod(tab, "focusRight", Q_RETURN_ARG(QVariant, ok)));
        QVERIFY(ok.toBool());
        QVERIFY(tab->property("midFocus").toBool());
        QVERIFY(QMetaObject::invokeMethod(tab, "move", Q_ARG(QVariant, 1)));   // Gamma
        QTRY_COMPARE(tab->property("albumIndex").toInt(), 1);
        QCOMPARE(rows().size(), 1);
        QVERIFY(QMetaObject::invokeMethod(tab, "focusRight", Q_RETURN_ARG(QVariant, ok)));
        QVERIFY(tab->property("rightFocus").toBool());
        QVERIFY(!tab->property("midFocus").toBool());
        QVERIFY(QMetaObject::invokeMethod(tab, "back", Q_RETURN_ARG(QVariant, ok)));
        QVERIFY(tab->property("midFocus").toBool());
        QVERIFY(QMetaObject::invokeMethod(tab, "activate"));   // Enter on an album plays it
        QCOMPARE(player.played.size(), 1);
        QVERIFY(QMetaObject::invokeMethod(tab, "back", Q_RETURN_ARG(QVariant, ok)));
        QVERIFY(!tab->property("midFocus").toBool());
    }

    void anotherArtistStartsBackOnAllAlbums()
    {
        QVERIFY(QMetaObject::invokeMethod(tab, "selectAlbum", Q_ARG(QVariant, 1)));
        QSignalSpy spy(library, &Library::indexChanged);
        library->setTracks({mk("/m/b1.flac", "B one", "Beta"), mk("/m/b2.flac", "B two", "Beta"),
                            mk("/m/g1.flac", "G one", "Gamma")});
        QTRY_COMPARE(tab->property("albumIndex").toInt(), 0);   // back on the first album
        QCOMPARE(rows().size(), 2);
    }
};

QTEST_MAIN(TstArtistUi)
#include "tst_artist_ui.moc"
