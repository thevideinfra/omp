# OMP

OMP (Omarchy Music Player) is a Qt 6 music player for Omarchy. It was called omamusic until 2026-10-19: on first start it moves your old settings, playlists, caches, saved server password and start-at-login entry to the new names. It browses local folders by album, artist, folder and queue, and plays through an `mpv` child process. A standalone window is tiled by Hyprland, and a tray icon opens a compact popup panel. It follows the Omarchy theme. Library is read-only.

## Install

```
cmake -S . -B build && cmake --build build
cmake --install build --prefix ~/.local
```

This puts `omp` and `omp-scan` in `~/.local/bin` and adds a launcher entry and icon. Run the install again after each rebuild.

## Dependencies

Qt 6 (Core, Gui, Widgets, Network, Quick, QuickControls2), `qt6-svg` (tray icon), `mpv`, `ffmpeg` (for `ffprobe`), `python3`, `cmake`, `ninja`, and `libsecret` (its `secret-tool` keeps the Subsonic password in the keyring; only needed for a server).

## Build

```sh
cmake -S . -B build -G Ninja && cmake --build build
```

## Run

```sh
./build/omp           # show the main window (default verb)
./build/omp toggle    # show or hide the main window
./build/omp panel     # toggle the tray panel
./build/omp tray      # start in the tray, no window (used by Start at login)
./build/omp quit
```

A second launch sends its verb to the running instance and exits.

## Config

`~/.config/omp/config.toml`:

```toml
folders = ["~/Music", "/other"]
```

One-line or multi-line arrays both work. Defaults to `~/Music` when unset.

## Settings

The Settings tab (last in the Tab cycle) edits the appearance and panel options. It has two sections: **App** (the main window) and **Tray panel**. Each has its own density, font size, font and accent. The Tray panel section also has the section toggles and the up-next count. The gear at the top right of the tray panel opens this tab. Changes apply live and are saved to `~/.config/omp/config.toml`. Only the keys below are written; `folders` is left untouched. Managed keys must be plain `key = "value"` lines with no trailing comments. The font family applies to the app's text, not the tray menu.

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
| Tab | next tab (Albums, Artists, Songs, Playlists, Libraries, Queue, Settings) |
| Right / Left | Albums, Artists, Playlists: move into / out of the song pane |
| j / k, Down / Up | move selection |
| Enter | play selection |
| a | add selection to queue |
| m | add selection to a playlist |
| right-click | menu at the pointer: Play, Play next, Add to queue, Remove from queue / Move up / Move down (Queue tab), Add to playlist (a submenu: New playlist… and your playlists), Go to artist / album (one song), Add to / Remove from favorites (one server song), Remove from playlist (in a local playlist); Esc closes it |
| s / o | toggle shuffle / cycle repeat |
| f | star / unstar the playing server song |
| Delete | remove the selected song from the open playlist, or from the queue (Queue tab) |
| Shift + Up / Down | move the selected song up or down in the queue (Queue tab); songs can also be dragged |
| r | rescan the library |
| Backspace | up one folder (Libraries tab); out of the song pane |
| Esc | clear search; out of the song pane |

Panel: Esc hides the panel, Space play / pause.

## New files and start at login

Changes under your music folders are noticed on their own: OMP watches every directory (up to 4096) and rescans 3 seconds after the last change, probing only new or changed files. With more directories, or a folder that cannot be watched, it rescans every 5 minutes instead. `r` still forces a rescan.

The App section of Settings has **Start at login (in the tray)**, off by default. It writes `~/.config/autostart/omp.desktop` with `Exec="<path to omp>" tray`, which your session runs at login, and removes it when switched off. The `tray` verb starts the app in the tray with no window and does nothing to an instance that is already running. The entry points at the binary you ran it from, so switch it off and on again after moving the binary.

## Tray

- Left-click: toggle the panel. Double-click (two clicks within the system double-click time): open the full window instead.
- Middle-click: play / pause.
- No scroll gesture.
- The tray icon is drawn from the theme foreground at startup and on a theme change.

## Songs and music folders

The **Libraries** tab (the old Folders tab) shows your music folders and what is inside them: **+ Add folder…** adds one, right-click a folder at the top to remove it (the last one stays) or to play or queue everything in it, and **← Back** with the folder's path goes up a level. When a Subsonic server is set up it is listed under the folders as a status card with the server software (Navidrome, Airsonic and so on, from the server's own ping) and its state (connected, syncing, offline or off), greyed out when it is not connected. The **Songs** tab lists every track by title; Enter or double-click plays one song, `a` adds it to the queue. The **Library** tab in Settings lists the music folders (default `~/Music`): add one by typing its path or with Browse…, remove with Remove (the last one stays). Changes rescan at once and are saved as `folders = [...]` in `config.toml`. A mounted network share is just another folder here; handling of unmounted shares and slow mounts comes with the shares milestone.

## Playlists

Playlists are `.m3u` files (one path per line) in `~/.local/share/omp/playlists/`. The **Playlists** tab lists them with their songs on the right; Enter or double-click plays one, and **New playlist**, **Rename** and **Delete** (click twice) manage them. Add songs from anywhere with right-click or `m` on a song, album or artist, which opens a picker with your playlists and a field for a new one. The Queue tab has **Save as playlist**. In the Queue the accent highlight is always the song that is playing and moves with it; a row you click or select with the keys gets only a quiet highlight and an outline. Delete removes the selected song from the open playlist. Songs that are no longer in the library are skipped when a playlist is shown but stay in the file.

## Shuffle, repeat and equalizer

The Artists tab has three columns: the artists under A, B, C... headings (when sorted by name), the selected artist's albums with large thumbnails, and the open album's songs under a large cover. An artist opens on its first album. Albums show their year after the artist name ("Artist · 2019", the earliest year of their songs). The **Hide albums / Show albums** button in the Artists title bar (or Settings > App > Lists) switches to a two-column view where the songs column lists every album of the artist, each under its own header. Right/Left move between the columns. The columns in Albums, Artists and Playlists start out equal (halves, thirds for artists) and can be resized by dragging their dividers (Settings > App > Layout has "Remember column sizes", off by default, which stores the sizes you drag for each tab, and "Reset column sizes", which goes back to equal columns); the large album cover grows and shrinks with its column. Artists in the list show a thumbnail of their first album.

Songs sort by title, artist, album, year, length, added (newest first) or favorite (starred first), albums by title, artist or added, and artists by name or added; every sort is a click on a header (ascending first, descending on the second click), in the table headers for Songs and in a header strip above the Albums and Artists lists; "Remember sort order" in Settings > App > Layout (off by default) keeps the three choices, and the Name / Year choice of the Albums column in Artists, between runs. The left rail shows a picture and a name for each tab and collapses to just the pictures with the button at its bottom (remembered); its top has the OMP logo, drawn in the theme's accent colour. The moon button in the player bar starts a **sleep timer**: pause in 15, 30, 45, 60 or 90 minutes, or after the current song (the next song then waits, paused); the time left shows at the top right of the bar, and the same menu cancels it. Settings > App > Playback sets loudness matching (ReplayGain: off, track or album, with a pre-amp of -15 to +15 dB; clipping is avoided by lowering the gain) and gapless playback. Settings > App > Lists has "Hide server songs I already have locally" (off by default): a server song is left out of the lists when a local song has the same artist, title and album and a length within two seconds; playlists can still find the hidden copy. Settings > App has toggles for which tabs the left panel shows (Settings itself always shows, and one other tab always stays), for the local / server tags on songs and for remembering the sort order. Rows highlight under the pointer. Songs, Queue and Search are tables under a header row. The default columns are Title, Artist, Album, Year and Time; right-click a header to also show Album artist, Track number, Added, Size, Format, Favorite (a heart) and Playlists (the playlists a song is in). In Songs, click a header to sort by it (first click ascending, the next descending; the arrow shows which); the columns in Albums, Artists and Playlists have titles, and the artist list has a large letter heading for each initial. The Albums column in Artists has its own sort strip, Name or Year (click again to turn it round; albums with no year go last).

The bar under the song has **shuffle and repeat (off, all, one) buttons; `s` toggles shuffle and `o` cycles repeat. Shuffle reorders the queue and keeps the playing song; switching it off restores the order. Both are remembered. The equalizer button (sliders icon) beside the transport buttons opens a popup that picks one of the standard presets (a ten-band mpv filter), also remembered; boosts come with a small level cut so loud tracks do not clip.

## Cover art

A folder image (`cover.jpg`, `folder.jpg`, `front.jpg`, `.png` too) wins. Otherwise the art embedded in the first track of the album folder is extracted with `ffmpeg` on first view and cached in `~/.local/state/omp/covers/`. A folder with no embedded art is not retried until the app restarts.

## Media keys (MPRIS)

OMP has its own MPRIS service (`org.mpris.MediaPlayer2.omp`, identity `OMP`): play/pause, next/previous, seek, volume, shuffle, loop, title, artist, album and cover art show on the bus, and Raise/Quit work. mpv runs with `--load-scripts=no`, so the `mpv-mpris` script is not used. Untagged files show their file name as the title.

## Subsonic servers

Settings > Library > Subsonic servers lists your servers (Navidrome, Airsonic, Subsonic) and adds more. For each, enter the address, user and password, press Test connection, then Add. Every server has its own On/Off switch, Edit (leave the password empty to keep it) and Remove. Each password goes into the system keyring through `secret-tool` (it is never written to a file; `servers.json` next to `config.toml` keeps the address, user, whether it is on, and which software it reported).

- The songs of all servers join your own in Albums, Artists, Songs and search. Albums with the same name never merge across servers or with local albums. Libraries stays local, and lists each server as a status card.
- Each library syncs at start and with `r`. Each is cached in `~/.local/state/omp/subsonic-<id>.jsonl`, so it opens at once and stays visible when its server is offline ("Server offline" shows in the bottom bar).
- Songs stream through mpv; covers are downloaded once and cached.
- Server playlists show in Playlists after your own, marked with their server, and can be edited like local ones: create (right-click > Add to playlist > New playlist on <server>…), Rename, Delete, remove a song with `Delete`, and add songs from anywhere in the Add to playlist submenu (only that server's own songs go in; the server answers each change and the list is fetched again, and a refusal shows at the top right of the player bar). **Favorites** (per server) lists the songs you starred and is read-only (star and unstar with the heart). A heart button in the bottom bar (or `f`) stars or unstars the playing server song, on the server that owns it.
- Plays are reported to the server a song came from ("now playing" at the start, a scrobble after half the song or four minutes).
- Remove forgets that server's password, cached songs and settings.
- An older single-server setup is carried over as the first server (its settings, cache and keyring password).
- Not included: offline downloads.

## YouTube Music

Optional, and unofficial. Install `python-ytmusicapi` and `yt-dlp`. Sign in to music.youtube.com in your browser, then in Settings > Library > YouTube Music pick that browser (OMP lists the Brave, Chromium, Chrome, Edge, Vivaldi and Firefox profiles it finds). The `omp-ytm` helper reads the YouTube and Google cookies of that profile through `yt-dlp`'s cookie reader on every sync, so the login stays as fresh as the browser's own; only the browser's name is kept in the system keyring (through `secret-tool`), never a cookie. If it says "Not signed in", sign in again in that browser. "Paste a request instead" is the fallback: a request copied as cURL from music.youtube.com (F12 > Network, filter "browse", right-click > Copy > Copy as cURL), kept in the keyring, which goes stale when the browser refreshes its cookies. Your liked songs, library songs, saved albums and playlists join the library with a **YT** tag (cached in `subsonic-ytm.jsonl` in the state folder, shown at once on the next start, refreshed in the background; Refresh does it by hand). Your own playlists can be changed from OMP (see below); ones you saved from others, and Liked Music, are read-only. A song plays from `https://music.youtube.com/watch?v=<id>` through mpv and `yt-dlp`, which YouTube's terms of service do not allow and which can break when YouTube changes. There is no ReplayGain or scrobbling for these songs, and gapless is unreliable. The heart (in a menu as **Like on YouTube Music**, the heart in the bottom bar, or `f`) likes or unlikes a song on YouTube Music; the songs of Liked Music carry a heart. "YouTube Music in its own tab" gives it an area of its own: a **YouTube Music** row at the bottom of the left rail, below Queue and a line, and its songs leave Songs, Albums and Artists. The area opens on **Home**, rows of cards from your account: **Listen again** (what you played lately on YouTube Music, read from its history), **Liked songs**, **Your playlists**, **Your albums** and **Your artists** (a row with nothing in it is left out; **See all** opens that view of the Library; click a song card to play it and the songs after it, an album, artist or playlist card to open its page). A bar across the top chooses **Home**, **Library** or **Search**; Library has **Songs** (a table in the order YouTube gives them; a click on a header sorts, as in Songs) and **Artists**, **Albums** and **Playlists** as grids of cover cards (a round cover for an artist; the first song's art stands for a playlist). Hover a card for a round Play button, click it to open its page, right-click it for the menu. A page (**Back**, `Esc` or `Backspace` goes back) has a big cover, the title and buttons (**Play**, **Shuffle**, **Add to queue**, **＋ Library** on an album, **Rename** and **Delete** on your own playlists) above its songs; an artist's page opens at once with your songs by that artist and then, when YouTube answers, turns into YouTube's own page: its **Top songs**, one shelf of **Albums** with the **Singles and EPs** after a vertical divider (the whole shelf, not only what you have; **Year** and **Name** above it sort the albums and the singles, a click again turns it round; an album of it opens at once, shows Loading… and is read from YouTube, and an album read once opens instantly after that). **Radio** on a page, or **Start radio** in the menu of a YouTube Music song, plays an endless-style list of songs like it (the song first). The top of a tall page scrolls so the songs stay in reach. The songs table of every page sorts by a click on a header, as in Songs. An album or artist shows a YouTube Music song once even when YouTube lists it under two ids (one in a playlist or your liked songs, one in the album); the Songs table keeps both, because liking and playlists need each id. The arrow keys move over the cards. **Search**: type words and press Enter to look up songs and albums on all of YouTube Music, not only your library. A search asks for up to 40 songs and 40 albums; **Show more results** (it appears when the search came back full) asks for up to 120. A song plays or queues like any other (double-click, `a`, right-click); double-click an album to play it, `a` to queue it, or right-click it for the menu. The search box above the window filters the Library, Artists and Albums views by title, artist and album while that area is open. "Show YouTube Music playlists in Playlists" (on by default) decides whether its playlists are also listed in the Playlists tab. The **Playlists** grid has a **New playlist** button; your own playlists can be renamed, deleted and have songs removed (`Delete`) like local ones, and any YouTube Music song can be added to one from the right-click menu (Add to playlist, or **New playlist on YouTube Music…**). A change shows at once and a refusal puts it back and says why at the top right of the player bar. **Add album to YouTube Music library** (right-click a song or an album of YouTube Music) saves an album to your library. Right-click a song and choose **Play album** to play its whole album from the first track: a YouTube Music song plays the full album as YouTube has it, any other song the album's songs in your library.

## Hyprland panel rule

Put this in your Hyprland Lua config (tested on Hyprland 0.56.2; opens the panel centered under the mouse cursor, i.e. under the tray icon you clicked, just below the bar):

```lua
hl.window_rule({
  name = "omp-panel",
  match = { class = "^omp$", title = "^omp-panel$" },
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
