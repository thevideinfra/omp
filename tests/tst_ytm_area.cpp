#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtTest>

#include <QCryptographicHash>
#include <QImage>
#include <QPainter>

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
        library = new Library({"/m"}, "/nonexistent-scanner", dir.filePath("index.jsonl"));
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

    // With OMP_SHOT_DIR set, the area is drawn in each of its states into PNG files there (to look at by eye).
    void drawsItselfIntoPicturesWhenAsked()
    {
        const QString out = qEnvironmentVariable("OMP_SHOT_DIR");
        if (out.isEmpty())
            QSKIP("set OMP_SHOT_DIR to take pictures");
        QDir().mkpath(out);
        QDir().mkpath(dir.filePath("covers"));
        const QStringList artists{"Daft Punk", "Gorillaz", "Tame Impala", "Khruangbin", "Bonobo", "Four Tet"};
        const QStringList albums{"Discovery", "Demon Days", "Currents", "Con Todo El Mundo", "Migration", "There Is Love In You",
                                 "Random Access Memories", "Plastic Beach", "Lonerism", "Mordechai", "Black Sands", "Rounds"};
        QVector<Track> tracks;
        for (int i = 0; i < albums.size(); ++i) {
            for (int n = 1; n <= 4; ++n) {
                Track t = mk(QStringLiteral("ytm:shot%1_%2").arg(i).arg(n), QStringLiteral("Song %1 of %2").arg(n).arg(albums[i]),
                             artists[i % artists.size()], albums[i], n, 2001 + i);
                t.coverId = QStringLiteral("https://example.org/cover%1").arg(i);
                t.duration = 180 + n * 17;
                tracks.append(t);
            }
            QImage img(300, 300, QImage::Format_RGB32);
            QPainter p(&img);
            QLinearGradient g(0, 0, 300, 300);
            g.setColorAt(0, QColor::fromHsv((i * 47) % 360, 200, 230));
            g.setColorAt(1, QColor::fromHsv((i * 47 + 60) % 360, 220, 90));
            p.fillRect(img.rect(), g);
            p.setPen(Qt::white);
            p.setFont(QFont("sans", 40, QFont::Bold));
            p.drawText(img.rect(), Qt::AlignCenter, QString::number(i + 1));
            p.end();
            const QByteArray hex = QCryptographicHash::hash(QStringLiteral("ytm/https://example.org/cover%1").arg(i).toUtf8(), QCryptographicHash::Sha1).toHex();
            img.save(dir.filePath("covers/remote-") + QString::fromLatin1(hex) + ".jpg");
        }
        library->setRemoteTracks("ytm", tracks);
        playlists->setServerLists("ytm", {{"Road trip", {"ytm:shot0_1", "ytm:shot1_2", "ytm:shot2_3"}, "PL1", "ytm", "YouTube Music", false},
                                          {"Chill", {"ytm:shot3_1", "ytm:shot4_2"}, "PL2", "ytm", "YouTube Music", false}});
        const auto shot = [&](const QString &name) {
            QTest::qWait(700);
            QVERIFY(window->grabWindow().save(out + "/" + name + ".png"));
        };
        window->setColor(QColor("#141917"));
        window->setColor(theme->background());
        window->resize(1100, 760);
        area->setSize(QSizeF(1100, 760));
        QVector<QVariantMap> heard;
        for (int i = 0; i < 8; ++i)
            heard.append({{"title", QStringLiteral("Song %1 of %2").arg(1 + i % 4).arg(albums[(i * 3) % albums.size()])},
                          {"subtitle", artists[i % artists.size()]}, {"cover", QStringLiteral("ytm:shot%1_1").arg((i * 3) % albums.size())},
                          {"path", QStringLiteral("ytm:shot%1_1").arg((i * 3) % albums.size())}});
        ytm->history()->setRows(heard);
        area->setProperty("section", "Home");
        shot("0-home");
        area->setProperty("section", "Library");
        area->setProperty("view", "Songs");
        shot("1-songs");
        area->setProperty("view", "Artists");
        shot("2-artists");
        area->setProperty("view", "Albums");
        shot("3-albums");
        area->setProperty("view", "Playlists");
        shot("4-playlists");
        const QString albumKey = library->separateAlbums()->get(0).value("key").toString();
        QVERIFY(QMetaObject::invokeMethod(area, "openAlbum", Q_ARG(QVariant, albumKey), Q_ARG(QVariant, library->separateAlbums()->get(0).value("title")),
                                          Q_ARG(QVariant, library->separateAlbums()->get(0).value("subtitle")), Q_ARG(QVariant, library->separateAlbums()->get(0).value("cover"))));
        shot("5-album-page");
        QVERIFY(QMetaObject::invokeMethod(area, "back"));
        const QVariantMap artist = library->separateArtists()->get(0);
        QVERIFY(QMetaObject::invokeMethod(area, "openArtist", Q_ARG(QVariant, artist.value("key")), Q_ARG(QVariant, artist.value("title")), Q_ARG(QVariant, artist.value("cover"))));
        shot("6-artist-page");
        {   // YouTube's own page for the artist arrives
            QVariantList cards, tops;
            for (int i = 0; i < 5; ++i)
                cards << QVariantMap{{"key", QStringLiteral("MP%1").arg(i)}, {"title", albums[i]}, {"subtitle", QString::number(2001 + i)},
                                     {"meta", "Album"}, {"cover", QStringLiteral("ytm:shot%1_1").arg(i)}};
            for (int i = 0; i < 6; ++i)
                tops << QVariantMap{{"path", QStringLiteral("ytm:shot%1_%2").arg(i).arg(1 + i % 4)}, {"title", QStringLiteral("Top song %1").arg(i + 1)},
                                    {"artist", artist.value("title")}, {"album", albums[i]}, {"duration", 200 + i * 11}, {"meta", "3:20"}};
            QVariantMap pg = area->property("page").toMap();
            QVERIFY(QMetaObject::invokeMethod(area, "push", Q_ARG(QVariant, QVariant(QVariantMap{{"kind", "artist"}, {"key", "x"}, {"title", artist.value("title")},
                                                {"cover", artist.value("cover")}, {"artistId", "UCx"}, {"tracks", QVariantList{}}, {"albums", QVariantList{}}}))));
            emit ytm->artistFetched("UCx", {{"name", "x"}, {"tracks", tops}, {"albums", cards}, {"singles", cards.mid(0, 2)}});
            shot("6b-artist-youtube");
            QVERIFY(QMetaObject::invokeMethod(area, "back"));
        }
        QVERIFY(QMetaObject::invokeMethod(area, "back"));
        const QVariantMap list = playlists->listOf("ytm")->get(0);
        QVERIFY(QMetaObject::invokeMethod(area, "openPlaylist", Q_ARG(QVariant, list.value("key")), Q_ARG(QVariant, list.value("title")), Q_ARG(QVariant, list.value("playlistId")),
                                          Q_ARG(QVariant, true), Q_ARG(QVariant, QString("ytm:shot0_1"))));
        shot("7-playlist-page");
        QVERIFY(QMetaObject::invokeMethod(area, "back"));
        area->setProperty("section", "Search");
        shot("8-search");
        area->setProperty("section", "Library");
        area->setProperty("view", "Songs");
        library->setRemoteTracks("ytm", {});
        playlists->setServerLists("ytm", {});
        ytm->history()->setRows({});
    }

    void itOpensOnTheLibraryAndHandsTheKeyboardTheListItShows()
    {
        QCOMPARE(area->property("section").toString(), QString("Home"));                // it opens on the Home page
        QVERIFY(area->property("current").value<QObject *>()->property("empty").isValid());
        area->setProperty("section", "Library");
        QCOMPARE(area->property("view").toString(), QString("Songs"));
        QCOMPARE(library->separateSongs()->rowCount(), 3);
        const auto model = [&] { return area->property("current").value<QObject *>()->property("model").value<QObject *>(); };
        QVERIFY(area->property("current").value<QObject *>());
        QCOMPARE(model(), qobject_cast<QObject *>(library->separateSongs()));
        area->setProperty("view", "Artists");
        QTRY_COMPARE(model(), qobject_cast<QObject *>(library->separateArtists()));
        area->setProperty("view", "Albums");
        QTRY_COMPARE(model(), qobject_cast<QObject *>(library->separateAlbums()));
        area->setProperty("view", "Playlists");
        QTRY_COMPARE(model(), qobject_cast<QObject *>(playlists->listOf("ytm")));
        area->setProperty("section", "Search");
        QTRY_COMPARE(model(), qobject_cast<QObject *>(ytm->searchSongs()));   // the keyboard works on the songs found
        area->setProperty("section", "Library");
        area->setProperty("view", "Songs");
    }

    void theHomePageShowsRowsOnlyForWhatThereIsAndSeeAllGoesToTheLibrary()
    {
        area->setProperty("section", "Home");
        QObject *home = area->property("current").value<QObject *>();
        QVERIFY(home);
        QVERIFY(!home->property("empty").toBool());                                       // the library has albums, artists and a playlist
        ytm->history()->setRows({QVariantMap{{"title", "Heard"}, {"subtitle", "Zed — Gamma"}, {"cover", "ytm:a"}, {"path", "ytm:a"}}});
        QCOMPARE(ytm->history()->rowCount(), 1);
        QSignalSpy played(home, SIGNAL(songsPlayed(QVariant, int)));
        const QVariant one = QVariantList{QVariantMap{{"path", "ytm:a"}}};
        QVERIFY(QMetaObject::invokeMethod(home, "songsPlayed", Q_ARG(QVariant, one), Q_ARG(int, 0)));
        QCOMPARE(played.count(), 1);
        QCOMPARE(player.played.size(), 1);                                                // a song of a row is played
        QVERIFY(QMetaObject::invokeMethod(home, "seeAll", Q_ARG(QString, QString("Albums"))));
        QCOMPARE(area->property("section").toString(), QString("Library"));
        QCOMPARE(area->property("view").toString(), QString("Albums"));
        ytm->history()->setRows({});
        area->setProperty("view", "Songs");
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

    void anAlbumOpensAsAPageWithItsSongsAndBackGoesBack()
    {
        QCOMPARE(area->property("pages").toList().size(), 0);
        const QString key = library->separateAlbums()->get(0).value("key").toString();         // Gamma: Alpha, Bravo
        QVERIFY(QMetaObject::invokeMethod(area, "openAlbum", Q_ARG(QVariant, key), Q_ARG(QVariant, QString("Gamma")),
                                          Q_ARG(QVariant, QString("Zed  ·  2019")), Q_ARG(QVariant, QString("ytm:a"))));
        QCOMPARE(area->property("pages").toList().size(), 1);
        const QVariantMap page = area->property("page").toMap();
        QCOMPARE(page.value("kind").toString(), QString("album"));
        QCOMPARE(page.value("title").toString(), QString("Gamma"));
        QCOMPARE(page.value("tracks").toList().size(), 2);
        QCOMPARE(page.value("meta").toString(), QString("2 songs  ·  2 min"));
        auto *list = qobject_cast<QQuickItem *>(area->property("current").value<QObject *>());
        QVERIFY(list);
        QTRY_COMPARE(list->property("count").toInt(), 2);                          // the page's own song list is what the keyboard works on
        QVariant back;
        QVERIFY(QMetaObject::invokeMethod(area, "back", Q_RETURN_ARG(QVariant, back)));
        QVERIFY(back.toBool());
        QCOMPARE(area->property("pages").toList().size(), 0);
        QVERIFY(QMetaObject::invokeMethod(area, "back", Q_RETURN_ARG(QVariant, back)));
        QVERIFY(!back.toBool());                                                   // nothing to go back to: the key does something else
    }

    void anArtistPageHasItsSongsAndAShelfOfItsAlbums()
    {
        QVERIFY(QMetaObject::invokeMethod(area, "openArtist", Q_ARG(QVariant, QString("Zed")), Q_ARG(QVariant, QString("Zed")),
                                          Q_ARG(QVariant, QString("ytm:a"))));
        const QVariantMap page = area->property("page").toMap();
        QCOMPARE(page.value("kind").toString(), QString("artist"));
        QCOMPARE(page.value("tracks").toList().size(), 2);
        QCOMPARE(page.value("albums").toList().size(), 1);                         // Gamma
        QCOMPARE(page.value("albums").toList().at(0).toMap().value("title").toString(), QString("Gamma"));
        QCOMPARE(page.value("albums").toList().at(0).toMap().value("subtitle").toString(), QString("2019"));
        QCOMPARE(page.value("subtitle").toString(), QString("1 albums"));
        // an album of the shelf opens on top of the artist, and back returns to the artist
        QVERIFY(QMetaObject::invokeMethod(area, "openAlbum", Q_ARG(QVariant, page.value("albums").toList().at(0).toMap().value("key")),
                                          Q_ARG(QVariant, QString("Gamma")), Q_ARG(QVariant, QString()), Q_ARG(QVariant, QString("ytm:a"))));
        QCOMPARE(area->property("pages").toList().size(), 2);
        QVERIFY(QMetaObject::invokeMethod(area, "back"));
        QCOMPARE(area->property("page").toMap().value("kind").toString(), QString("artist"));
        QVERIFY(QMetaObject::invokeMethod(area, "back"));
        QCOMPARE(area->property("pages").toList().size(), 0);
    }

    void youtubesOwnArtistPageReplacesTheLibraryOnesWhenItArrivesAndItsAlbumsOpenFromYoutube()
    {
        const QVariant start = QVariantMap{{"kind", "artist"}, {"key", "Zed"}, {"title", "Zed"},
                                           {"artistId", "UCzed"}, {"tracks", QVariantList{}}, {"albums", QVariantList{}}};
        QVERIFY(QMetaObject::invokeMethod(area, "push", Q_ARG(QVariant, start)));
        QCOMPARE(area->property("pages").toList().size(), 1);
        const QVariantMap card{{"key", "MPREb_x"}, {"title", "Elsewhere"}, {"subtitle", "2020"}, {"meta", "Album"}, {"cover", "ytm:album:MPREb_x"}};
        const QVariantMap top{{"path", "ytm:t1"}, {"title", "Top One"}, {"artist", "Zed"}, {"album", "Elsewhere"}, {"duration", 200}};
        emit ytm->artistFetched("UCother", {{"name", "Someone"}, {"tracks", QVariantList{top}}, {"albums", QVariantList{card}}, {"singles", QVariantList{}}});
        QVERIFY(!area->property("page").toMap().value("youtube").toBool());                  // another artist's answer is ignored
        emit ytm->artistFetched("UCzed", {{"name", "Zed"}, {"tracks", QVariantList{top}}, {"albums", QVariantList{card}}, {"singles", QVariantList{card}}});
        const QVariantMap page = area->property("page").toMap();
        QVERIFY(page.value("youtube").toBool());
        QVERIFY(page.value("top").toBool());                                                 // the songs are the top songs now
        QCOMPARE(page.value("tracks").toList().size(), 1);
        QCOMPARE(page.value("albums").toList().size(), 1);
        QCOMPARE(page.value("singles").toList().size(), 1);
        QCOMPARE(area->property("pages").toList().size(), 1);                                // replaced in place, not added
        // an album of YouTube's shelf: its songs are read, then the page opens on top
        QVERIFY(QMetaObject::invokeMethod(area, "openYoutubeAlbum", Q_ARG(QVariant, QString("MPREb_x")), Q_ARG(QVariant, QString("Elsewhere")),
                                          Q_ARG(QVariant, QString("2020")), Q_ARG(QVariant, QString("ytm:album:MPREb_x"))));
        QCOMPARE(area->property("pages").toList().size(), 1);                                // not before the songs arrive
        emit ytm->albumFetched("MPREb_x", "open", QVariantList{top});
        QCOMPARE(area->property("pages").toList().size(), 2);
        QCOMPARE(area->property("page").toMap().value("kind").toString(), QString("album"));
        QCOMPARE(area->property("page").toMap().value("albumId").toString(), QString("MPREb_x"));
        QVERIFY(QMetaObject::invokeMethod(area, "back"));
        QVERIFY(QMetaObject::invokeMethod(area, "back"));
        QCOMPARE(area->property("pages").toList().size(), 0);
    }

    void aPlaylistPageIsReadOnlyWhenItIsNotYoursAndListsItsSongsInOrder()
    {
        const QString key = playlists->listOf("ytm")->get(0).value("key").toString();
        QVERIFY(QMetaObject::invokeMethod(area, "openPlaylist", Q_ARG(QVariant, key), Q_ARG(QVariant, QString("Mix")), Q_ARG(QVariant, QString("PL1")),
                                          Q_ARG(QVariant, false), Q_ARG(QVariant, QString("ytm:c"))));
        const QVariantMap page = area->property("page").toMap();
        QCOMPARE(page.value("kind").toString(), QString("playlist"));
        QVERIFY(!page.value("editable").toBool());
        auto *list = qobject_cast<QQuickItem *>(area->property("current").value<QObject *>());
        QTRY_COMPARE(list->property("count").toInt(), 2);                          // Mix: Charlie, Alpha
        QVERIFY(QMetaObject::invokeMethod(area, "back"));
    }
};

QTEST_MAIN(TstYtmArea)
#include "tst_ytm_area.moc"
