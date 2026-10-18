#pragma once

#include <QString>

namespace Paths {
QString configDir();         // $XDG_CONFIG_HOME/omamusic
QString stateDir();          // $XDG_STATE_HOME/omamusic
QString runtimeDir();        // $XDG_RUNTIME_DIR/omamusic, created on call
QString omarchyUserShellFile();  // $XDG_CONFIG_HOME/omarchy/shell.toml (user override)
QString omarchyThemeFile();  // $XDG_STATE_HOME/omarchy/current/theme/colors.toml
}
