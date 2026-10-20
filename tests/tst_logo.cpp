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
        // The logo is 955 x 592 on its grid, so in a 400 box it is 400 x 248 and starts at y = 76.
        const double k = 400.0 / 955.0;
        const auto at = [&](double x, double y) { return img.pixelColor(int(x * k), int(76 + y * k)); };
        const QColor stroke = at(40, 300);               // the thick left bar of the first letter
        QVERIFY2(stroke.red() > 240 && stroke.green() < 20 && stroke.blue() < 20, qPrintable(stroke.name()));
        const QColor gap = at(500, 560);                 // empty space under the middle of the M
        QVERIFY2(gap.red() < 20, qPrintable(gap.name()));
        QVERIFY(img.pixelColor(200, 20).red() < 20);     // nothing above the centred mark
        QVERIFY(img.pixelColor(200, 390).red() < 20);    // or below it

        logo->setProperty("ink", QColor("#00ff00"));     // a theme change recolours it
        QTest::qWait(300);
        const QColor green = window.grabWindow().pixelColor(int(40 * k), int(76 + 300 * k));
        QVERIFY2(green.green() > 240 && green.red() < 20, qPrintable(green.name()));
    }
};

QTEST_MAIN(TstLogo)
#include "tst_logo.moc"
