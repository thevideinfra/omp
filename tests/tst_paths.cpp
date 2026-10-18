#include <QtTest>

#include "paths.h"

class TstPaths : public QObject
{
    Q_OBJECT
private slots:
    void honoursXdgOverrides()
    {
        qputenv("XDG_STATE_HOME", "/tmp/oma-state");
        qputenv("XDG_CONFIG_HOME", "/tmp/oma-config");
        QCOMPARE(Paths::stateDir(), QString("/tmp/oma-state/omamusic"));
        QCOMPARE(Paths::configDir(), QString("/tmp/oma-config/omamusic"));
        QCOMPARE(Paths::omarchyThemeFile(),
                 QString("/tmp/oma-state/omarchy/current/theme/colors.toml"));
    }

    void runtimeDirIsCreated()
    {
        QTemporaryDir base;
        qputenv("XDG_RUNTIME_DIR", base.path().toUtf8());
        const QString dir = Paths::runtimeDir();
        QCOMPARE(dir, base.path() + "/omamusic");
        QVERIFY(QDir(dir).exists());
    }
};

QTEST_GUILESS_MAIN(TstPaths)
#include "tst_paths.moc"
