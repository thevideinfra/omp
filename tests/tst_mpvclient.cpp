#include <QtTest>
#include <QLocalServer>
#include <QLocalSocket>

#include "mpvclient.h"

class TstMpvClient : public QObject
{
    Q_OBJECT
private slots:
    void sendsCommandsAndParsesPropertyEvents()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("mpv.sock");
        QLocalServer server;
        QVERIFY(server.listen(path));

        MpvClient client;
        QSignalSpy connected(&client, &MpvClient::connected);
        QSignalSpy changed(&client, &MpvClient::propertyChanged);
        client.connectTo(path);
        QTRY_VERIFY(server.hasPendingConnections());
        QLocalSocket *peer = server.nextPendingConnection();
        QTRY_COMPARE(connected.count(), 1);
        QVERIFY(client.isConnected());

        client.observe("pause");
        QTRY_VERIFY(peer->bytesAvailable() > 0);
        QCOMPARE(peer->readLine().trimmed(),
                 QByteArray(R"({"command":["observe_property",1,"pause"]})"));

        client.setProperty("volume", 80);
        QTRY_VERIFY(peer->canReadLine());
        QCOMPARE(peer->readLine().trimmed(),
                 QByteArray(R"({"command":["set_property","volume",80]})"));

        // two events in one chunk, the second split across writes
        peer->write("{\"event\":\"property-change\",\"id\":1,\"name\":\"pause\",\"data\":true}\n"
                    "{\"event\":\"property-chan");
        peer->flush();
        QTRY_COMPARE(changed.count(), 1);
        peer->write("ge\",\"id\":2,\"name\":\"time-pos\",\"data\":1.5}\n"
                    "{\"event\":\"idle\"}\n");
        peer->flush();
        QTRY_COMPARE(changed.count(), 2);
        QCOMPARE(changed.at(0).at(0).toString(), QString("pause"));
        QCOMPARE(changed.at(0).at(1).toBool(), true);
        QCOMPARE(changed.at(1).at(0).toString(), QString("time-pos"));
        QCOMPARE(changed.at(1).at(1).toDouble(), 1.5);
    }

    void commandWhileDisconnectedIsIgnored()
    {
        MpvClient client;
        client.command({"stop"});  // must not crash
        QVERIFY(!client.isConnected());
    }
};

QTEST_GUILESS_MAIN(TstMpvClient)
#include "tst_mpvclient.moc"
