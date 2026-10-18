#include <QtTest>

#include <QElapsedTimer>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>

#include "lookscope.h"
#include "rowsmodel.h"
#include "settings.h"
#include "theme.h"
#include "track.h"

// Stand-ins for the context objects Panel.qml uses.
class StubPlayer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double position MEMBER position CONSTANT)
    Q_PROPERTY(double duration MEMBER duration CONSTANT)
    Q_PROPERTY(int volume MEMBER volume NOTIFY volumeChanged)
    Q_PROPERTY(bool playing MEMBER playing CONSTANT)
    Q_PROPERTY(int currentIndex MEMBER index NOTIFY currentIndexChanged)
    Q_PROPERTY(QVariantMap current MEMBER current NOTIFY currentChanged)
    Q_PROPERTY(RowsModel *queue READ queueModel CONSTANT)
    Q_PROPERTY(QString error MEMBER error CONSTANT)
public:
    StubPlayer() : model(Track::roleNames()) {}
    RowsModel *queueModel() { return &model; }
    Q_INVOKABLE void togglePause() {}
    Q_INVOKABLE void next() {}
    Q_INVOKABLE void previous() {}
    Q_INVOKABLE void seekTo(double) {}
    Q_INVOKABLE void clearQueue() {}
    double position = 10, duration = 100;
    int volume = 50, index = 0;
    bool playing = true;
    QVariantMap current;
    QString error;
    RowsModel model;
signals:
    void volumeChanged();
    void currentIndexChanged();
    void currentChanged();
};

class StubLibrary : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE QString coverFor(const QString &) { return {}; }
};

class StubShell : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE void showMain() {}
    Q_INVOKABLE void showSettings() {}
    Q_INVOKABLE void hidePanel() {}
};

static QVector<QVariantMap> makeRows(int n, const QString &title)
{
    QVector<QVariantMap> rows;
    for (int i = 0; i < n; ++i) {
        Track t;
        t.path = QStringLiteral("/m/%1.flac").arg(i);
        t.title = title.isEmpty() ? QStringLiteral("Track %1").arg(i) : title;
        t.artist = QStringLiteral("Artist %1").arg(i);
        t.album = QStringLiteral("Album");
        QVariantMap m = t.toMap();
        m.insert("subtitle", t.artist);
        rows << m;
    }
    return rows;
}

class TstPanelHeight : public QObject
{
    Q_OBJECT

    QTemporaryDir dir;
    Settings *settings = nullptr;
    LookScope *look = nullptr;
    Theme *theme = nullptr;
    StubPlayer player;
    StubLibrary library;
    StubShell shell;
    QQmlEngine *engine = nullptr;
    QQuickWindow *panel = nullptr;

    QQuickItem *column() const
    {
        auto *body = panel->findChild<QQuickItem *>("panelBody");
        return body ? body->childItems().value(0) : nullptr;
    }
    double real(const char *name) const { return panel->property(name).toDouble(); }

    void configure(const QString &density, const QString &size, const QString &family, bool cover,
                   bool seek, bool volume, bool upNext, int count)
    {
        look->set("density", density);
        look->set("fontSize", size);
        look->set("fontFamily", family);
        look->set("panelCover", cover);
        look->set("panelSeek", seek);
        look->set("panelVolume", volume);
        look->set("panelUpNext", upNext);
        look->set("panelUpNextCount", count);
    }

    int upNextDelegates() const
    {
        int n = 0;
        for (QQuickItem *c : column()->childItems())
            if (c->property("row").isValid())
                ++n;
        return n;
    }

    // Bottom edge of the laid-out column: last visible child with a size.
    double columnBottom() const
    {
        double b = 0;
        for (QQuickItem *c : column()->childItems())
            if (c->isVisible() && c->height() > 0)
                b = qMax(b, c->y() + c->height());
        return b;
    }

    void checkShown(double hiddenWanted)
    {
        panel->setVisible(true);
        QVERIFY(QTest::qWaitForWindowExposed(panel));
        QTest::qWait(100);
        QVERIFY(panel->height() > 0);
        const double max = real("maxHeight");
        if (hiddenWanted < max)
            QCOMPARE(double(panel->height()), hiddenWanted);          // (a)
        QVERIFY2(qAbs(columnBottom() - real("contentHeight")) < 0.5,   // (b) zero drift
                 qPrintable(QStringLiteral("column %1 vs contentHeight %2")
                                .arg(columnBottom()).arg(real("contentHeight"))));
        auto *footer = panel->findChild<QQuickItem *>("panelFooter");
        QVERIFY(footer);
        const double bottom = footer->mapToItem(panel->contentItem(), QPointF(0, footer->height())).y();
        QVERIFY2(bottom <= panel->height() + 0.01, "footer inside window");
        panel->setVisible(false);
        QTest::qWait(30);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir.isValid());
        settings = new Settings(dir.filePath("config.toml"), this);
        look = new LookScope(settings, true, this);
        theme = new Theme(dir.filePath("none.toml"), this);
        engine = new QQmlEngine(this);
        auto *ctx = new QQmlContext(engine->rootContext(), engine);
        ctx->setContextProperty("theme", theme);
        ctx->setContextProperty("ui", look);
        ctx->setContextProperty("player", &player);
        ctx->setContextProperty("library", &library);
        ctx->setContextProperty("shell", &shell);
        QQmlComponent comp(engine, QUrl::fromLocalFile(QStringLiteral(OMA_QML_DIR "/Panel.qml")));
        QObject *o = comp.create(ctx);
        QVERIFY2(o, qPrintable(comp.errorString()));
        panel = qobject_cast<QQuickWindow *>(o);
        QVERIFY(panel);
        player.model.setRows(makeRows(20, {}));
        player.current = player.model.get(0);
    }

    void heightMatchesLayout_data()
    {
        QTest::addColumn<QString>("density");
        QTest::addColumn<QString>("size");
        QTest::addColumn<QString>("family");
        QTest::addColumn<bool>("cover");
        QTest::addColumn<bool>("sections");
        QTest::addColumn<int>("count");
        QTest::addColumn<int>("rows");
        QTest::addColumn<QString>("title");
        const QString n = "normal", e;
        QTest::newRow("default, 20 rows") << n << n << e << true << true << 10 << 20 << e;
        QTest::newRow("cover off") << n << n << e << false << true << 10 << 20 << e;
        QTest::newRow("count 0") << n << n << e << true << true << 0 << 20 << e;
        QTest::newRow("count 4") << n << n << e << true << true << 4 << 20 << e;
        QTest::newRow("sections off") << n << n << e << false << false << 4 << 20 << e;
        QTest::newRow("compact small") << QString("compact") << QString("small") << e << false << true << 4 << 20 << e;
        QTest::newRow("comfortable large") << QString("comfortable") << QString("large") << e << true << true << 4 << 20 << e;
        QTest::newRow("font family") << n << n << QString("DejaVu Sans") << false << true << 4 << 20 << e;
        QTest::newRow("long titles") << n << n << e << false << true << 4 << 20
                                     << QString("A very long track title that certainly has to be elided to fit the narrow panel width");
        QTest::newRow("cjk titles") << n << n << e << false << true << 4 << 20 << QString::fromUtf8("日本語のとても長いタイトルです");
        QTest::newRow("emoji titles") << n << n << e << false << true << 4 << 20 << QString::fromUtf8("🎵🎶 music 🎧");
        QTest::newRow("empty queue") << n << n << e << false << true << 10 << 0 << e;
        QTest::newRow("one row") << n << n << e << false << true << 10 << 1 << e;
        QTest::newRow("5000 rows") << n << n << e << false << true << 10 << 5000 << e;
    }

    void heightMatchesLayout()
    {
        QFETCH(QString, density);
        QFETCH(QString, size);
        QFETCH(QString, family);
        QFETCH(bool, cover);
        QFETCH(bool, sections);
        QFETCH(int, count);
        QFETCH(int, rows);
        QFETCH(QString, title);

        QVERIFY(!panel->isVisible());
        QElapsedTimer timer;
        timer.start();
        configure(density, size, family, cover, sections, sections, sections, count);
        player.index = 0;
        player.model.setRows(makeRows(rows, title));
        player.current = rows ? player.model.get(0) : QVariantMap();
        emit player.currentIndexChanged();
        emit player.currentChanged();
        QTest::qWait(30);
        if (rows >= 1000)
            qInfo() << rows << "rows: set queue" << timer.elapsed() << "ms";
        QVERIFY2(timer.elapsed() < 2000, "changing a large queue stays fast");

        QVERIFY(upNextDelegates() <= 10);
        const double hiddenWanted = real("wantedHeight");
        QVERIFY(hiddenWanted >= 120 && hiddenWanted <= 680);
        checkShown(hiddenWanted);
    }

    void changesWhileHiddenAreSeenAtOnce()
    {
        configure("normal", "normal", "", true, true, true, true, 10);
        player.model.setRows(makeRows(20, {}));
        QTest::qWait(30);
        const double a = real("wantedHeight");
        configure("normal", "normal", "", false, true, true, true, 2);
        QTest::qWait(30);
        const double b = real("wantedHeight");
        QVERIFY2(b < a, "fewer rows and no cover make the panel shorter while hidden");
        checkShown(b);
    }

    void freezeContract()
    {
        configure("normal", "normal", "", true, true, true, true, 4);
        player.model.setRows(makeRows(20, {}));
        QTest::qWait(30);
        QVERIFY(QMetaObject::invokeMethod(panel, "freeze"));
        const int frozen = panel->property("frozenHeight").toInt();
        QCOMPARE(frozen, int(real("wantedHeight")));
        panel->setVisible(true);
        QVERIFY(QTest::qWaitForWindowExposed(panel));
        QTest::qWait(50);
        const int h = panel->height();
        QCOMPARE(h, frozen);
        player.model.setRows(makeRows(1, {}));
        look->set("panelCover", false);
        look->set("panelUpNextCount", 1);
        QTest::qWait(80);
        QVERIFY(real("wantedHeight") != h);
        QCOMPARE(panel->height(), h);
        panel->setVisible(false);
        QTest::qWait(30);
        QCOMPARE(panel->property("frozenHeight").toInt(), 0);
        QCOMPARE(double(panel->height()), real("wantedHeight"));
    }

    void largeQueueIsCheap()
    {
        configure("normal", "normal", "", true, true, true, true, 10);
        player.model.setRows(makeRows(5000, {}));
        QTest::qWait(30);
        QElapsedTimer t;
        t.start();
        player.index = 100;
        emit player.currentIndexChanged();
        look->set("fontSize", "large");
        QTest::qWait(30);
        qInfo() << "5000 rows: play + font change" << t.elapsed() << "ms";
        QVERIFY2(t.elapsed() < 2000, "play + font change on 5000 rows");
        QVERIFY(upNextDelegates() <= 10);
        t.restart();
        player.model.setRows({});
        player.index = 0;
        emit player.currentIndexChanged();
        QTest::qWait(30);
        qInfo() << "5000 rows: clear" << t.elapsed() << "ms";
        QVERIFY2(t.elapsed() < 2000, "clearing 5000 rows");
        QCOMPARE(upNextDelegates(), 0);
    }
};

QTEST_MAIN(TstPanelHeight)
#include "tst_panel_height.moc"
