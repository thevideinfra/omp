# omamusic

A Qt 6 music player for Omarchy. It browses local folders by album, artist, folder and queue, and plays through an `mpv` child process. A standalone window is tiled by Hyprland, and a tray icon opens a compact popup panel. It follows the Omarchy theme. Library is read-only.

## Install

```
cmake -S . -B build && cmake --build build
cmake --install build --prefix ~/.local
```

This puts `omamusic` and `omamusic-scan` in `~/.local/bin` and adds a launcher entry and icon. Run the install again after each rebuild.

## Dependencies

Qt 6 (Core, Gui, Widgets, Network, Quick, QuickControls2), `qt6-svg` (tray icon), `mpv`, `ffmpeg` (for `ffprobe`), `python3`, `cmake`, `ninja`, and `libsecret` (its `secret-tool` keeps the Subsonic password in the keyring; only needed for a server).

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
| m | add selection to a playlist |
| right-click | menu at the pointer: Play, Add to queue, Add to playlist, Add to / Remove from favorites (one server song), Remove from playlist (in a local playlist); Esc closes it |
| s / o | toggle shuffle / cycle repeat |
| f | star / unstar the playing server song |
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

The Artists tab has three columns: the artists under A, B, C... headings (when sorted by name), the selected artist's albums with large thumbnails, and the open album's songs under a large cover. An artist opens on its first album. Right/Left move between the columns. The columns in Albums, Artists and Playlists start out equal (halves, thirds for artists) and can be resized by dragging their dividers (Settings > App > Layout has "Remember column sizes", off by default, which stores the sizes you drag for each tab, and "Reset column sizes", which goes back to equal columns); the large album cover grows and shrinks with its column. Artists in the list show a thumbnail of their first album.

Songs sort by title, artist, album, year, length, added (newest first) or favorite (starred first), albums by title, artist, length or added, and artists by name, length or added, each with a reverse toggle; "Remember sort order" in Settings > App > Layout (off by default) keeps the three choices between runs. The left rail shows a picture and a name for each tab and collapses to just the pictures with the button at its bottom (remembered); it has an area at the top reserved for a logo. "Show local / server tags on songs" in Settings > App turns the small tags off. Songs, Queue, Search and Folders are tables (Title, Artist, Album, Time) under a header row; the columns in Albums, Artists and Playlists have titles, and the artist list has a large letter heading for each initial.

The bar under the song has **shuffle and repeat (off, all, one) buttons; `s` toggles shuffle and `o` cycles repeat. Shuffle reorders the queue and keeps the playing song; switching it off restores the order. Both are remembered. The equalizer button (sliders icon) beside the transport buttons opens a popup that picks one of the standard presets (a ten-band mpv filter), also remembered; boosts come with a small level cut so loud tracks do not clip.

## Cover art

A folder image (`cover.jpg`, `folder.jpg`, `front.jpg`, `.png` too) wins. Otherwise the art embedded in the first track of the album folder is extracted with `ffmpeg` on first view and cached in `~/.local/state/omamusic/covers/`. A folder with no embedded art is not retried until the app restarts.

## Media keys (MPRIS)

omamusic has its own MPRIS service (`org.mpris.MediaPlayer2.omamusic`, identity `omamusic`): play/pause, next/previous, seek, volume, shuffle, loop, title, artist, album and cover art show on the bus, and Raise/Quit work. mpv runs with `--load-scripts=no`, so the `mpv-mpris` script is not used. Untagged files show their file name as the title.

## Subsonic server

Settings > Library > Subsonic server connects omamusic to a Subsonic-API server (Navidrome, Airsonic, Subsonic). Enter the address, user and password, press Test connection, then Save. The password goes into the system keyring through `secret-tool` (it is never written to `config.toml`, which keeps only `server_enabled`, `server_url` and `server_user`).

- The server's songs join your own in Albums, Artists, Songs and search. A server album never merges with a local album of the same name. Folders stays local.
- The library syncs at start and with `r`. It is cached in `~/.local/state/omamusic/subsonic.jsonl`, so it opens at once and stays visible when the server is offline ("Server offline" shows in the bottom bar).
- Songs stream through mpv; covers are downloaded once and cached.
- Server playlists show in Playlists after your own, read-only, marked "server". **Favorites** lists the songs you starred. A heart button in the bottom bar (or `f`) stars or unstars the playing server song.
- Plays are reported to the server ("now playing" at the start, a scrobble after half the song or four minutes).
- Remove (in the same settings section) forgets the password, the cached songs and the settings.
- Not included: offline downloads, several servers, editing server playlists.

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

Network shares: a music folder that is missing, empty or stops answering (unmounted or hung NFS/SMB; the scan gives up after 20 s without progress) is shown as offline in Settings > Library and keeps its last known songs, while the other folders rescan normally. Covers and change watching skip folders that have not been confirmed reachable, so a dead mount cannot freeze the window. Playing a song on a dead mount is still up to mpv.

The tray icon follows the theme foreground (checked against a throwaway instance on the live bar: the icon changed colour within two seconds of a theme file change).

## Test

```sh
ctest --test-dir build --output-on-failure
```
