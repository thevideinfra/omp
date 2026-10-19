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
./build/omamusic tray      # start in the tray, no window (used by Start at login)
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

The Settings tab (last in the Tab cycle) edits the appearance and panel options. It has two sections: **App** (the main window) and **Tray panel**. Each has its own density, font size, font and accent. The Tray panel section also has the section toggles and the up-next count. The gear at the top right of the tray panel opens this tab. Changes apply live and are saved to `~/.config/omamusic/config.toml`. Only the keys below are written; `folders` is left untouched. Managed keys must be plain `key = "value"` lines with no trailing comments. The font family applies to the app's text, not the tray menu.

Tray panel changes show the next time the panel opens, because Hyprland does not let a floating window resize itself while it is open.

| Key | Values | Default |
|---|---|---|
| `density` | `compact`, `normal`, `comfortable` | `normal` |
| `font_size` | `small`, `normal`, `large` | `normal` |
| `font_family` | any installed family | system default (empty) |
| `accent` | `theme` or a palette colour name | `theme` |
| `show_accent` | `true` / `false` (off = neutral foreground instead of accent) | `true` |
| `panel_show_accent` | `true` / `false` | `true` |
| `panel_density` | `compact`, `normal`, `comfortable` | `normal` |
| `panel_font_size` | `small`, `normal`, `large` | `normal` |
| `panel_font_family` | any installed family | system default (empty) |
| `panel_accent` | `theme` or a palette colour name | `theme` |
| `volume` | `0`-`130`, saved when you change the volume | `100` |
| `shuffle` | `true` / `false` (shuffle when a queue starts) | `false` |
| `repeat` | `off`, `all`, `one` | `off` |
| `eq_preset` | `Flat`, `Bass Boost`, `Treble Boost`, `Rock`, `Pop`, `Jazz`, `Classical`, `Vocal`, `Dance`, `Loudness` | `Flat` |
| `panel_cover` | `true` / `false` | `true` |
| `panel_seek` | `true` / `false` | `true` |
| `panel_volume` | `true` / `false` | `true` |
| `panel_up_next` | `true` / `false` | `true` |
| `panel_up_next_count` | `0`-`10` | `4` |

The font base size comes from the Omarchy shell's `[font] base-size`: your `~/.config/omarchy/shell.toml` wins, otherwise the theme's `shell.toml` next to the colours file. The panel's look keys (`panel_density`, `panel_font_size`, `panel_font_family`, `panel_accent`) are new: if you customised the app density or font before, the panel starts at normal/theme, and you can set it in Settings > Tray panel. The panel's sections are toggled in the Tray panel section of the Settings tab. The panel sizes itself to its content when it opens; the volume row shows the volume percentage.

## Keys

Main window:

| Key | Action |
|---|---|
| Space | play / pause |
| n / p | next / previous |
| h / l | seek -10 s / +10 s |
| / | focus search |
| Tab | next tab (Albums, Artists, Songs, Playlists, Folders, Queue, Settings) |
| Right / Left | Albums, Artists, Playlists: move into / out of the song pane |
| j / k, Down / Up | move selection |
| Enter | play selection |
| a | add selection to queue |
| m | add selection to a playlist (also right-click) |
| s / o | toggle shuffle / cycle repeat |
| Delete | remove the selected song from the open playlist |
| r | rescan the library |
| Backspace | up one folder (Folders tab); out of the song pane |
| Esc | clear search; out of the song pane |

Panel: Esc hides the panel, Space play / pause.

## New files and start at login

Changes under your music folders are noticed on their own: omamusic watches every directory (up to 4096) and rescans 3 seconds after the last change, probing only new or changed files. With more directories, or a folder that cannot be watched, it rescans every 5 minutes instead. `r` still forces a rescan.

The App section of Settings has **Start at login (in the tray)**, off by default. It writes `~/.config/autostart/omamusic.desktop` with `Exec="<path to omamusic>" tray`, which your session runs at login, and removes it when switched off. The `tray` verb starts the app in the tray with no window and does nothing to an instance that is already running. The entry points at the binary you ran it from, so switch it off and on again after moving the binary.

## Tray

- Left-click: toggle the panel.
- Middle-click: play / pause.
- No scroll gesture.
- The tray icon is drawn from the theme foreground at startup and on a theme change.

## Songs and music folders

The **Songs** tab lists every track by title; Enter or double-click plays one song, `a` adds it to the queue. The **Library** tab in Settings lists the music folders (default `~/Music`): add one by typing its path or with Browse…, remove with Remove (the last one stays). Changes rescan at once and are saved as `folders = [...]` in `config.toml`. A mounted network share is just another folder here; handling of unmounted shares and slow mounts comes with the shares milestone.

## Playlists

Playlists are `.m3u` files (one path per line) in `~/.local/share/omamusic/playlists/`. The **Playlists** tab lists them with their songs on the right; Enter or double-click plays one, and **New playlist**, **Rename** and **Delete** (click twice) manage them. Add songs from anywhere with right-click or `m` on a song, album or artist, which opens a picker with your playlists and a field for a new one. The Queue tab has **Save as playlist**. Delete removes the selected song from the open playlist. Songs that are no longer in the library are skipped when a playlist is shown but stay in the file.

## Shuffle, repeat and equalizer

The bar under the song has **Shuffle** and **Repeat** (off, all, one) buttons; `s` toggles shuffle and `o` cycles repeat. Shuffle reorders the queue and keeps the playing song; switching it off restores the order. Both are remembered. The Equalizer tab in Settings picks one of the standard presets (a ten-band mpv filter), also remembered; boosts come with a small level cut so loud tracks do not clip.

## Cover art

A folder image (`cover.jpg`, `folder.jpg`, `front.jpg`, `.png` too) wins. Otherwise the art embedded in the first track of the album folder is extracted with `ffmpeg` on first view and cached in `~/.local/state/omamusic/covers/`. A folder with no embedded art is not retried until the app restarts.

## Media keys (MPRIS)

MPRIS comes from the `mpv-mpris` mpv script, not from omamusic. Checked: play/pause, pause, title, album artist and the cover art (embedded art included) show on the bus. The player shows as `mpv`. Untagged files show their file name as the title.

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

Deferred to later plans: a native MPRIS identity (the player shows as `mpv`), SMB/NFS shares, Subsonic. The tray icon recolour is implemented but not confirmed to change on a theme switch.

## Test

```sh
ctest --test-dir build --output-on-failure
```
