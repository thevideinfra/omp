#include <QtTest>

#include "track.h"

class TstTrack : public QObject
{
    Q_OBJECT
private slots:
    void parsesScanLine()
    {
        bool ok = false;
        const Track t = Track::fromJsonLine(
            R"({"path":"/m/a/01.flac","title":"One","artist":"Amy","album_artist":"Amy","album":"Beta","track":1,"disc":1,"duration":61.4,"size":10,"mtime":20})",
            &ok);
        QVERIFY(ok);
        QCOMPARE(t.title, QString("One"));
        QCOMPARE(t.albumArtist, QString("Amy"));
        QCOMPARE(t.track, 1);
        QCOMPARE(t.size, qint64(10));
    }

    void fillsDefaults()
    {
        bool ok = false;
        const Track t = Track::fromJsonLine(R"({"path":"/m/a/song name.mp3"})", &ok);
        QVERIFY(ok);
        QCOMPARE(t.title, QString("song name"));
        QCOMPARE(t.artist, QString("Unknown Artist"));
        QCOMPARE(t.albumArtist, QString("Unknown Artist"));
        QCOMPARE(t.album, QString("Unknown Album"));
    }

    void rejectsGarbage()
    {
        bool ok = true;
        Track::fromJsonLine("not json", &ok);
        QVERIFY(!ok);
        ok = true;
        Track::fromJsonLine(R"({"title":"no path"})", &ok);
        QVERIFY(!ok);
    }

    void roundTripsThroughCacheLine()
    {
        bool ok = false;
        const Track a = Track::fromJsonLine(
            R"({"path":"/x.flac","title":"T","artist":"A","album_artist":"AA","album":"B","track":3,"disc":2,"duration":5.5,"size":1,"mtime":2})");
        const Track b = Track::fromJsonLine(a.toJsonLine(), &ok);
        QVERIFY(ok);
        QCOMPARE(b.toMap(), a.toMap());
        QCOMPARE(b.size, a.size);
        QCOMPARE(b.mtime, a.mtime);
    }

    void mapHasDisplayFields()
    {
        Track t;
        t.path = "/x";
        t.title = "T";
        t.artist = "A";
        t.album = "B";
        t.duration = 125.2;
        const QVariantMap m = t.toMap();
        QCOMPARE(m.value("subtitle").toString(), QString("A — B"));
        QCOMPARE(m.value("meta").toString(), QString("2:05"));
        QCOMPARE(m.value("cover").toString(), QString());
        for (const QString &role : Track::roleNames())
            QVERIFY2(m.contains(role), qPrintable(role));
    }
};

QTEST_GUILESS_MAIN(TstTrack)
#include "tst_track.moc"
