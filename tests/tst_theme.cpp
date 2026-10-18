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
};

QTEST_GUILESS_MAIN(TstTheme)
#include "tst_theme.moc"
