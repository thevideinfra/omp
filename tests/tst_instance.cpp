#include <QtTest>

#include "instance.h"

class TstInstance : public QObject
{
    Q_OBJECT
private slots:
    void secondLaunchForwardsVerb()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("i.sock");
        Instance first(path);
        QSignalSpy spy(&first, &Instance::verbReceived);
        QVERIFY(first.claimOrSend("show"));

        Instance second(path);
        QVERIFY(!second.claimOrSend("panel"));
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("panel"));
    }

    void staleSocketFileIsReplaced()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("i.sock");
        QFile stale(path);
        QVERIFY(stale.open(QIODevice::WriteOnly));
        stale.close();

        Instance only(path);
        QVERIFY(only.claimOrSend("show"));
        QSignalSpy spy(&only, &Instance::verbReceived);
        Instance other(path);
        QVERIFY(!other.claimOrSend("quit"));
        QTRY_COMPARE(spy.count(), 1);
    }
};

QTEST_GUILESS_MAIN(TstInstance)
#include "tst_instance.moc"
