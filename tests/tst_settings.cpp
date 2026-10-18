#include <QtTest>

#include "settings.h"

static void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(data);
}

static QByteArray readFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

class TstSettings : public QObject
{
    Q_OBJECT
private slots:
    void defaultsWhenEmpty()
    {
        const QVariantMap m = Settings::parse("");
        QCOMPARE(m.value("density").toString(), QString("normal"));
        QCOMPARE(m.value("fontSize").toString(), QString("normal"));
        QCOMPARE(m.value("fontFamily").toString(), QString());
        QCOMPARE(m.value("accent").toString(), QString("theme"));
        QCOMPARE(m.value("panelCover").toBool(), true);
        QCOMPARE(m.value("panelSeek").toBool(), true);
        QCOMPARE(m.value("panelVolume").toBool(), true);
        QCOMPARE(m.value("panelUpNext").toBool(), true);
        QCOMPARE(m.value("panelUpNextCount").toInt(), 4);
    }

    void parsesAndValidates()
    {
        const QVariantMap m = Settings::parse(
            "density = \"compact\"\nfont_size = \"huge\"\nfont_family = \"JetBrains Mono\"\n"
            "accent = \"blue\"\npanel_cover = false\npanel_up_next_count = 99\n");
        QCOMPARE(m.value("density").toString(), QString("compact"));
        QCOMPARE(m.value("fontSize").toString(), QString("normal"));   // invalid -> default
        QCOMPARE(m.value("fontFamily").toString(), QString("JetBrains Mono"));
        QCOMPARE(m.value("accent").toString(), QString("blue"));
        QCOMPARE(m.value("panelCover").toBool(), false);
        QCOMPARE(m.value("panelUpNextCount").toInt(), 10);              // clamped
    }

    void applyReplacesInPlaceAndPreservesOthers()
    {
        const QByteArray in = "# my config\nfolders = [\n  \"~/Music\",\n]\ndensity = \"normal\"\nother = 1\n";
        const QByteArray out = Settings::apply(in, {{"density", "compact"}});
        QCOMPARE(out, QByteArray("# my config\nfolders = [\n  \"~/Music\",\n]\ndensity = \"compact\"\nother = 1\n"));
    }

    void applyAppendsMissingKeysUnderMarker()
    {
        const QByteArray out = Settings::apply("folders = [\"/a\"]\n",
                                               {{"fontSize", "large"}, {"panelSeek", false}});
        QCOMPARE(out, QByteArray("folders = [\"/a\"]\n\n# omamusic settings\nfont_size = \"large\"\npanel_seek = false\n"));
    }

    void applyHandlesMissingTrailingNewlineAndEmptyInput()
    {
        QCOMPARE(Settings::apply("density = \"normal\"", {{"accent", "blue"}}),
                 QByteArray("density = \"normal\"\n\n# omamusic settings\naccent = \"blue\"\n"));
        QCOMPARE(Settings::apply("", {{"accent", "blue"}}),
                 QByteArray("# omamusic settings\naccent = \"blue\"\n"));
    }

    void applyIsIdempotentAndKeepsLongerKeyNames()
    {
        const QByteArray in = "panel_up_next_count = 4\n";
        const QVariantMap u{{"panelUpNext", false}, {"panelUpNextCount", 6}};
        const QByteArray once = Settings::apply(in, u);
        QCOMPARE(once, QByteArray("panel_up_next_count = 6\n\n# omamusic settings\npanel_up_next = false\n"));
        QCOMPARE(Settings::apply(once, u), once);
    }

    void scales()
    {
        QCOMPARE(Settings::densityScaleFor("compact"), 0.8);
        QCOMPARE(Settings::densityScaleFor("normal"), 1.0);
        QCOMPARE(Settings::densityScaleFor("comfortable"), 1.25);
        QCOMPARE(Settings::densityScaleFor("bogus"), 1.0);
        QCOMPARE(Settings::fontScaleFor("small"), 0.9);
        QCOMPARE(Settings::fontScaleFor("normal"), 1.0);
        QCOMPARE(Settings::fontScaleFor("large"), 1.15);
        QCOMPARE(Settings::fontScaleFor("bogus"), 1.0);
    }

    void loadsFromFile()
    {
        QTemporaryDir dir;
        const QString f = dir.filePath("config.toml");
        writeFile(f, "density = \"compact\"\npanel_up_next_count = 2\n");
        Settings s(f);
        QCOMPARE(s.density(), QString("compact"));
        QCOMPARE(s.densityScale(), 0.8);
        QCOMPARE(s.panelUpNextCount(), 2);
        QCOMPARE(s.accent(), QString("theme"));
    }

    void setValidatesAndEmits()
    {
        QTemporaryDir dir;
        Settings s(dir.filePath("config.toml"));
        QSignalSpy spy(&s, &Settings::changed);
        s.set("density", "bogus");
        s.set("unknownKey", 1);
        s.set("fontFamily", "Bad\"Name");
        QCOMPARE(spy.count(), 0);
        QCOMPARE(s.density(), QString("normal"));

        s.set("density", "compact");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(s.density(), QString("compact"));
        s.set("density", "compact");   // no change
        QCOMPARE(spy.count(), 1);

        s.set("panelUpNextCount", 99);
        QCOMPARE(s.panelUpNextCount(), 10);
        s.set("panelCover", false);
        QCOMPARE(s.panelCover(), false);
        s.set("panelCover", "true");
        QCOMPARE(s.panelCover(), true);
    }

    void savesDebouncedKeepingFolders()
    {
        QTemporaryDir dir;
        const QString f = dir.filePath("config.toml");
        writeFile(f, "folders = [\"~/Music\"]\n# hand note\n");
        Settings s(f);
        s.set("density", "compact");
        s.set("accent", "blue");
        QTRY_VERIFY_WITH_TIMEOUT(readFile(f).contains("accent = \"blue\""), 2000);
        const QByteArray text = readFile(f);
        QVERIFY(text.contains("folders = [\"~/Music\"]\n"));
        QVERIFY(text.contains("# hand note\n"));
        QVERIFY(text.contains("density = \"compact\""));
        QVERIFY(!text.contains("font_size"));
    }

    void flushesPendingSaveOnDestruction()
    {
        QTemporaryDir dir;
        const QString f = dir.filePath("config.toml");
        {
            Settings s(f);
            s.set("fontSize", "large");
        }   // destroyed before the debounce fires
        QVERIFY(readFile(f).contains("font_size = \"large\""));
    }

    void unwritableConfigWarnsAndKeepsRunning()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("blocker"), "x");
        Settings s(dir.filePath("blocker/sub/config.toml"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("cannot (create|write) settings"));
        s.set("density", "compact");
        s.save();
        QCOMPARE(s.density(), QString("compact"));
    }
};

QTEST_GUILESS_MAIN(TstSettings)
#include "tst_settings.moc"
