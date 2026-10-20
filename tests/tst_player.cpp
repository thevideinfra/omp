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

    void startIndexNeverStartsTheFirstSong()
    {
        QTemporaryDir dir;
        const QString a = writeWav(dir.filePath("a.wav"), 5.0);
        const QString b = writeWav(dir.filePath("b.wav"), 5.0);
        const QString c = writeWav(dir.filePath("c.wav"), 5.0);
        Player p("mpv", {"--ao=null"}, dir.filePath("mpv.sock"));
        p.start();
        QTRY_VERIFY_WITH_TIMEOUT(p.ready(), 5000);
        QList<int> seen;
        connect(&p, &Player::currentIndexChanged, this, [&] { seen << p.currentIndex(); });
        p.playTracks({row(a, "A"), row(b, "B"), row(c, "C")}, 2);
        QTRY_COMPARE_WITH_TIMEOUT(p.currentIndex(), 2, 3000);
        QTest::qWait(300);
        QVERIFY2(!seen.contains(0) && !seen.contains(1), qPrintable(QString::number(seen.size())));
        QCOMPARE(p.queue()->get(0).value("title").toString(), QString("A"));   // the queue keeps its order
        QCOMPARE(p.queue()->get(2).value("title").toString(), QString("C"));
    }

    void aResolverTurnsServerPathsIntoStreamUrls()
    {
        QTemporaryDir dir;
        const QString wav = writeWav(dir.filePath("a.wav"), 4.0);
        Player p("mpv", {"--ao=null"}, dir.filePath("mpv.sock"));
        p.setPathResolver([&](const QString &path) { return path == "subsonic:1" ? wav : path; });
        p.start();
        QTRY_VERIFY_WITH_TIMEOUT(p.ready(), 5000);
        p.playTracks({row("subsonic:1", "Remote")}, 0);
        QTRY_VERIFY_WITH_TIMEOUT(p.playing(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(p.position() > 0.1, 5000);       // it really played the wav
        QCOMPARE(p.queue()->get(0).value("path").toString(), QString("subsonic:1"));   // the queue keeps the id

        p.enqueue(row("subsonic:1", "Again"));                    // enqueue and shuffle resolve too
        p.setShuffle(true);
        QTRY_COMPARE_WITH_TIMEOUT(p.queue()->rowCount(), 2, 3000);
        QCOMPARE(p.queue()->get(1).value("path").toString(), QString("subsonic:1"));
    }

    void playNextInsertsAfterTheCurrentSongInOrder()
    {
        QTemporaryDir dir;
        const QString a = writeWav(dir.filePath("a.wav"), 5.0);
        const QString b = writeWav(dir.filePath("b.wav"), 5.0);
        const QString c = writeWav(dir.filePath("c.wav"), 5.0);
        const QString x = writeWav(dir.filePath("x.wav"), 5.0);
        const QString y = writeWav(dir.filePath("y.wav"), 5.0);
        Player p("mpv", {"--ao=null"}, dir.filePath("mpv.sock"));
        p.start();
        QTRY_VERIFY_WITH_TIMEOUT(p.ready(), 5000);

        p.playNext({row(x, "X")});                                  // nothing playing: it just plays
        QTRY_VERIFY_WITH_TIMEOUT(p.playing(), 5000);
        QCOMPARE(p.queue()->rowCount(), 1);

        p.playTracks({row(a, "A"), row(b, "B"), row(c, "C")}, 0);
        QTRY_COMPARE_WITH_TIMEOUT(p.current().value("title").toString(), QString("A"), 3000);
        p.playNext({row(x, "X"), row(y, "Y")});
        QCOMPARE(p.queue()->rowCount(), 5);
        QStringList order;
        for (int i = 0; i < 5; ++i)
            order << p.queue()->get(i).value("title").toString();
        QCOMPARE(order, QStringList({"A", "X", "Y", "B", "C"}));
        QCOMPARE(p.currentIndex(), 0);                              // the song playing is not disturbed

        p.next();
        QTRY_COMPARE_WITH_TIMEOUT(p.current().value("title").toString(), QString("X"), 3000);   // mpv agrees
        p.next();
        QTRY_COMPARE_WITH_TIMEOUT(p.current().value("title").toString(), QString("Y"), 3000);
        p.next();
        QTRY_COMPARE_WITH_TIMEOUT(p.current().value("title").toString(), QString("B"), 3000);
    }

    void removeAtDropsASongAndKeepsTheRestInStep()
    {
        QTemporaryDir dir;
        const QString a = writeWav(dir.filePath("a.wav"), 5.0), b = writeWav(dir.filePath("b.wav"), 5.0);
        const QString c = writeWav(dir.filePath("c.wav"), 5.0), d = writeWav(dir.filePath("d.wav"), 5.0);
        Player p("mpv", {"--ao=null"}, dir.filePath("mpv.sock"));
        p.start();
        QTRY_VERIFY_WITH_TIMEOUT(p.ready(), 5000);
        const auto order = [&] {
            QStringList o;
            for (int i = 0; i < p.queue()->rowCount(); ++i)
                o << p.queue()->get(i).value("title").toString();
            return o;
        };
        p.playTracks({row(a, "A"), row(b, "B"), row(c, "C"), row(d, "D")}, 1);       // B is playing
        QTRY_COMPARE_WITH_TIMEOUT(p.current().value("title").toString(), QString("B"), 3000);

        p.removeAt(3);                                                              // after the playing song
        QCOMPARE(order(), QStringList({"A", "B", "C"}));
        QCOMPARE(p.current().value("title").toString(), QString("B"));
        p.removeAt(0);                                                              // before it
        QCOMPARE(order(), QStringList({"B", "C"}));
        QCOMPARE(p.currentIndex(), 0);
        QCOMPARE(p.current().value("title").toString(), QString("B"));              // still B, now first
        QTRY_VERIFY_WITH_TIMEOUT(p.playing(), 3000);

        p.next();                                                                   // mpv agrees about the order
        QTRY_COMPARE_WITH_TIMEOUT(p.current().value("title").toString(), QString("C"), 3000);

        p.previous();
        QTRY_COMPARE_WITH_TIMEOUT(p.current().value("title").toString(), QString("B"), 3000);
        p.removeAt(0);                                                              // the playing one: playback moves on
        QCOMPARE(order(), QStringList({"C"}));
        QTRY_COMPARE_WITH_TIMEOUT(p.current().value("title").toString(), QString("C"), 3000);
        p.removeAt(9);                                                              // out of range: nothing
        QCOMPARE(p.queue()->rowCount(), 1);
    }

    void moveRowReordersTheQueueAndMpvFollows()
    {
        QTemporaryDir dir;
        const QString a = writeWav(dir.filePath("a.wav"), 5.0), b = writeWav(dir.filePath("b.wav"), 5.0);
        const QString c = writeWav(dir.filePath("c.wav"), 5.0), d = writeWav(dir.filePath("d.wav"), 5.0);
        Player p("mpv", {"--ao=null"}, dir.filePath("mpv.sock"));
        p.start();
        QTRY_VERIFY_WITH_TIMEOUT(p.ready(), 5000);
        const auto order = [&] {
            QStringList o;
            for (int i = 0; i < p.queue()->rowCount(); ++i)
                o << p.queue()->get(i).value("title").toString();
            return o;
        };
        p.playTracks({row(a, "A"), row(b, "B"), row(c, "C"), row(d, "D")}, 0);
        QTRY_COMPARE_WITH_TIMEOUT(p.current().value("title").toString(), QString("A"), 3000);

        p.moveRow(3, 1);                                                            // D up to second place
        QCOMPARE(order(), QStringList({"A", "D", "B", "C"}));
        QCOMPARE(p.currentIndex(), 0);
        p.moveRow(0, 2);                                                            // the playing song down
        QCOMPARE(order(), QStringList({"D", "B", "A", "C"}));
        QCOMPARE(p.currentIndex(), 2);
        QCOMPARE(p.current().value("title").toString(), QString("A"));
        p.moveRow(1, 3);                                                            // across the playing one
        QCOMPARE(order(), QStringList({"D", "A", "C", "B"}));
        QCOMPARE(p.currentIndex(), 1);
        QCOMPARE(p.current().value("title").toString(), QString("A"));

        p.next();                                                                   // mpv has the same order
        QTRY_COMPARE_WITH_TIMEOUT(p.current().value("title").toString(), QString("C"), 3000);
        p.moveRow(5, 0);                                                            // out of range: nothing
        QCOMPARE(order(), QStringList({"D", "A", "C", "B"}));
    }

    void enqueueAndClear()
    {
        QTemporaryDir dir;
        const QString a = writeWav(dir.filePath("a.wav"), 5.0);
        Player p("mpv", {"--ao=null"}, dir.filePath("mpv.sock"));
        p.start();
        QTRY_VERIFY_WITH_TIMEOUT(p.ready(), 5000);
        const QString b = writeWav(dir.filePath("b.wav"), 5.0);
        p.enqueue(row(a, "A"));
        QTRY_VERIFY_WITH_TIMEOUT(p.playing(), 5000);   // append-play starts an idle player
        p.enqueue(row(b, "B"));
        QCOMPARE(p.queue()->rowCount(), 2);
        p.clearQueue();
        // The song that is playing stays and keeps playing.
        QCOMPARE(p.queue()->rowCount(), 1);
        QCOMPARE(p.current().value("title").toString(), QString("A"));
        QTRY_COMPARE_WITH_TIMEOUT(p.currentIndex(), 0, 3000);
        QTest::qWait(500);
        QVERIFY(p.playing());
        QVERIFY(p.position() > 0.2);   // not restarted
    }

    void shuffleMixesTheUpcomingSongsOnlyAndUnshuffleRestoresThem()
    {
        QTemporaryDir dir;
        Player p("mpv", {"--ao=null"}, dir.filePath("mpv.sock"));
        p.start();
        QTRY_VERIFY_WITH_TIMEOUT(p.ready(), 5000);
        QVariantList tracks;
        QStringList titles;
        for (const char *t : {"A", "B", "C", "D", "E", "F", "G", "H"}) {
            tracks << row(writeWav(dir.filePath(QString(t) + ".wav"), 20.0), t);
            titles << t;
        }
        auto order = [&p] {
            QStringList l;
            for (int i = 0; i < p.queue()->rowCount(); ++i)
                l << p.queue()->get(i).value("title").toString();
            return l;
        };
        p.playTracks(tracks, 2);
        QTRY_COMPARE_WITH_TIMEOUT(p.current().value("title").toString(), QString("C"), 5000);

        p.setShuffle(true);
        QVERIFY(p.shuffle());
        QTest::qWait(600);
        const QStringList shuffled = order();
        QCOMPARE(shuffled.mid(0, 3), QStringList({"A", "B", "C"}));   // played and playing stay put
        QStringList upcoming = shuffled.mid(3);
        upcoming.sort();
        QCOMPARE(upcoming, QStringList({"D", "E", "F", "G", "H"}));    // nothing lost
        QCOMPARE(p.current().value("title").toString(), QString("C"));
        QCOMPARE(p.currentIndex(), 2);

        p.setShuffle(false);
        QTRY_COMPARE_WITH_TIMEOUT(order(), titles, 3000);
        QCOMPARE(p.current().value("title").toString(), QString("C"));
    }

    void startingAQueueWithShuffleOnPlaysTheChosenSongFirstAndKeepsAllSongs()
    {
        QTemporaryDir dir;
        Player p("mpv", {"--ao=null"}, dir.filePath("mpv.sock"));
        p.setStartPlayback(true, "off", "Flat");
        p.start();
        QTRY_VERIFY_WITH_TIMEOUT(p.ready(), 5000);
        QVariantList tracks;
        QStringList titles;
        for (const char *t : {"A", "B", "C", "D", "E", "F"}) {
            tracks << row(writeWav(dir.filePath(QString(t) + ".wav"), 20.0), t);
            titles << t;
        }
        p.playTracks(tracks, 3);
        QTRY_COMPARE_WITH_TIMEOUT(p.current().value("title").toString(), QString("D"), 5000);
        QCOMPARE(p.currentIndex(), 0);   // the song you chose comes first; every other song follows
        QStringList all;
        for (int i = 0; i < p.queue()->rowCount(); ++i)
            all << p.queue()->get(i).value("title").toString();
        all.sort();
        QCOMPARE(all, titles);
    }

    void repeatOneKeepsPlayingTheSong()
    {
        QTemporaryDir dir;
        const QString a = writeWav(dir.filePath("a.wav"), 0.6);
        Player p("mpv", {"--ao=null"}, dir.filePath("mpv.sock"));
        p.start();
        QTRY_VERIFY_WITH_TIMEOUT(p.ready(), 5000);
        p.playTracks({row(a, "A")}, 0);
        QTRY_VERIFY_WITH_TIMEOUT(!p.playing(), 4000);   // off: it ends

        p.setRepeat("one");
        QCOMPARE(p.repeat(), QString("one"));
        p.playTracks({row(a, "A")}, 0);
        QTRY_VERIFY_WITH_TIMEOUT(p.playing(), 3000);
        QTest::qWait(1800);
        QVERIFY(p.playing());   // three times its length later

        p.cycleRepeat();
        QCOMPARE(p.repeat(), QString("off"));
        p.setRepeat("bogus");
        QCOMPARE(p.repeat(), QString("off"));
    }

    void equalizerPresetsApplyAtStartAndWhilePlaying()
    {
        QTemporaryDir dir;
        const QString a = writeWav(dir.filePath("a.wav"), 5.0);
        Player p("mpv", {"--ao=null"}, dir.filePath("mpv.sock"));
        p.setStartPlayback(false, "all", "Rock");
        QCOMPARE(p.eqPreset(), QString("Rock"));
        p.start();
        QTRY_VERIFY_WITH_TIMEOUT(p.ready(), 5000);
        p.playTracks({row(a, "A")}, 0);
        QTRY_VERIFY_WITH_TIMEOUT(p.playing(), 5000);
        QVERIFY(p.error().isEmpty());

        p.setEqPreset("Bass Boost");
        QCOMPARE(p.eqPreset(), QString("Bass Boost"));
        p.setEqPreset("nope");
        QCOMPARE(p.eqPreset(), QString("Bass Boost"));
        p.setEqPreset("Flat");
        QTest::qWait(400);
        QVERIFY(p.playing());
        QVERIFY(p.position() > 0.2);
        QVERIFY(p.eqPresets().contains("Jazz"));
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
