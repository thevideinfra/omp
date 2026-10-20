#include <QtTest>

#include "scrobbletracker.h"

class TstScrobble : public QObject
{
    Q_OBJECT
private slots:
    void announcesTheStartAndCountsHalfThroughOnce()
    {
        ScrobbleTracker t;
        QSignalSpy now(&t, &ScrobbleTracker::nowPlaying), sub(&t, &ScrobbleTracker::submit);
        t.update("subsonic:7", 0.2, 200);
        QCOMPARE(now.count(), 1);
        QCOMPARE(now.at(0).at(0).toString(), QString("7"));
        t.update("subsonic:7", 50, 200);
        QCOMPARE(sub.count(), 0);                               // not yet half
        t.update("subsonic:7", 100, 200);
        QCOMPARE(sub.count(), 1);
        QCOMPARE(sub.at(0).at(0).toString(), QString("7"));
        t.update("subsonic:7", 150, 200);
        t.update("subsonic:7", 20, 200);                         // a seek back does not count again
        t.update("subsonic:7", 120, 200);
        QCOMPARE(sub.count(), 1);
        QCOMPARE(now.count(), 1);
    }

    void longSongsCountAfterFourMinutes()
    {
        ScrobbleTracker t;
        QSignalSpy sub(&t, &ScrobbleTracker::submit);
        t.update("subsonic:1", 0, 900);
        t.update("subsonic:1", 239, 900);
        QCOMPARE(sub.count(), 0);
        t.update("subsonic:1", 240, 900);
        QCOMPARE(sub.count(), 1);
    }

    void aNewSongStartsOverAndShortOrLocalSongsNeverCount()
    {
        ScrobbleTracker t;
        QSignalSpy now(&t, &ScrobbleTracker::nowPlaying), sub(&t, &ScrobbleTracker::submit);
        t.update("subsonic:1", 100, 200);                        // first seen mid-song: it starts here
        t.update("subsonic:2", 0, 200);
        QCOMPARE(now.count(), 2);
        t.update("subsonic:3", 0, 20);                           // under 30 s: announced, never counted
        t.update("subsonic:3", 19, 20);
        t.update("/music/local.flac", 0, 200);                   // local songs are not for the server
        t.update("/music/local.flac", 150, 200);
        QCOMPARE(now.count(), 3);
        QCOMPARE(sub.count(), 0);
    }

    void aRepeatedSongCountsAgain()
    {
        ScrobbleTracker t;
        QSignalSpy now(&t, &ScrobbleTracker::nowPlaying), sub(&t, &ScrobbleTracker::submit);
        t.update("subsonic:1", 0, 60);
        t.update("subsonic:1", 31, 60);
        QCOMPARE(sub.count(), 1);
        t.update("subsonic:1", 59, 60);
        t.update("subsonic:1", 0.5, 60);                         // repeat one: back at the start
        QCOMPARE(now.count(), 2);
        t.update("subsonic:1", 31, 60);
        QCOMPARE(sub.count(), 2);
    }
};

QTEST_MAIN(TstScrobble)
#include "tst_scrobble.moc"
