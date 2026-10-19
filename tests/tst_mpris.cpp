#include <QtDBus>
#include <QtTest>

#include "library.h"
#include "mpris.h"
#include "player.h"

class TstMpris : public QObject
{
    Q_OBJECT
private slots:
    void exposesAnOmamusicPlayerOnTheBus()
    {
        if (!QDBusConnection::sessionBus().isConnected())
            QSKIP("no session bus");
        QTemporaryDir dir;
        Player player("mpv", {"--ao=null"}, dir.filePath("mpv.sock"));
        Library library({}, QString(), dir.filePath("index.jsonl"));
        const QString service = QStringLiteral("org.mpris.MediaPlayer2.omamusic_test%1").arg(QCoreApplication::applicationPid());
        Mpris mpris(player, library, service);
        QVERIFY(mpris.registered());

        const QString path = QStringLiteral("/org/mpris/MediaPlayer2");
        QDBusInterface root(service, path, "org.mpris.MediaPlayer2");
        QDBusInterface props(service, path, "org.freedesktop.DBus.Properties");
        QVERIFY(root.isValid());
        QCOMPARE(root.property("Identity").toString(), QString("omamusic"));
        QCOMPARE(root.property("DesktopEntry").toString(), QString("omamusic"));
        QVERIFY(root.property("CanQuit").toBool());

        QDBusInterface pl(service, path, "org.mpris.MediaPlayer2.Player");
        QCOMPARE(pl.property("PlaybackStatus").toString(), QString("Stopped"));
        QCOMPARE(pl.property("LoopStatus").toString(), QString("None"));
        QVERIFY(!pl.property("Shuffle").toBool());
        QVERIFY(pl.property("CanControl").toBool());

        // Writes go through to the player.
        QDBusReply<void> r = props.call("Set", "org.mpris.MediaPlayer2.Player", "LoopStatus",
                                        QVariant::fromValue(QDBusVariant(QString("Playlist"))));
        QVERIFY(r.isValid());
        QCOMPARE(player.repeat(), QString("all"));
        QCOMPARE(pl.property("LoopStatus").toString(), QString("Playlist"));
        props.call("Set", "org.mpris.MediaPlayer2.Player", "Shuffle", QVariant::fromValue(QDBusVariant(true)));
        QVERIFY(player.shuffle());
    }
};

QTEST_MAIN(TstMpris)
#include "tst_mpris.moc"
