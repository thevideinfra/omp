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

static Track mk(const QString &path, const QString &title, const QString &album, int year = 0)
{
    Track t;
    t.path = path;
    t.title = title;
    t.artist = "Amy";
    t.albumArtist = "Amy";
    t.album = album;
    t.duration = 60;
    t.year = year;
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
        library->setTracks({mk("/m/b1.flac", "B one", "Beta", 2020), mk("/m/b2.flac", "B two", "Beta", 2020),
                            mk("/m/g1.flac", "G one", "Gamma", 2010)});
        playlists = new Playlists(dir.filePath("pl"), this);

        engine = new QQmlEngine(this);
        auto *ctx = new QQmlContext(engine->rootContext(), engine);
        ctx->setContextProperty("settingsStore", settings);
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
        tab->setProperty("kind", "artist");
        tab->setSize(QSizeF(300, 600));    // a tiled window starts small, then is resized
        tab->setSize(QSizeF(1000, 600));
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

    void theThreeColumnsStartOutEqual()
    {
        QQuickItem *pane = tab->findChild<QQuickItem *>("listPane");
        QVERIFY(pane);
        QVERIFY2(qAbs(pane->width() - 1000.0 / 3) < 3, qPrintable(QString::number(pane->width())));
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

    void aDraggedDividerSurvivesSettingsChanges()
    {
        QQuickItem *pane = tab->findChild<QQuickItem *>("listPane");
        QVERIFY(pane);
        const qreal before = pane->width();
        const QPoint handle(int(before) + 3, 300);   // the divider right of the artists
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, handle);
        QTest::mouseMove(window, handle + QPoint(15, 0));
        QTest::mouseMove(window, handle + QPoint(25, 0));
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, handle + QPoint(25, 0));
        const qreal dragged = pane->width();
        QVERIFY2(dragged > before + 15, qPrintable(QString("before %1 dragged %2").arg(before).arg(dragged)));

        QMetaObject::invokeMethod(look, "changed");   // what any setting change (shuffle, repeat) emits
        QCOMPARE(pane->width(), dragged);
    }

    void rememberedSizesAreStoredOnlyWhenOnAndResetGoesBackToEqual()
    {
        QQuickItem *pane = tab->findChild<QQuickItem *>("listPane");
        QVERIFY(pane);
        const qreal dragged = pane->width();           // dragged by the test above
        QCOMPARE(settings->layoutFor("artist"), QString());   // the option is off: nothing stored

        settings->set("saveLayout", true);             // switching it on stores the current sizes
        const QStringList f = settings->layoutFor("artist").split(',');
        QCOMPARE(f.size(), 2);
        QVERIFY(qAbs(f.at(0).toDouble() * 1000 - dragged) < 1);

        settings->resetLayout();
        QCOMPARE(settings->layoutFor("artist"), QString());
        QTRY_VERIFY2(qAbs(pane->width() - 1000.0 / 3) < 3, qPrintable(QString::number(pane->width())));
        settings->set("saveLayout", false);
    }

    void aClickOnASortHeaderSortsAscendingThenDescending()
    {
        QCOMPARE(library->artistSort(), QString("title"));
        QVERIFY(QMetaObject::invokeMethod(tab, "sortBy", Q_ARG(QVariant, QString("length"))));
        QCOMPARE(library->artistSort(), QString("length"));
        QVERIFY(!library->artistDescending());
        QVERIFY(QMetaObject::invokeMethod(tab, "sortBy", Q_ARG(QVariant, QString("length"))));
        QVERIFY(library->artistDescending());                       // the same header again: descending
        QVERIFY(QMetaObject::invokeMethod(tab, "sortBy", Q_ARG(QVariant, QString("added"))));
        QCOMPARE(library->artistSort(), QString("added"));
        QVERIFY(!library->artistDescending());                      // another header starts ascending
        QVERIFY(QMetaObject::invokeMethod(tab, "sortBy", Q_ARG(QVariant, QString("title"))));   // put it back
    }

    void theAlbumsColumnSortsByNameAndYear()
    {
        const auto first = [&] { return tab->property("albumGroups").toList().at(0).toMap().value("title").toString(); };
        QCOMPARE(first(), QString("Beta"));                                     // name, ascending, to begin with
        QVERIFY(QMetaObject::invokeMethod(tab, "sortAlbums", Q_ARG(QVariant, QString("year"))));
        QCOMPARE(first(), QString("Gamma"));                                    // 2010 before 2020
        QCOMPARE(tab->property("albumIndex").toInt(), 0);                       // the first album is open again
        QCOMPARE(title(0), QString("G one"));
        QVERIFY(QMetaObject::invokeMethod(tab, "sortAlbums", Q_ARG(QVariant, QString("year"))));
        QCOMPARE(first(), QString("Beta"));                                     // the same header again: newest first
        QVERIFY(QMetaObject::invokeMethod(tab, "sortAlbums", Q_ARG(QVariant, QString("title"))));
        QCOMPARE(first(), QString("Beta"));
        QVERIFY(QMetaObject::invokeMethod(tab, "sortAlbums", Q_ARG(QVariant, QString("title"))));
        QCOMPARE(first(), QString("Gamma"));                                    // name, descending
        QVERIFY(QMetaObject::invokeMethod(tab, "sortAlbums", Q_ARG(QVariant, QString("year"))));
        QVERIFY(QMetaObject::invokeMethod(tab, "sortAlbums", Q_ARG(QVariant, QString("title"))));
        QCOMPARE(first(), QString("Beta"));                                     // a new key starts ascending
    }

    void withoutTheAlbumsColumnTheArtistShowsAllItsAlbumsAndSongs()
    {
        settings->set("artistAlbumsColumn", false);
        QTRY_COMPARE(rows().size(), 5);                               // Beta, its 2 songs, Gamma, its song
        QVERIFY(rows().at(0).toMap().value("header").toBool());
        QCOMPARE(title(0), QString("Beta"));
        QCOMPARE(title(3), QString("Gamma"));
        QCOMPARE(tab->property("allTracks").toList().size(), 3);      // Play all plays everything
        QQuickItem *pane = tab->findChild<QQuickItem *>("listPane");
        QTRY_VERIFY2(qAbs(pane->width() - 500.0) < 20, qPrintable(QString::number(pane->width())));   // two columns now
        QVariant ok;
        QVERIFY(QMetaObject::invokeMethod(tab, "focusRight", Q_RETURN_ARG(QVariant, ok)));
        QVERIFY(tab->property("rightFocus").toBool());                // Right goes straight to the songs
        QVERIFY(!tab->property("midFocus").toBool());
        QVERIFY(QMetaObject::invokeMethod(tab, "back", Q_RETURN_ARG(QVariant, ok)));
        QVERIFY(!tab->property("rightFocus").toBool());
        QVERIFY(!tab->property("midFocus").toBool());                 // and Left goes back to the artists

        settings->set("artistAlbumsColumn", true);
        QTRY_VERIFY(rows().size() < 5);                               // back to one album's songs
        QVERIFY(QMetaObject::invokeMethod(tab, "selectAlbum", Q_ARG(QVariant, 0)));
        QCOMPARE(rows().size(), 2);
    }

    void selectKeyOpensTheArtistWithThatKey()
    {
        QVariant ok;
        QVERIFY(QMetaObject::invokeMethod(tab, "selectKey", Q_RETURN_ARG(QVariant, ok), Q_ARG(QVariant, QString("Amy"))));
        QVERIFY(ok.toBool());
        QCOMPARE(tab->property("openTitle").toString(), QString("Amy"));
        QVERIFY(QMetaObject::invokeMethod(tab, "selectKey", Q_RETURN_ARG(QVariant, ok), Q_ARG(QVariant, QString("Nobody"))));
        QVERIFY(!ok.toBool());
    }

    void anotherArtistStartsBackOnAllAlbums()
    {
        QVERIFY(QMetaObject::invokeMethod(tab, "selectAlbum", Q_ARG(QVariant, 1)));
        QSignalSpy spy(library, &Library::indexChanged);
        library->setTracks({mk("/m/b1.flac", "B one", "Beta", 2020), mk("/m/b2.flac", "B two", "Beta", 2020),
                            mk("/m/g1.flac", "G one", "Gamma", 2010)});
        QTRY_COMPARE(tab->property("albumIndex").toInt(), 0);   // back on the first album
        QCOMPARE(rows().size(), 2);
    }
};

QTEST_MAIN(TstArtistUi)
#include "tst_artist_ui.moc"
