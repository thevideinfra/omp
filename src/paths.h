#pragma once

#include <QString>

namespace Paths {
QString configHome();        // $XDG_CONFIG_HOME, or ~/.config
QString dataHome();          // $XDG_DATA_HOME, or ~/.local/share
QString stateHome();         // $XDG_STATE_HOME, or ~/.local/state
QString configDir();         // $XDG_CONFIG_HOME/omp
QString dataDir();           // $XDG_DATA_HOME/omp
QString stateDir();          // $XDG_STATE_HOME/omp
QString runtimeDir();        // $XDG_RUNTIME_DIR/omp, created on call
QString omarchyUserShellFile();  // $XDG_CONFIG_HOME/omarchy/shell.toml (user override)
QString omarchyThemeFile();  // $XDG_STATE_HOME/omarchy/current/theme/colors.toml
}
