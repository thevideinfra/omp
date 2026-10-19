#include <QtTest>

#include "trayicon.h"

static const QByteArray kSvg =
    R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><path fill="#dfede5" d="M9 3v11.3A3.5 3.5 0 1 0 11 17.5V7h7V3z"/></svg>)";

class TstTrayIcon : public QObject
{
    Q_OBJECT
private slots:
    void recolorReplacesTheFill()
    {
        const QByteArray out = TrayIcon::recolor(kSvg, QColor("#112233"));
        QVERIFY(out.contains("fill=\"#112233\""));
        QVERIFY(!out.contains("#dfede5"));
    }

    void renderUsesTheColour()
    {
        const QImage img = TrayIcon::render(kSvg, QColor("#ff0000"), 48);
        QCOMPARE(img.size(), QSize(48, 48));
        const QColor px = img.pixelColor(29, 10);   // inside the note's top bar
        QCOMPARE(px.alpha(), 255);
        QCOMPARE(px.red(), 255);
        QCOMPARE(px.green(), 0);
        QCOMPARE(px.blue(), 0);
        QCOMPARE(img.pixelColor(2, 40).alpha(), 0);   // transparent elsewhere
    }
};

QTEST_GUILESS_MAIN(TstTrayIcon)
#include "tst_trayicon.moc"
