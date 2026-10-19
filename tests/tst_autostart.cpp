#include <QtTest>

#include "autostart.h"

class TstAutostart : public QObject
{
    Q_OBJECT
private slots:
    void writesAndRemovesTheEntry()
    {
        QTemporaryDir dir;
        const QString autostartDir = dir.filePath("config/autostart");   // does not exist yet
        Autostart a(autostartDir, "/opt/my apps/omamusic");
        QVERIFY(!a.enabled());

        QSignalSpy spy(&a, &Autostart::changed);
        a.setEnabled(true);
        QVERIFY(a.enabled());
        QCOMPARE(spy.count(), 1);
        QFile f(autostartDir + "/omamusic.desktop");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray text = f.readAll();
        QVERIFY(text.contains("[Desktop Entry]"));
        QVERIFY(text.contains("Exec=\"/opt/my apps/omamusic\" tray\n"));

        a.setEnabled(true);          // no change, no signal
        QCOMPARE(spy.count(), 1);

        Autostart again(autostartDir, "/x");
        QVERIFY(again.enabled());    // state is the file

        a.setEnabled(false);
        QVERIFY(!a.enabled());
        QVERIFY(!QFileInfo::exists(autostartDir + "/omamusic.desktop"));
        QCOMPARE(spy.count(), 2);
    }
};

QTEST_GUILESS_MAIN(TstAutostart)
#include "tst_autostart.moc"
