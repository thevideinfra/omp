import QtQuick
import QtQuick.Layouts
import "Scale.js" as S

// The Home page: rows of cards from your account: what you played lately, your liked songs, playlists,
// albums and artists. A row with nothing in it is left out.
Flickable {
    id: root
    signal songsPlayed(var tracks, int start)
    signal contextRequested(var tracks, real x, real y, bool canRemove)
    signal albumOpened(string key, string title, string subtitle, string cover)
    signal artistOpened(string key, string title, string cover)
    signal playlistOpened(int row)
    signal seeAll(string view)

    // The keyboard: Up and Down scroll; there is nothing to select here.
    function move(delta) { contentY = Math.max(0, Math.min(Math.max(0, contentHeight - height), contentY + delta * S.sp(120))) }
    function step(delta) {}
    function activate() {}
    function enqueue() {}
    function pick() {}
    function focusRight() {}
    function back() { return false }
    function removeSelected() {}

    clip: true
    contentWidth: width
    contentHeight: column.implicitHeight + S.sp(24)
    boundsBehavior: Flickable.StopAtBounds
    FastWheel { view: root; notch: S.sp(90) }

    // The songs the rows of songs stand for (read again when the hearts change).
    ListModel { id: liked }
    function refill() {
        liked.clear()
        library.separateLiked("ytm", 20).forEach(t => liked.append({ title: t.title, subtitle: t.artist, cover: t.path }))
    }
    Connections {
        target: library
        function onStarredChanged() { root.refill() }
        function onIndexChanged() { root.refill() }
    }
    Component.onCompleted: refill()

    readonly property bool empty: ytm.history.count === 0 && liked.count === 0 && playlists.listOf("ytm").count === 0
                                  && library.separateAlbums.count === 0 && library.separateArtists.count === 0

    ColumnLayout {
        id: column
        x: 0
        y: S.sp(8)
        width: root.width
        spacing: S.sp(14)

        UiText {
            visible: root.empty
            Layout.margins: S.sp(16)
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: theme.dimForeground
            text: ytm.loggedIn ? qsTr("Nothing here yet. Press Refresh on the YouTube Music card in Settings > Library, or search for something.")
                               : qsTr("Log in under Settings > Library > YouTube Music to see your music here.")
        }

        YtmShelf {   // what you played lately
            visible: ytm.history.count > 0
            Layout.fillWidth: true
            heading: qsTr("Listen again")
            icon: "songs"
            model: ytm.history
            onOpened: (row) => root.songsPlayed(ytm.history.all(), row)
            onPlayed: (row) => root.songsPlayed(ytm.history.all(), row)
            onContextRequested: (row, x, y) => root.contextRequested([ytm.history.get(row)], x, y, false)
        }

        YtmShelf {   // the songs you liked
            visible: liked.count > 0
            Layout.fillWidth: true
            heading: qsTr("Liked songs")
            icon: "songs"
            model: liked
            onOpened: (row) => root.songsPlayed(library.separateLiked("ytm", 20), row)
            onPlayed: (row) => root.songsPlayed(library.separateLiked("ytm", 20), row)
            onContextRequested: (row, x, y) => root.contextRequested([library.separateLiked("ytm", 20)[row]], x, y, false)
        }

        YtmShelf {
            visible: playlists.listOf("ytm").count > 0
            Layout.fillWidth: true
            heading: qsTr("Your playlists")
            icon: "playlists"
            model: playlists.listOf("ytm")
            onOpened: (row) => root.playlistOpened(row)
            onPlayed: (row) => root.songsPlayed(library.tracksForPaths(playlists.paths(playlists.listOf("ytm").get(row).key)), 0)
            onContextRequested: (row, x, y) => root.contextRequested(library.tracksForPaths(playlists.paths(playlists.listOf("ytm").get(row).key)), x, y, false)
            showSeeAll: true
            onSeeAll: root.seeAll("Playlists")
        }

        YtmShelf {
            visible: library.separateAlbums.count > 0
            Layout.fillWidth: true
            heading: qsTr("Your albums")
            icon: "albums"
            model: library.separateAlbums
            onOpened: (row) => { const a = library.separateAlbums.get(row); root.albumOpened(a.key, a.title, a.subtitle, a.cover) }
            onPlayed: (row) => root.songsPlayed(library.separateAlbumTracks(library.separateAlbums.get(row).key), 0)
            onContextRequested: (row, x, y) => root.contextRequested(library.separateAlbumTracks(library.separateAlbums.get(row).key), x, y, false)
        }

        YtmShelf {
            visible: library.separateArtists.count > 0
            Layout.fillWidth: true
            heading: qsTr("Your artists")
            icon: "artists"
            round: true
            model: library.separateArtists
            onOpened: (row) => { const a = library.separateArtists.get(row); root.artistOpened(a.key, a.title, a.cover) }
            onPlayed: (row) => root.songsPlayed(library.separateArtistTracks(library.separateArtists.get(row).key), 0)
            onContextRequested: (row, x, y) => root.contextRequested(library.separateArtistTracks(library.separateArtists.get(row).key), x, y, false)
            showSeeAll: true
            onSeeAll: root.seeAll("Artists")
        }
    }
}
