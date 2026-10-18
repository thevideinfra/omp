#include <QtTest>

#include <QRegularExpression>

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

    void listeningReflectsSocketState()
    {
        QTemporaryDir dir;
        Instance ok(dir.filePath("ok.sock"));
        QVERIFY(ok.claimOrSend("show"));
        QVERIFY(ok.listening());

        // Parent directory does not exist, so listen() must fail.
        Instance broken(dir.filePath("missing/dir/broken.sock"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("single-instance socket failed"));
        QVERIFY(broken.claimOrSend("show"));
        QVERIFY(!broken.listening());
    }
};

QTEST_GUILESS_MAIN(TstInstance)
#include "tst_instance.moc"
