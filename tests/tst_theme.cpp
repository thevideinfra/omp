#include <QtTest>

#include "theme.h"

static void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    f.open(QIODevice::WriteOnly | QIODevice::Truncate);
    f.write(data);
}

class TstTheme : public QObject
{
    Q_OBJECT
private slots:
    void parsesQuotedValuesAndSkipsComments()
    {
        const auto kv = Theme::parse("# c\nmode = \"dark\"\n[x]\naccent = \"#59C98D\"\nbare = 5\n");
        QCOMPARE(kv.value("mode"), QString("dark"));
        QCOMPARE(kv.value("accent"), QString("#59C98D"));
        QCOMPARE(kv.value("bare"), QString("5"));
    }

    void fallsBackWhenFileMissing()
    {
        Theme t("/nonexistent/colors.toml");
        QVERIFY(t.background().isValid());
        QVERIFY(t.foreground().isValid());
    }

    void mapsFileKeysToProperties()
    {
        QTemporaryDir dir;
        const QString f = dir.filePath("colors.toml");
        writeFile(f, "background = \"#112233\"\nlighter_background = \"#223344\"\n"
                     "dark_foreground = \"#334455\"\naccent = \"#445566\"\nred = \"#556677\"\n");
        Theme t(f);
        QCOMPARE(t.background(), QColor("#112233"));
        QCOMPARE(t.surface(), QColor("#223344"));
        QCOMPARE(t.dimForeground(), QColor("#334455"));
        QCOMPARE(t.accent(), QColor("#445566"));
        QCOMPARE(t.danger(), QColor("#556677"));
    }

    void reloadsWhenFileChanges()
    {
        QTemporaryDir dir;
        const QString f = dir.filePath("colors.toml");
        writeFile(f, "background = \"#112233\"\n");
        Theme t(f);
        QSignalSpy spy(&t, &Theme::changed);
        writeFile(f, "background = \"#abcdef\"\n");
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 3000);
        QCOMPARE(t.background(), QColor("#abcdef"));
    }

    void followsFileReplacement()
    {
        QTemporaryDir dir;
        const QString f = dir.filePath("colors.toml");
        writeFile(f, "background = \"#112233\"\n");
        Theme t(f);
        QSignalSpy spy(&t, &Theme::changed);
        QFile::remove(f);
        writeFile(f, "background = \"#aa0000\"\n");
        QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 3000);
        QCOMPARE(t.background(), QColor("#aa0000"));
        const int before = spy.count();
        writeFile(f, "background = \"#00bb00\"\n");
        QTRY_VERIFY_WITH_TIMEOUT(spy.count() > before, 3000);
        QCOMPARE(t.background(), QColor("#00bb00"));
    }

    void accentChoiceUsesPaletteAndFallsBack()
    {
        QTemporaryDir dir;
        const QString f = dir.filePath("colors.toml");
        writeFile(f, "accent = \"#111111\"\nblue = \"#0000ff\"\nred = \"#ff0000\"\n");
        Theme t(f);
        QCOMPARE(t.accentChoices(), QStringList({"theme", "blue", "red"}));
        QCOMPARE(t.accentChoice(), QString("theme"));
        QCOMPARE(t.accent(), QColor("#111111"));

        QSignalSpy spy(&t, &Theme::changed);
        t.setAccentChoice("blue");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(t.accent(), QColor("#0000ff"));
        t.setAccentChoice("blue");
        QCOMPARE(spy.count(), 1);

        t.setAccentChoice("cyan");   // not in this theme -> theme accent, name kept
        QCOMPARE(t.accentChoice(), QString("cyan"));
        QCOMPARE(t.accent(), QColor("#111111"));

        QCOMPARE(t.paletteColor("red"), QColor("#ff0000"));
        QCOMPARE(t.paletteColor("theme"), QColor("#111111"));
        QVERIFY(!t.paletteColor("cyan").isValid());

        t.setAccentChoice("");
        QCOMPARE(t.accentChoice(), QString("theme"));
    }

    void accentChoiceFollowsThemeSwitch()
    {
        QTemporaryDir dir;
        const QString f = dir.filePath("colors.toml");
        writeFile(f, "accent = \"#111111\"\nblue = \"#0000ff\"\n");
        Theme t(f);
        t.setAccentChoice("blue");
        QCOMPARE(t.accent(), QColor("#0000ff"));
        writeFile(f, "accent = \"#222222\"\nblue = \"#00ff00\"\n");
        QTRY_COMPARE_WITH_TIMEOUT(t.accent(), QColor("#00ff00"), 3000);
        QCOMPARE(t.accentChoice(), QString("blue"));
    }

    void fontBaseComesFromShellToml()
    {
        QTemporaryDir dir;
        const QString f = dir.filePath("colors.toml");
        writeFile(f, "accent = \"#111111\"\n");
        {
            Theme t(f);
            QCOMPARE(t.fontBase(), 12.0);   // no shell.toml
        }
        writeFile(dir.filePath("shell.toml"), "[bar]\ntext = \"#fff\"\n\n[font]\n# comment\nbase-size = 14\n");
        Theme t(f);
        QCOMPARE(t.fontBase(), 14.0);

        writeFile(dir.filePath("shell.toml"), "[font]\nbase-size = 16\n");
        QTRY_COMPARE_WITH_TIMEOUT(t.fontBase(), 16.0, 3000);

        writeFile(dir.filePath("shell.toml"), "[font]\nbase-size = 0\n");
        QTRY_COMPARE_WITH_TIMEOUT(t.fontBase(), 12.0, 3000);
    }
};

QTEST_GUILESS_MAIN(TstTheme)
#include "tst_theme.moc"
