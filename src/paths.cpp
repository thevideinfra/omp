#include "paths.h"

#include <QDir>

#include <unistd.h>

namespace {
QString xdg(const char *variable, const QString &homeRelative)
{
    const QString value = qEnvironmentVariable(variable);
    return value.isEmpty() ? QDir::homePath() + '/' + homeRelative : value;
}
}

namespace Paths {

QString configDir()
{
    return xdg("XDG_CONFIG_HOME", ".config") + QStringLiteral("/omamusic");
}

QString dataDir()
{
    return xdg("XDG_DATA_HOME", ".local/share") + QStringLiteral("/omamusic");
}

QString stateDir()
{
    return xdg("XDG_STATE_HOME", ".local/state") + QStringLiteral("/omamusic");
}

QString runtimeDir()
{
    const QString base = qEnvironmentVariable("XDG_RUNTIME_DIR");
    const QString dir = base.isEmpty()
        ? QDir::tempPath() + QStringLiteral("/omamusic-") + QString::number(getuid())
        : base + QStringLiteral("/omamusic");
    QDir().mkpath(dir);
    return dir;
}

QString omarchyUserShellFile()
{
    return xdg("XDG_CONFIG_HOME", ".config") + QStringLiteral("/omarchy/shell.toml");
}

QString omarchyThemeFile()
{
    return xdg("XDG_STATE_HOME", ".local/state")
        + QStringLiteral("/omarchy/current/theme/colors.toml");
}

}
