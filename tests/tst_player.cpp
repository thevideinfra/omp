#include <QtTest>

#include "player.h"
#include "testutil.h"

static QVariantMap row(const QString &path, const QString &title)
{
    return {{"path", path}, {"title", title}, {"artist", "A"}, {"album", "B"},
            {"duration", 1.0}, {"subtitle", "A — B"}, {"meta", "0:01"}, {"cover", ""}};
}

class TstPlayer : public QObject
{
    Q_OBJECT
private slots:
    void playsPausesAndSeeks()
    {
        QTemporaryDir dir;
        const QString wav = writeWav(dir.filePath("a.wav"), 4.0);
        Player p("mpv", {"--ao=null"}, dir.filePath("mpv.sock"));
        p.start();
        QTRY_VERIFY_WITH_TIMEOUT(p.ready(), 5000);

        p.playTracks({row(wav, "A")}, 0);
        QTRY_VERIFY_WITH_TIMEOUT(p.playing(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(p.position() > 0.1, 5000);
        QCOMPARE(p.queue()->rowCount(), 1);
        QCOMPARE(p.current().value("title").toString(), QString("A"));
        QTRY_VERIFY_WITH_TIMEOUT(p.duration() > 3.5, 3000);

        p.togglePause();
        QTRY_VERIFY_WITH_TIMEOUT(!p.playing(), 3000);
        p.togglePause();
        QTRY_VERIFY_WITH_TIMEOUT(p.playing(), 3000);

        p.seekTo(2.5);
        QTRY_VERIFY_WITH_TIMEOUT(p.position() > 2.4, 3000);
        p.setVolume(40);
        QTRY_COMPARE_WITH_TIMEOUT(p.volume(), 40, 3000);
    }

    void startsAtTheGivenVolume()
    {
        QTemporaryDir dir;
        Player p("mpv", {"--ao=null"}, dir.filePath("mpv.sock"));
        p.setStartVolume(35);
        p.start();
        QTRY_VERIFY_WITH_TIMEOUT(p.ready(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(p.volume(), 35, 3000);
    }

    void advancesAcrossQueueAndJumps()
    {
        QTemporaryDir dir;
        const QString a = writeWav(dir.filePath("a.wav"), 1.0);
        const QString b = writeWav(dir.filePath("b.wav"), 1.0);
        const QString c = writeWav(dir.filePath("c.wav"), 5.0);
        Player p("mpv", {"--ao=null"}, dir.filePath("mpv.sock"));
        p.start();
        QTRY_VERIFY_WITH_TIMEOUT(p.ready(), 5000);

        p.playTracks({row(a, "A"), row(b, "B"), row(c, "C")}, 0);
        QTRY_COMPARE_WITH_TIMEOUT(p.currentIndex(), 1, 6000);   // gapless advance
        QCOMPARE(p.current().value("title").toString(), QString("B"));

        p.jumpTo(2);
        QTRY_COMPARE_WITH_TIMEOUT(p.currentIndex(), 2, 3000);
        p.previous();
        QTRY_COMPARE_WITH_TIMEOUT(p.currentIndex(), 1, 3000);
    }

    void startIndexSkipsAhead()
    {
        QTemporaryDir dir;
        const QString a = writeWav(dir.filePath("a.wav"), 5.0);
        const QString b = writeWav(dir.filePath("b.wav"), 5.0);
        Player p("mpv", {"--ao=null"}, dir.filePath("mpv.sock"));
        p.start();
        QTRY_VERIFY_WITH_TIMEOUT(p.ready(), 5000);
        p.playTracks({row(a, "A"), row(b, "B")}, 1);
        QTRY_COMPARE_WITH_TIMEOUT(p.currentIndex(), 1, 3000);
    }

    void enqueueAndClear()
    {
        QTemporaryDir dir;
        const QString a = writeWav(dir.filePath("a.wav"), 5.0);
        Player p("mpv", {"--ao=null"}, dir.filePath("mpv.sock"));
        p.start();
        QTRY_VERIFY_WITH_TIMEOUT(p.ready(), 5000);
        p.enqueue(row(a, "A"));
        QTRY_VERIFY_WITH_TIMEOUT(p.playing(), 5000);   // append-play starts an idle player
        QCOMPARE(p.queue()->rowCount(), 1);
        p.clearQueue();
        QCOMPARE(p.queue()->rowCount(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(!p.playing(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(p.currentIndex(), -1, 3000);
    }

    void reportsMissingMpv()
    {
        QTemporaryDir dir;
        Player p("/nonexistent/mpv", {}, dir.filePath("mpv.sock"));
        QSignalSpy err(&p, &Player::errorChanged);
        p.start();
        QTRY_VERIFY_WITH_TIMEOUT(!p.error().isEmpty(), 3000);
        QVERIFY(!p.ready());
    }
};

QTEST_GUILESS_MAIN(TstPlayer)
#include "tst_player.moc"
