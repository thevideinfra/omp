#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest>

#include "lookscope.h"
#include "rowsmodel.h"
#include "settings.h"
#include "theme.h"

class StubLibrary : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int coverRevision MEMBER rev NOTIFY changed)
    Q_PROPERTY(int starredRevision MEMBER rev NOTIFY changed)
public:
    Q_INVOKABLE QString coverFor(const QString &) const { return {}; }
    Q_INVOKABLE bool isStarred(const QString &) const { return false; }
    Q_INVOKABLE QVariantMap trackInfo(const QString &) const { return {}; }
    int rev = 0;
signals:
    void changed();
};

class StubPlaylists : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int revision MEMBER rev NOTIFY changed)
public:
    Q_INVOKABLE QStringList namesFor(const QString &) const { return {}; }
    int rev = 0;
signals:
    void changed();
};

// The song table: header clicks ask for a sort, right-clicks ask for the column menu.
class TstBrowseTable : public QObject
{
    Q_OBJECT
    QTemporaryDir dir;
    Settings *settings = nullptr;
    LookScope *look = nullptr;
    Theme *theme = nullptr;
    StubLibrary library;
    StubPlaylists playlists;
    RowsModel *model = nullptr;
    QQmlEngine *engine = nullptr;
    QQuickWindow *window = nullptr;
    QQuickItem *list = nullptr;

    // The header cell for a column id, as a point in the window.
    QPoint headerPoint(const QString &id) const
    {
        const QVariantList cols = list->property("columnModel").toList();
        for (const QVariant &c : cols) {
            const QVariantMap m = c.toMap();
            if (m.value("id").toString() == id)
                return QPoint(int(m.value("x").toDouble() + 8), 12);
        }
        return {};
    }

private slots:
    void initTestCase()
    {
        settings = new Settings(dir.filePath("config.toml"), this);
        look = new LookScope(settings, false, this);
        theme = new Theme(dir.filePath("none.toml"), this);
        model = new RowsModel({"title", "subtitle", "meta", "cover"}, this);
        model->setRows({{{"title", "T"}, {"subtitle", "A — B"}, {"meta", "1:00"}, {"cover", "/m/a.flac"}}});
        engine = new QQmlEngine(this);
        auto *ctx = new QQmlContext(engine->rootContext(), engine);
        ctx->setContextProperty("theme", theme);
        ctx->setContextProperty("ui", look);
        ctx->setContextProperty("settingsStore", settings);
        ctx->setContextProperty("library", &library);
        ctx->setContextProperty("playlists", &playlists);
        QQmlComponent comp(engine, QUrl::fromLocalFile(QStringLiteral(OMA_QML_DIR "/BrowseList.qml")));
        QObject *o = comp.create(ctx);
        QVERIFY2(o, qPrintable(comp.errorString()));
        list = qobject_cast<QQuickItem *>(o);
        window = new QQuickWindow;
        window->resize(1000, 300);
        list->setParentItem(window->contentItem());
        list->setSize(QSizeF(1000, 300));
        list->setProperty("table", true);
        list->setProperty("model", QVariant::fromValue(model));
        list->setProperty("columns", QVariantList{"title", "artist", "album", "favorite", "time"});
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
    }

    void cleanupTestCase()
    {
        delete window;
        delete engine;
    }

    void aHeaderClickAsksForThatSortWhenTheListIsSortable()
    {
        QSignalSpy sort(list, SIGNAL(sortRequested(QString)));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, headerPoint("artist"));
        QCOMPARE(sort.count(), 0);                                 // not sortable yet (Queue, Search, Folders)

        list->setProperty("sortable", true);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, headerPoint("artist"));
        QCOMPARE(sort.count(), 1);
        QCOMPARE(sort.at(0).at(0).toString(), QString("artist"));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, headerPoint("favorite"));
        QCOMPARE(sort.at(1).at(0).toString(), QString("favorite"));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, headerPoint("time"));
        QCOMPARE(sort.at(2).at(0).toString(), QString("length"));   // Time sorts by length
    }

    void aSortHeaderStripAsksForTheClickedSort()
    {
        QQmlComponent comp(engine, QUrl::fromLocalFile(QStringLiteral(OMA_QML_DIR "/SortHeader.qml")));
        QObject *o = comp.create(engine->rootContext());
        QVERIFY2(o, qPrintable(comp.errorString()));
        auto *strip = qobject_cast<QQuickItem *>(o);
        strip->setParentItem(window->contentItem());
        strip->setY(200);
        strip->setSize(QSizeF(400, 30));
        strip->setProperty("options", QVariantList{QVariantList{"title", "Title"}, QVariantList{"artist", "Artist"},
                                                   QVariantList{"length", "Length"}, QVariantList{"added", "Added"}});
        QSignalSpy sort(strip, SIGNAL(sortRequested(QString)));
        QTest::qWait(50);   // let the row lay out its cells
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(150, 215));   // the second of four cells
        QCOMPARE(sort.count(), 1);
        QCOMPARE(sort.at(0).at(0).toString(), QString("artist"));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(390, 215));
        QCOMPARE(sort.at(1).at(0).toString(), QString("added"));
        delete strip;
    }

    void aRightClickOnTheHeaderAsksForTheColumnMenu()
    {
        QSignalSpy menu(list, SIGNAL(columnsRequested(qreal, qreal)));
        QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, headerPoint("album"));
        QCOMPARE(menu.count(), 1);
        QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, QPoint(990, 12));   // empty header space too
        QCOMPARE(menu.count(), 2);
    }
};

QTEST_MAIN(TstBrowseTable)
#include "tst_browse_table.moc"
