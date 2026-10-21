#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest>

#include "lookscope.h"
#include "settings.h"
#include "theme.h"

class TstLogo : public QObject
{
    Q_OBJECT
private slots:
    void theMarkIsDrawnInTheGivenColourAndStaysCentred()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath("config.toml"));
        LookScope look(&settings, false);
        Theme theme(dir.filePath("none.toml"));
        QQmlEngine engine;
        auto *ctx = new QQmlContext(engine.rootContext(), &engine);
        ctx->setContextProperty("theme", &theme);
        ctx->setContextProperty("ui", &look);
        QQmlComponent comp(&engine, QUrl::fromLocalFile(QStringLiteral(OMA_QML_DIR "/LogoMark.qml")));
        QObject *o = comp.create(ctx);
        QVERIFY2(o, qPrintable(comp.errorString()));
        auto *logo = qobject_cast<QQuickItem *>(o);
        QQuickWindow window;
        window.setColor(Qt::black);
        window.resize(400, 400);
        logo->setParentItem(window.contentItem());
        logo->setProperty("ink", QColor("#ff0000"));
        logo->setSize(QSizeF(400, 400));               // a square box: the mark keeps its shape and sits in the middle
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(300);                              // the canvas paints after the first frame

        const QImage img = window.grabWindow();
        // The logo is 190 x 82 on its grid, so in a 400 box it is 400 x 173 and starts at y = 114.
        const double k = 400.0 / 190;
        const auto at = [&](double x, double y) { return img.pixelColor(int(x * k), int(114 + y * k)); };
        const QColor stroke = at(6, 26);                // the left side of the o
        QVERIFY2(stroke.red() > 240 && stroke.green() < 20 && stroke.blue() < 20, qPrintable(stroke.name()));
        const QColor gap = at(26, 26);                  // the hole in the middle of the o
        QVERIFY2(gap.red() < 20, qPrintable(gap.name()));
        QVERIFY(img.pixelColor(200, 103).red() < 20);     // nothing above the centred mark
        QVERIFY(img.pixelColor(200, 296).red() < 20);    // or below it

        logo->setProperty("ink", QColor("#00ff00"));     // a theme change recolours it
        QTest::qWait(300);
        const QColor green = window.grabWindow().pixelColor(int(6 * k), int(114 + 26 * k));
        QVERIFY2(green.green() > 240 && green.red() < 20, qPrintable(green.name()));
    }
};

QTEST_MAIN(TstLogo)
#include "tst_logo.moc"
