#include "migrate.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "autostart.h"

namespace {
// Moves home/omamusic to home/omp unless the new one is already there.
bool moveFolder(const QString &home)
{
    const QString from = home + QStringLiteral("/omamusic"), to = home + QStringLiteral("/omp");
    if (!QFileInfo(from).isDir() || QFileInfo::exists(to))
        return false;
    if (!QDir().rename(from, to)) {
        qWarning() << "cannot move" << from << "to" << to;
        return false;
    }
    return true;
}
}

namespace Migrate {

bool fromOmamusic(const QString &configHome, const QString &dataHome, const QString &stateHome,
                  const QString &exePath)
{
    bool moved = false;
    for (const QString &home : {configHome, dataHome, stateHome})
        moved = moveFolder(home) || moved;

    const QString autostartDir = configHome + QStringLiteral("/autostart");
    const QString oldEntry = autostartDir + QStringLiteral("/omamusic.desktop");
    if (QFileInfo::exists(oldEntry)) {
        QFile::remove(oldEntry);
        Autostart(autostartDir, exePath).setEnabled(true);   // the same wish under the new name
        moved = true;
    }
    return moved;
}

}
