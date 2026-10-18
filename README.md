# omamusic

A Qt 6 music player for Omarchy. It browses local folders by album, artist, folder and queue, and plays through an `mpv` child process. A standalone window is tiled by Hyprland, and a tray icon opens a compact popup panel. It follows the Omarchy theme. Library is read-only.

## Dependencies

Qt 6 (Core, Gui, Widgets, Network, Quick, QuickControls2), `qt6-svg` (tray icon), `mpv`, `ffmpeg` (for `ffprobe`), `python3`, `cmake`, `ninja`.

## Build

```sh
cmake -S . -B build -G Ninja && cmake --build build
```

## Run

```sh
./build/omamusic           # show the main window (default verb)
./build/omamusic toggle    # show or hide the main window
./build/omamusic panel     # toggle the tray panel
./build/omamusic quit
```

A second launch sends its verb to the running instance and exits.

## Config

`~/.config/omamusic/config.toml`:

```toml
folders = ["~/Music", "/other"]
```

One-line or multi-line arrays both work. Defaults to `~/Music` when unset.

## Settings

The Settings tab (last in the Tab cycle) and the gear in the tray panel both edit the appearance and panel options. Changes apply live and are saved to `~/.config/omamusic/config.toml`. Only the keys below are written; `folders` is left untouched.

| Key | Values | Default |
|---|---|---|
| `density` | `compact`, `normal`, `comfortable` | `normal` |
| `font_size` | `small`, `normal`, `large` | `normal` |
| `font_family` | any installed family | system default (empty) |
| `accent` | `theme` or a palette colour name | `theme` |
| `panel_cover` | `true` / `false` | `true` |
| `panel_seek` | `true` / `false` | `true` |
| `panel_volume` | `true` / `false` | `true` |
| `panel_up_next` | `true` / `false` | `true` |
| `panel_up_next_count` | `0`-`10` | `4` |

The font base size comes from the Omarchy shell's `[font] base-size` in `shell.toml`, next to the colours file. The panel's sections are toggled in the gear view; the panel sizes itself to its content.

## Keys

Main window:

| Key | Action |
|---|---|
| Space | play / pause |
| n / p | next / previous |
| h / l | seek -10 s / +10 s |
| / | focus search |
| Tab | next tab (Albums, Artists, Folders, Queue, Settings) |
| j / k, Down / Up | move selection |
| Enter | play selection |
| a | add selection to queue |
| r | rescan the library |
| Backspace | up one folder (Folders tab) |
| Esc | clear search |

Panel: Esc hides the panel, Space play / pause.

## Tray

- Left-click: toggle the panel.
- Middle-click: play / pause.
- No scroll gesture.

## Hyprland panel rule

Put this in your Hyprland Lua config (tested on Hyprland 0.56.2; opens the panel centered under the mouse cursor, i.e. under the tray icon you clicked, just below the bar):

```lua
hl.window_rule({
  name = "omamusic-panel",
  match = { class = "^omamusic$", title = "^omamusic-panel$" },
  float = true,
  move = { "(cursor_x-window_w/2)", 38 },
})
```

The panel sizes itself to its content, so the rule has no `size`. A Qt client cannot place its own window on Wayland, so this rule floats the panel and positions it under the top-right of the bar.

## Known gaps

Deferred to later plans: embedded cover art, tray icon recolour, MPRIS, SMB/NFS shares, Subsonic.

## Test

```sh
ctest --test-dir build --output-on-failure
```
