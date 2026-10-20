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

QString configHome() { return xdg("XDG_CONFIG_HOME", ".config"); }
QString dataHome() { return xdg("XDG_DATA_HOME", ".local/share"); }
QString stateHome() { return xdg("XDG_STATE_HOME", ".local/state"); }

QString configDir()
{
    return xdg("XDG_CONFIG_HOME", ".config") + QStringLiteral("/omp");
}

QString dataDir()
{
    return xdg("XDG_DATA_HOME", ".local/share") + QStringLiteral("/omp");
}

QString stateDir()
{
    return xdg("XDG_STATE_HOME", ".local/state") + QStringLiteral("/omp");
}

QString runtimeDir()
{
    const QString base = qEnvironmentVariable("XDG_RUNTIME_DIR");
    const QString dir = base.isEmpty()
        ? QDir::tempPath() + QStringLiteral("/omp-") + QString::number(getuid())
        : base + QStringLiteral("/omp");
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
