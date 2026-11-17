import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Scale.js" as S

// The YouTube Music area. A top bar chooses Home (rows of cards), Library or Search; Library has Songs (a table) and Artists, Albums
// and Playlists (grids of cards). Opening a card shows its page (an album, a playlist or an artist) with a
// Back button. The search box above the window filters the Library views by title, artist and album.
Item {
    id: root
    property string section: "Home"      // Home, Library or Search
    property string view: "Songs"        // in Library: Songs, Artists, Albums or Playlists
    property var pages: []               // the pages opened, one on top of the other; the last one shows
    readonly property var page: pages.length > 0 ? pages[pages.length - 1] : null
    signal clicked()
    signal columnsRequested(real x, real y)
    signal pickRequested(var tracks)
    signal contextRequested(var tracks, real x, real y, bool canRemove)
    signal newPlaylistRequested()

    // A click on a column header: the first click sorts ascending, the next on the same one descending.
    function sortBy(key) {
        if (library.separateSort === key) {
            library.separateDescending = !library.separateDescending
        } else {
            library.separateSort = key
            library.separateDescending = false
        }
    }

    // Goes back one page; false when there is none (the key handler then does something else with the key).
    function back() {
        if (pages.length === 0) return false
        pages = pages.slice(0, -1)
        return true
    }
    function push(p) { pages = pages.concat([p]) }

    function minutes(tracks) {
        let total = 0
        tracks.forEach(t => total += t.duration ?? 0)
        return qsTr("%1 songs  ·  %2 min").arg(tracks.length).arg(Math.max(1, Math.round(total / 60)))
    }

    function openAlbum(key, title, subtitle, cover) {
        const tracks = library.separateAlbumTracks(key)
        push({ kind: "album", key: key, title: title, subtitle: subtitle, cover: cover,
               albumId: tracks.length > 0 ? (tracks[0].albumId ?? "") : "", meta: minutes(tracks), tracks: tracks })
    }

    // An album that is not in the library (an artist's page, as YouTube has it): its songs are read first.
    property var pendingAlbums: ({})
    function openYoutubeAlbum(browseId, title, subtitle, cover) {
        const wanted = pendingAlbums
        wanted[browseId] = { title: title, subtitle: subtitle, cover: cover }
        pendingAlbums = wanted
        // The page opens at once; its songs fill in when YouTube has answered (at once for one read before).
        push({ kind: "album", key: browseId, title: title, subtitle: subtitle, cover: cover, albumId: browseId,
               meta: qsTr("Loading…"), tracks: [], loading: true })
        ytm.fetchAlbum(browseId, "open")
    }

    function openArtist(key, title, cover) {
        const tracks = library.separateArtistTracks(key)
        const seen = {}, albums = []
        tracks.forEach(t => {
            const k = library.albumKeyFor(t.path)
            if (k === "" || seen[k]) return
            seen[k] = true
            const group = tracks.filter(x => library.albumKeyFor(x.path) === k)
            const year = Math.min(...group.map(x => (x.year ?? 0) > 0 ? x.year : 9999))
            albums.push({ key: k, title: t.album, subtitle: year < 9999 ? String(year) : "", cover: t.path })
        })
        const artistId = tracks.length > 0 ? (tracks[0].artistId ?? "") : ""
        push({ kind: "artist", key: key, title: title, subtitle: qsTr("%1 albums").arg(albums.length), cover: cover,
               meta: minutes(tracks), tracks: tracks, albums: albums, artistId: artistId })
        if (artistId !== "" && ytm.loggedIn) ytm.fetchArtist(artistId)   // YouTube's own page for the artist follows
    }

    function openPlaylist(key, title, playlistId, editable, cover) {
        const tracks = library.tracksForPaths(playlists.paths(key))
        push({ kind: "playlist", key: key, title: title, subtitle: "YouTube Music", cover: cover,
               meta: minutes(tracks), playlistId: playlistId, editable: editable, tracks: tracks })
    }

    // The list the keyboard works on (the same calls as the other tabs).
    readonly property var current: root.page ? pageView.list
                                   : root.section === "Search" ? foundSongs
                                   : root.section === "Home" ? home
                                   : stack.children[stack.currentIndex]

    // Where an album's menu was asked for: the songs arrive a moment later.
    property real menuX: 0
    property real menuY: 0
    Connections {
        target: ytm
        function onArtistFetched(channelId, data) {
            // The artist page opened from the library gets YouTube's top songs, albums and singles when they arrive.
            const top = root.page
            if (!top || top.kind !== "artist" || top.artistId !== channelId || !data.name) return
            const songs = data.tracks.length > 0 ? data.tracks : top.tracks
            const enriched = Object.assign({}, top, { youtube: true, top: data.tracks.length > 0, tracks: songs, meta: root.minutes(songs),
                                                      albums: data.albums, singles: data.singles, subtitle: qsTr("%1 albums").arg(data.albums.length) })
            root.pages = root.pages.slice(0, -1).concat([enriched])
        }
        function onAlbumFetched(id, purpose, tracks) {
            if (purpose === "open") {
                const info = root.pendingAlbums[id]
                const at = root.pages.findIndex(p => p.key === id && p.loading)
                if (!info || at < 0) return               // the page was left meanwhile
                if (tracks.length === 0) { root.pages = root.pages.filter((p, i) => i !== at); return }   // it failed: the player bar says why
                root.pages = root.pages.map((p, i) => i === at ? { kind: "album", key: id, title: info.title, subtitle: info.subtitle,
                                                                   cover: info.cover, albumId: id, meta: root.minutes(tracks), tracks: tracks } : p)
                return
            }
            if (tracks.length === 0) return
            if (purpose === "play") player.playTracks(tracks, 0)
            else if (purpose === "queue") player.enqueueTracks(tracks)
            else if (purpose === "pick") root.pickRequested(tracks)
            else if (purpose === "menu") root.contextRequested(tracks, root.menuX, root.menuY, false)
        }
    }

    // The songs a card of a grid stands for.
    function albumSongs(row) { return library.separateAlbumTracks(library.separateAlbums.get(row).key) }
    function artistSongs(row) { return library.separateArtistTracks(library.separateArtists.get(row).key) }
    function playlistSongs(row) { return library.tracksForPaths(playlists.paths(playlists.listOf("ytm").get(row).key)) }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {   // the top bar
            Layout.fillWidth: true
            Layout.margins: S.sp(8)
            spacing: S.sp(12)

            ChipButton { visible: root.page !== null; text: "‹  " + qsTr("Back"); onClicked: { root.back(); root.clicked() } }
            Segmented {
                visible: root.page === null
                options: ["Home", "Library", "Search"]
                selected: root.section
                onPicked: (value) => { root.section = value; root.clicked() }
            }
            Segmented {
                visible: root.page === null && root.section === "Library"
                options: ["Songs", "Artists", "Albums", "Playlists"]
                selected: root.view
                onPicked: (value) => { root.view = value; root.clicked() }
            }
            Item { Layout.fillWidth: true }
            ChipButton {
                visible: root.page === null && root.section === "Library" && root.view === "Playlists" && ytm.loggedIn
                text: qsTr("New playlist")
                onClicked: root.newPlaylistRequested()
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            YtmHeaderPage {
                id: pageView
                visible: root.page !== null
                anchors.fill: parent
                page: root.page ?? ({})
                tracks: { playlists.revision; return !root.page ? [] : root.page.kind === "playlist" ? library.tracksForPaths(playlists.paths(root.page.key)) : root.page.tracks }
                onClicked: root.clicked()
                onPlayRequested: (tracks, start) => player.playTracks(tracks, start)
                onEnqueueRequested: (tracks) => player.enqueueTracks(tracks)
                onPickRequested: (tracks) => root.pickRequested(tracks)
                onContextRequested: (tracks, x, y, canRemove) => root.contextRequested(tracks, x, y, canRemove)
                onAlbumOpened: (key, title, subtitle, cover) => root.page && root.page.youtube ? root.openYoutubeAlbum(key, title, subtitle, cover)
                                                                                         : root.openAlbum(key, title, subtitle, cover)
                onAlbumPlayRequested: (key) => root.page && root.page.youtube ? ytm.fetchAlbum(key, "play") : player.playTracks(library.separateAlbumTracks(key), 0)
                onAlbumMenuRequested: (key, x, y) => {
                    if (root.page && root.page.youtube) { root.menuX = x; root.menuY = y; ytm.fetchAlbum(key, "menu") }
                    else root.contextRequested(library.separateAlbumTracks(key), x, y, false)
                }
                onRadioRequested: (path) => ytm.startRadio(path)
                onLikeAlbumRequested: (id) => ytm.likeAlbum(id)
                onRenameRequested: (name) => { ytm.renamePlaylist("ytm", root.page.playlistId, name); root.back() }
                onDeleteRequested: { ytm.deletePlaylist("ytm", root.page.playlistId); root.back() }
                onRemoveRequested: (index) => ytm.removeFromPlaylist("ytm", root.page.playlistId, index)
            }

            StackLayout {
                id: stack
                visible: root.page === null
                anchors.fill: parent
                currentIndex: root.section === "Search" ? 5 : root.section === "Home" ? 4 : ["Songs", "Artists", "Albums", "Playlists"].indexOf(root.view)

                BrowseList {   // the songs, in the order YouTube gives them until a header is clicked
                    onClicked: root.clicked()
                    songRows: true
                    table: true
                    columns: settingsStore.songColumns
                    onColumnsRequested: (x, y) => root.columnsRequested(x, y)
                    sortable: true
                    sortKey: library.separateSort
                    sortDescending: library.separateDescending
                    onSortRequested: (key) => root.sortBy(key)
                    model: library.separateSongs
                    onActivated: (row) => player.playTracks([library.separateSongs.get(row)], 0)
                    onEnqueued: (row) => player.enqueue(library.separateSongs.get(row))
                    onMenuRequested: (row) => root.pickRequested([library.separateSongs.get(row)])
                    onContextRequested: (row, x, y) => root.contextRequested([library.separateSongs.get(row)], x, y, false)
                }

                YtmGrid {
                    round: true
                    icon: "artists"
                    model: library.separateArtists
                    onOpened: (row) => { const a = library.separateArtists.get(row); root.openArtist(a.key, a.title, a.cover); root.clicked() }
                    onPlayed: (row) => player.playTracks(root.artistSongs(row), 0)
                    onQueued: (row) => player.enqueueTracks(root.artistSongs(row))
                    onMenuRequested: (row) => root.pickRequested(root.artistSongs(row))
                    onContextRequested: (row, x, y) => root.contextRequested(root.artistSongs(row), x, y, false)
                }

                YtmGrid {
                    icon: "albums"
                    model: library.separateAlbums
                    onOpened: (row) => { const a = library.separateAlbums.get(row); root.openAlbum(a.key, a.title, a.subtitle, a.cover); root.clicked() }
                    onPlayed: (row) => player.playTracks(root.albumSongs(row), 0)
                    onQueued: (row) => player.enqueueTracks(root.albumSongs(row))
                    onMenuRequested: (row) => root.pickRequested(root.albumSongs(row))
                    onContextRequested: (row, x, y) => root.contextRequested(root.albumSongs(row), x, y, false)
                }

                YtmGrid {
                    icon: "playlists"
                    model: playlists.listOf("ytm")
                    onOpened: (row) => { const l = playlists.listOf("ytm").get(row); root.openPlaylist(l.key, l.title, l.playlistId, !l.readOnly, root.playlistSongs(row)[0]?.path ?? ""); root.clicked() }
                    onPlayed: (row) => player.playTracks(root.playlistSongs(row), 0)
                    onQueued: (row) => player.enqueueTracks(root.playlistSongs(row))
                    onMenuRequested: (row) => root.pickRequested(root.playlistSongs(row))
                    onContextRequested: (row, x, y) => root.contextRequested(root.playlistSongs(row), x, y, false)
                }

                YtmHome {
                    id: home
                    onSongsPlayed: (tracks, start) => player.playTracks(tracks, start)
                    onContextRequested: (tracks, x, y, canRemove) => root.contextRequested(tracks, x, y, canRemove)
                    onAlbumOpened: (key, title, subtitle, cover) => root.openAlbum(key, title, subtitle, cover)
                    onArtistOpened: (key, title, cover) => root.openArtist(key, title, cover)
                    onPlaylistOpened: (row) => { const l = playlists.listOf("ytm").get(row); root.openPlaylist(l.key, l.title, l.playlistId, !l.readOnly, root.playlistSongs(row)[0]?.path ?? "") }
                    onSeeAll: (view) => { root.view = view; root.section = "Library" }
                    onVisibleChanged: if (visible) ytm.refreshHistory()
                }

            ColumnLayout {   // songs and albums from all of YouTube Music, not only your library
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: S.sp(8)
                    spacing: S.sp(8)
                    TextField {
                        id: query
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search YouTube Music for songs and albums")
                        color: theme.foreground
                        placeholderTextColor: theme.muted
                        selectByMouse: true
                        font.pixelSize: S.fs(13)
                        font.family: ui.fontFamily !== "" ? ui.fontFamily : Qt.application.font.family
                        background: Rectangle { radius: S.sp(6); color: theme.surface }
                        onAccepted: { ytm.search(text); root.clicked() }
                        Keys.onEscapePressed: root.clicked()
                    }
                    ChipButton { text: qsTr("Search"); enabled: query.text.trim() !== "" && !ytm.searching; onClicked: { ytm.search(query.text); root.clicked() } }
                    ChipButton { visible: ytm.canShowMore; text: qsTr("Show more results"); onClicked: { ytm.showMore(); root.clicked() } }
                }

                UiText {
                    visible: ytm.searchStatus !== ""
                    Layout.leftMargin: S.sp(12)
                    Layout.bottomMargin: S.sp(6)
                    text: ytm.searchStatus
                    color: theme.dimForeground
                    px: 12
                }

                SplitView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    orientation: Qt.Horizontal

                    ColumnLayout {
                        SplitView.fillWidth: true
                        SplitView.minimumWidth: S.sp(240)
                        spacing: 0
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: S.sp(30)
                            color: theme.sunken
                            UiText { x: S.sp(12); anchors.verticalCenter: parent.verticalCenter; text: qsTr("Songs") + "  (" + foundSongs.count + ")"; color: theme.dimForeground; px: 12 }
                        }
                        BrowseList {
                            id: foundSongs
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            onClicked: root.clicked()
                            songRows: true
                            model: ytm.searchSongs
                            onActivated: (row) => player.playTracks([ytm.searchSongs.get(row)], 0)
                            onEnqueued: (row) => player.enqueue(ytm.searchSongs.get(row))
                            onMenuRequested: (row) => root.pickRequested([ytm.searchSongs.get(row)])
                            onContextRequested: (row, x, y) => root.contextRequested([ytm.searchSongs.get(row)], x, y, false)
                        }
                    }

                    ColumnLayout {
                        SplitView.preferredWidth: S.sp(340)
                        SplitView.minimumWidth: S.sp(220)
                        spacing: 0
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: S.sp(30)
                            color: theme.sunken
                            UiText { x: S.sp(12); anchors.verticalCenter: parent.verticalCenter; text: qsTr("Albums") + "  (" + foundAlbums.count + ")"; color: theme.dimForeground; px: 12 }
                        }
                        BrowseList {
                            id: foundAlbums
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            onClicked: root.clicked()
                            artSize: S.sp(72)
                            model: ytm.searchAlbums
                            onActivated: (row) => ytm.fetchAlbum(ytm.searchAlbums.get(row).key, "play")
                            onEnqueued: (row) => ytm.fetchAlbum(ytm.searchAlbums.get(row).key, "queue")
                            onMenuRequested: (row) => ytm.fetchAlbum(ytm.searchAlbums.get(row).key, "pick")
                            onContextRequested: (row, x, y) => { root.menuX = x; root.menuY = y; ytm.fetchAlbum(ytm.searchAlbums.get(row).key, "menu") }
                        }
                    }
                }
            }
        }
    }
}
}
