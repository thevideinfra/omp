# omamusic

A Qt 6 music player for Omarchy. It browses local folders by album, artist, folder and queue, and plays through an `mpv` child process. A standalone window is tiled by Hyprland, and a tray icon opens a compact popup panel. It follows the Omarchy theme. Library is read-only.

## Dependencies

Qt 6 (Core, Gui, Widgets, Network, Quick, QuickControls2), `mpv`, `ffmpeg` (for `ffprobe`), `python3`, `cmake`, `ninja`.

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

## Keys

Main window:

| Key | Action |
|---|---|
| Space | play / pause |
| n / p | next / previous |
| h / l | seek -10 s / +10 s |
| / | focus search |
| Tab | next tab |
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

Put this in your Hyprland Lua config (verify on your setup):

```lua
hl.window_rule({
  name = "omamusic-panel",
  match = { class = "^omamusic$", title = "^omamusic-panel$" },
  float = true,
  size = "360 540",
  move = "100%-w-12 38",
})
```

A Qt client cannot place its own window on Wayland, so this rule floats the panel and positions it under the top-right of the bar.

## Known gaps

Deferred to later plans: embedded cover art, tray icon recolour, MPRIS, SMB/NFS shares, Subsonic.

## Test

```sh
ctest --test-dir build --output-on-failure
```
