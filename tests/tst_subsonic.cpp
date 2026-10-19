#include <QtTest>

#include "fakesubsonic.h"
#include "subsonicclient.h"

class TstSubsonic : public QObject
{
    Q_OBJECT
private slots:
    void tokenIsTheMd5OfPasswordAndSalt()
    {
        // The example from the Subsonic API documentation.
        QCOMPARE(SubsonicClient::token("sesame", "c19b2d"), QString("26719a1196d2a940705a59634eb18eab"));
    }

    void endpointCarriesTheLoginAndNeverThePassword()
    {
        SubsonicClient c;
        c.setServer("https://music.example.com/", "amy", "sesame");
        const QUrl u = c.endpoint("getAlbum", {{"id", "al-1"}});
        QCOMPARE(u.path(), QString("/rest/getAlbum"));
        const QUrlQuery q(u);
        QCOMPARE(q.queryItemValue("u"), QString("amy"));
        QCOMPARE(q.queryItemValue("v"), QString("1.16.1"));
        QCOMPARE(q.queryItemValue("c"), QString("omamusic"));
        QCOMPARE(q.queryItemValue("f"), QString("json"));
        QCOMPARE(q.queryItemValue("id"), QString("al-1"));
        QCOMPARE(q.queryItemValue("t"), SubsonicClient::token("sesame", q.queryItemValue("s")));
        QVERIFY(!u.toString().contains("sesame"));
        QVERIFY(q.queryItemValue("s").size() >= 6);

        const QUrl s = c.streamUrl("42");
        QCOMPARE(s.path(), QString("/rest/stream"));
        QCOMPARE(QUrlQuery(s).queryItemValue("id"), QString("42"));
        const QUrl cv = c.coverUrl("al-1", 600);
        QCOMPARE(cv.path(), QString("/rest/getCoverArt"));
        QCOMPARE(QUrlQuery(cv).queryItemValue("size"), QString("600"));
    }

    void pingAnswersOkAndReportsAWrongPassword()
    {
        FakeSubsonic fake;
        fake.setResult("ping", {});
        SubsonicClient c;
        c.setServer(fake.url(), "amy", "sesame");
        QSignalSpy spy(&c, &SubsonicClient::pingDone);
        c.ping();
        QTRY_COMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toBool());

        c.setServer(fake.url(), "amy", "wrong");
        c.ping();
        QTRY_COMPARE(spy.count(), 2);
        QVERIFY(!spy.at(1).at(0).toBool());
        QVERIFY(spy.at(1).at(1).toString().contains("Wrong username or password"));
    }

    void anUnreachableOrSilentServerFailsInsteadOfHanging()
    {
        SubsonicClient c;
        c.setTimeoutMs(500);
        QSignalSpy spy(&c, &SubsonicClient::pingDone);
        c.setServer("http://127.0.0.1:1", "amy", "x");   // nothing listens there
        c.ping();
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
        QVERIFY(!spy.at(0).at(0).toBool());

        FakeSubsonic fake;
        fake.ignore("ping");
        c.setServer(fake.url(), "amy", "sesame");
        c.ping();
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 5000);
        QVERIFY(!spy.at(1).at(0).toBool());
    }
};

QTEST_MAIN(TstSubsonic)
#include "tst_subsonic.moc"
