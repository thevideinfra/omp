#pragma once

#include <QString>

// The app used to be called omamusic. On the first run under the new name, its
// config, data and state folders and its start-at-login entry move to the new names.
namespace Migrate {
// `exePath` is what the new start-at-login entry runs. Returns whether anything moved.
bool fromOmamusic(const QString &configHome, const QString &dataHome, const QString &stateHome,
                  const QString &exePath);
}
