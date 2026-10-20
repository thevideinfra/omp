#include <QtTest>

#include "fakesecrettool.h"
#include "secrets.h"

class TstSecrets : public QObject
{
    Q_OBJECT
private slots:
    void storesLooksUpAndClears()
    {
        QTemporaryDir dir;
        qputenv("FAKE_SECRET_DIR", dir.path().toUtf8());
        Secrets s(writeFake(dir.path()));

        QSignalSpy looked(&s, &Secrets::looked);
        s.lookup("amy@https://m.example.com");
        QTRY_COMPARE(looked.count(), 1);
        QVERIFY(!looked.at(0).at(1).toBool());                 // nothing stored yet

        QSignalSpy stored(&s, &Secrets::stored);
        s.store("amy@https://m.example.com", "pa ss\"word");
        QTRY_COMPARE(stored.count(), 1);
        QVERIFY(stored.at(0).at(0).toBool());

        s.lookup("amy@https://m.example.com");
        QTRY_COMPARE(looked.count(), 2);
        QVERIFY(looked.at(1).at(1).toBool());
        QCOMPARE(looked.at(1).at(0).toString(), QString("pa ss\"word"));

        QSignalSpy cleared(&s, &Secrets::cleared);
        s.clear("amy@https://m.example.com");
        QTRY_COMPARE(cleared.count(), 1);
        s.lookup("amy@https://m.example.com");
        QTRY_COMPARE(looked.count(), 3);
        QVERIFY(!looked.at(2).at(1).toBool());
    }

    void aMissingToolReportsItInsteadOfCrashing()
    {
        Secrets s("/nonexistent/secret-tool");
        QSignalSpy failed(&s, &Secrets::failed);
        QSignalSpy stored(&s, &Secrets::stored);
        s.store("amy@x", "pw");
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(failed.at(0).at(0).toString().contains("secret-tool"));
        QCOMPARE(stored.count(), 1);
        QVERIFY(!stored.at(0).at(0).toBool());
    }
};

QTEST_MAIN(TstSecrets)
#include "tst_secrets.moc"
