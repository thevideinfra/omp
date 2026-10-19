#include <QtTest>

#include "equalizer.h"

class TstEqualizer : public QObject
{
    Q_OBJECT
private slots:
    void hasTheStandardPresets()
    {
        const QStringList names = Equalizer::presetNames();
        QCOMPARE(names.first(), QString("Flat"));
        for (const char *n : {"Bass Boost", "Treble Boost", "Rock", "Pop", "Jazz", "Classical", "Vocal"})
            QVERIFY2(names.contains(n), n);
    }

    void everyPresetHasOneGainPerBand()
    {
        for (const QString &n : Equalizer::presetNames())
            QCOMPARE(Equalizer::gains(n).size(), Equalizer::kBands);
        QCOMPARE(Equalizer::gains("nope").size(), Equalizer::kBands);
    }

    void flatAndUnknownHaveNoFilter()
    {
        QCOMPARE(Equalizer::filter("Flat"), QString());
        QCOMPARE(Equalizer::filter("nope"), QString());
    }

    void boostsGetAPreampCut()
    {
        const QString f = Equalizer::filter("Bass Boost");
        QVERIFY(f.startsWith("lavfi=[volume=-5dB,equalizer=f=31:t=o:w=1:g=5,"));
        QVERIFY(f.endsWith("]"));
        QVERIFY(f.contains("equalizer=f=250:t=o:w=1:g=1.5"));
        QVERIFY(!f.contains("f=1000"));   // zero-gain bands are left out
    }

    void filtersAreValidForMpv()
    {
        QTemporaryDir dir;
        QProcess ff;
        ff.start("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "sine=d=0.3", "-y", dir.filePath("a.wav")});
        QVERIFY(ff.waitForFinished(10000));
        for (const QString &n : Equalizer::presetNames()) {
            QProcess mpv;
            QStringList args{"--no-video", "--no-terminal", "--ao=null", "--end=0.2"};
            if (!Equalizer::filter(n).isEmpty())
                args << "--af=" + Equalizer::filter(n);
            args << dir.filePath("a.wav");
            mpv.start("mpv", args);
            QVERIFY(mpv.waitForFinished(15000));
            QVERIFY2(mpv.exitCode() == 0, qPrintable(n + ": " + QString::fromUtf8(mpv.readAllStandardError())));
        }
    }
};

QTEST_GUILESS_MAIN(TstEqualizer)
#include "tst_equalizer.moc"
