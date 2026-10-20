#include <QtTest>

#include "lookscope.h"
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

    void columnLayoutIsOptInStoredPerTabAndResettable()
    {
        QTemporaryDir dir;
        const QString f = dir.filePath("config.toml");
        {
            Settings s(f);
            QVERIFY(!s.saveLayout());
            QCOMPARE(s.layoutFor("artist"), QString());
            QSignalSpy reset(&s, &Settings::layoutReset);

            s.setLayoutFor("artist", "0.3,0.4");
            s.setLayoutFor("album", "0.6");
            s.setLayoutFor("bogus", "0.5");        // not a tab
            s.setLayoutFor("playlist", "1.5,0.2"); // out of range
            s.setLayoutFor("playlist", "x");
            QCOMPARE(s.layoutFor("artist"), QString("0.3,0.4"));
            QCOMPARE(s.layoutFor("album"), QString("0.6"));
            QCOMPARE(s.layoutFor("playlist"), QString());
            QCOMPARE(s.layoutFor("bogus"), QString());

            s.set("saveLayout", true);
            QVERIFY(s.saveLayout());
            s.save();

            Settings again(f);
            QVERIFY(again.saveLayout());
            QCOMPARE(again.layoutFor("artist"), QString("0.3,0.4"));

            again.resetLayout();
            QCOMPARE(again.layoutFor("artist"), QString());
            QCOMPARE(again.layoutFor("album"), QString());
            QVERIFY(again.saveLayout());           // reset clears the sizes, not the toggle
        }
    }

    void sortMemoryKeysDefaultOffAndPersist()
    {
        QTemporaryDir dir;
        const QString f = dir.filePath("config.toml");
        {
            Settings s(f);
            QVERIFY(!s.saveSort());
            QCOMPARE(s.sortSongs(), QString());
            s.set("saveSort", true);
            s.set("sortSongs", "length,desc");
            s.set("sortArtists", "added,asc");
        }
        Settings again(f);
        QVERIFY(again.saveSort());
        QCOMPARE(again.sortSongs(), QString("length,desc"));
        QCOMPARE(again.sortArtists(), QString("added,asc"));
        QCOMPARE(again.sortAlbums(), QString());
    }

    void railAndTagOptionsDefaultAndPersist()
    {
        QTemporaryDir dir;
        const QString f = dir.filePath("config.toml");
        {
            Settings s(f);
            QVERIFY(!s.railCollapsed());          // the rail starts expanded
            QVERIFY(s.showSourceTags());          // and the local/server tags show
            s.set("railCollapsed", true);
            s.set("showSourceTags", false);
        }
        Settings again(f);
        QVERIFY(again.railCollapsed());
        QVERIFY(!again.showSourceTags());
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

        // the failed save keeps the key dirty, so a later save retries
        QVERIFY(QFile::remove(dir.filePath("blocker")));
        s.save();
        QVERIFY(readFile(dir.filePath("blocker/sub/config.toml")).contains("density = \"compact\""));
    }

    void applyRewritesEveryDuplicateLine()
    {
        const QByteArray out = Settings::apply(
            "density = \"normal\"\nx = 1\ndensity = \"comfortable\"\n", {{"density", "compact"}});
        QCOMPARE(out, QByteArray("density = \"compact\"\nx = 1\ndensity = \"compact\"\n"));
    }

    void foldersParseAddRemoveAndPersist()
    {
        QTemporaryDir dir;
        const QString f = dir.filePath("config.toml");
        QTemporaryDir a, b;
        QFile cfg(f);
        QVERIFY(cfg.open(QIODevice::WriteOnly));
        cfg.write(QString("density = \"compact\"\nfolders = [\n  \"%1\",\n]\nvolume = 20\n").arg(a.path()).toUtf8());
        cfg.close();

        Settings s(f);
        QCOMPARE(s.folders(), QStringList({a.path()}));
        s.addFolder(b.path());
        s.addFolder(b.path());                    // twice is once
        s.addFolder("/definitely/not/a/dir");     // must exist
        QCOMPARE(s.folders(), QStringList({a.path(), b.path()}));
        s.removeFolder(a.path());
        s.removeFolder(b.path());                 // the last one stays
        QCOMPARE(s.folders(), QStringList({b.path()}));
        s.save();

        const QByteArray out = readFile(f);
        QVERIFY(out.contains("density = \"compact\""));
        QVERIFY(out.contains("volume = 20"));
        QCOMPARE(out.count("folders"), 1);        // the old multi-line block is gone
        QVERIFY(out.contains(QString("folders = [\"%1\"]").arg(b.path()).toUtf8()));
        Settings again(f);
        QCOMPARE(again.folders(), QStringList({b.path()}));
    }

    void rewritingFoldersKeepsEverythingElse()
    {
        QTemporaryDir dir, b;
        const QString f = dir.filePath("config.toml");
        for (const QByteArray &folders : {QByteArray("folders = \"/mnt/music\"\n"),
                                          QByteArray("folders = [\"/a/[x]\",\n  # see [y]\n  \"/c\"]\n")}) {
            QFile cfg(f);
            QVERIFY(cfg.open(QIODevice::WriteOnly | QIODevice::Truncate));
            cfg.write("density = \"compact\"\n" + folders + "font_family = \"Foo\"\n[later]\nkeep = 1\n");
            cfg.close();
            Settings s(f);
            s.addFolder(b.path());
            s.save();
            const QByteArray out = readFile(f);
            QVERIFY2(out.contains("density = \"compact\""), out);
            QVERIFY2(out.contains("font_family = \"Foo\""), out);
            QVERIFY2(out.contains("[later]\nkeep = 1"), out);
            QCOMPARE(out.count("folders"), 1);
            QVERIFY2(!out.contains("see [y]"), out);   // the old array is gone entirely
        }
    }

    void folderUrlsAreDecodedAndHomeIsExpanded()
    {
        QTemporaryDir dir, odd;
        const QString oddDir = odd.filePath("Café #1 100%");
        QVERIFY(QDir().mkpath(oddDir));
        Settings s(dir.filePath("config.toml"));
        s.addFolder(QUrl::fromLocalFile(oddDir).toString());   // what a folder dialog returns
        QVERIFY(s.folders().contains(oddDir));
    }

    void panelLookKeysParseAndValidate()
    {
        const QVariantMap d = Settings::parse("");
        QCOMPARE(d.value("panelDensity").toString(), QString("normal"));
        QCOMPARE(d.value("panelFontSize").toString(), QString("normal"));
        QCOMPARE(d.value("panelFontFamily").toString(), QString());
        QCOMPARE(d.value("panelAccent").toString(), QString("theme"));
        QCOMPARE(d.value("showAccent").toBool(), true);
        QCOMPARE(d.value("volume").toInt(), 100);
        QCOMPARE(d.value("shuffle").toBool(), false);
        QCOMPARE(d.value("repeat").toString(), QString("off"));
        QCOMPARE(d.value("eqPreset").toString(), QString("Flat"));
        const QVariantMap m2 = Settings::parse("shuffle = true\nrepeat = \"one\"\neq_preset = \"Rock\"\n");
        QCOMPARE(m2.value("shuffle").toBool(), true);
        QCOMPARE(m2.value("repeat").toString(), QString("one"));
        QCOMPARE(m2.value("eqPreset").toString(), QString("Rock"));
        QCOMPARE(Settings::parse("repeat = \"sideways\"\n").value("repeat").toString(), QString("off"));
        QCOMPARE(Settings::parse("volume = 40\n").value("volume").toInt(), 40);
        QCOMPARE(Settings::parse("volume = 900\n").value("volume").toInt(), 130);
        QCOMPARE(d.value("panelShowAccent").toBool(), true);
        QCOMPARE(Settings::parse("show_accent = false\npanel_show_accent = false\n").value("panelShowAccent").toBool(), false);

        const QVariantMap m = Settings::parse(
            "density = \"compact\"\npanel_density = \"comfortable\"\npanel_font_size = \"huge\"\n"
            "panel_font_family = \"JetBrains Mono\"\npanel_accent = \"red\"\n");
        QCOMPARE(m.value("density").toString(), QString("compact"));            // app unaffected
        QCOMPARE(m.value("panelDensity").toString(), QString("comfortable"));
        QCOMPARE(m.value("panelFontSize").toString(), QString("normal"));       // invalid -> default
        QCOMPARE(m.value("panelFontFamily").toString(), QString("JetBrains Mono"));
        QCOMPARE(m.value("panelAccent").toString(), QString("red"));
    }

    void applyKeepsAppAndPanelKeysApart()
    {
        const QByteArray in = "density = \"normal\"\nfont_size = \"normal\"\naccent = \"theme\"\n";
        const QByteArray out = Settings::apply(in, {{"panelDensity", "compact"}, {"panelFontSize", "large"},
                                                    {"panelAccent", "blue"}, {"density", "comfortable"}});
        QCOMPARE(out, QByteArray("density = \"comfortable\"\nfont_size = \"normal\"\naccent = \"theme\"\n\n"
                                 "# omamusic settings\npanel_density = \"compact\"\npanel_font_size = \"large\"\n"
                                 "panel_accent = \"blue\"\n"));
        // replacing an existing panel key in place must not touch the app key with a similar name
        const QByteArray again = Settings::apply(out, {{"panelDensity", "normal"}});
        QVERIFY(again.contains("density = \"comfortable\"\n"));
        QVERIFY(again.contains("panel_density = \"normal\"\n"));
    }

    void settingsExposesPanelScales()
    {
        QTemporaryDir dir;
        Settings s(dir.filePath("config.toml"));
        QCOMPARE(s.panelDensityScale(), 1.0);
        s.set("panelDensity", "compact");
        s.set("panelFontSize", "large");
        QCOMPARE(s.panelDensityScale(), 0.8);
        QCOMPARE(s.panelFontScale(), 1.15);
        QCOMPARE(s.densityScale(), 1.0);   // app scope independent
        QCOMPARE(s.fontScale(), 1.0);
    }

    void lookScopeMapsToTheRightKeys()
    {
        QTemporaryDir dir;
        Settings s(dir.filePath("config.toml"));
        LookScope app(&s, false);
        LookScope panel(&s, true);
        QSignalSpy appSpy(&app, &LookScope::changed);
        QSignalSpy panelSpy(&panel, &LookScope::changed);

        panel.set("density", "compact");
        QCOMPARE(s.panelDensity(), QString("compact"));
        QCOMPARE(s.density(), QString("normal"));
        QCOMPARE(panel.density(), QString("compact"));
        QCOMPARE(panel.densityScale(), 0.8);
        QCOMPARE(app.density(), QString("normal"));
        QCOMPARE(app.densityScale(), 1.0);
        QCOMPARE(panelSpy.count(), 1);
        QCOMPARE(appSpy.count(), 1);   // both scopes re-evaluate on any Settings change

        app.set("accent", "blue");
        QCOMPARE(s.accent(), QString("blue"));
        QCOMPARE(s.panelAccent(), QString("theme"));
        QCOMPARE(app.accent(), QString("blue"));
        QCOMPARE(panel.accent(), QString("theme"));

        app.set("fontSize", "small");
        panel.set("fontFamily", "DejaVu Sans");
        QCOMPARE(app.fontScale(), 0.9);
        QCOMPARE(panel.fontScale(), 1.0);
        QCOMPARE(app.fontFamily(), QString());
        QCOMPARE(panel.fontFamily(), QString("DejaVu Sans"));
    }

    void lookScopeForwardsPanelOptions()
    {
        QTemporaryDir dir;
        Settings s(dir.filePath("config.toml"));
        LookScope app(&s, false);
        LookScope panel(&s, true);
        panel.set("panelCover", false);
        panel.set("panelUpNextCount", 7);
        QCOMPARE(s.panelCover(), false);
        QCOMPARE(panel.panelCover(), false);
        QCOMPARE(app.panelCover(), false);   // same underlying value
        QCOMPARE(panel.panelUpNextCount(), 7);
        // fontFamilies() needs a QGuiApplication (QFontDatabase); this suite is guiless, so it is not called here.
    }
};

QTEST_GUILESS_MAIN(TstSettings)
#include "tst_settings.moc"
