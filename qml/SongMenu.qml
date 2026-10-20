import QtQuick
import QtQuick.Controls
import "Scale.js" as S

// The menu that opens where you right-click a song, album, artist or playlist.
// Set `tracks` (song maps with a path), then popup(x, y). Esc or a click elsewhere closes it.
Menu {
    id: root
    property var tracks: []
    property bool canRemove: false   // the songs sit in a local playlist
    property string folderPath: ""   // set for a music folder at the top of Libraries: it can be removed
    signal addToPlaylist(var tracks)
    signal removeFromPlaylist()
    signal removeFolder(string path)
    signal goToArtist(string path)
    signal goToAlbum(string path)

    readonly property bool single: tracks.length === 1
    readonly property bool remoteSingle: single && (tracks[0].path ?? "").indexOf("subsonic:") === 0
    readonly property bool starred: { library.starredRevision; return remoteSingle && library.isStarred(tracks[0].path) }

    function play() { player.playTracks(tracks, 0) }
    function enqueue() { player.enqueueTracks(tracks) }
    function playNext() { player.playNext(tracks) }
    function toggleFavorite() { subsonic.toggleStar(tracks[0].path) }

    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: S.sp(4)

    background: Rectangle {
        implicitWidth: S.sp(210)
        radius: S.sp(8)
        color: theme.background
        border.width: 1
        border.color: Qt.alpha(theme.accent, 0.5)
    }

    // Opens with its top-left corner at (x, y) in the window.
    function openAt(x, y) { popup(x, y) }

    component Entry: MenuItem {
        id: entry
        property bool wanted: true   // shown in the menu at all
        visible: wanted
        implicitHeight: wanted ? S.sp(34) : 0
        height: implicitHeight
        contentItem: UiText {
            leftPadding: S.sp(10)
            verticalAlignment: Text.AlignVCenter
            text: entry.text
            color: entry.enabled ? theme.foreground : theme.dimForeground
        }
        background: Rectangle {
            radius: S.sp(6)
            color: entry.highlighted ? Qt.alpha(theme.accent, 0.15) : "transparent"
        }
    }

    Entry { objectName: "play"; text: qsTr("Play"); enabled: root.tracks.length > 0; onTriggered: root.play() }
    Entry { objectName: "next"; text: qsTr("Play next"); enabled: root.tracks.length > 0; onTriggered: root.playNext() }
    Entry { objectName: "queue"; text: qsTr("Add to queue"); enabled: root.tracks.length > 0; onTriggered: root.enqueue() }
    Entry { objectName: "playlist"; text: qsTr("Add to playlist…"); enabled: root.tracks.length > 0; onTriggered: root.addToPlaylist(root.tracks) }
    Entry { objectName: "artist"; wanted: root.single; text: qsTr("Go to artist"); onTriggered: root.goToArtist(root.tracks[0].path) }
    Entry { objectName: "album"; wanted: root.single; text: qsTr("Go to album"); onTriggered: root.goToAlbum(root.tracks[0].path) }
    Entry {
        objectName: "favorite"
        wanted: root.remoteSingle
        text: root.starred ? qsTr("Remove from favorites") : qsTr("Add to favorites")
        onTriggered: root.toggleFavorite()
    }
    Entry {
        objectName: "removeFolder"
        wanted: root.folderPath !== ""
        enabled: settingsStore.folders.length > 1          // the last folder stays
        text: qsTr("Remove from library")
        onTriggered: root.removeFolder(root.folderPath)
    }
    Entry { objectName: "remove"; wanted: root.canRemove; text: qsTr("Remove from playlist"); onTriggered: root.removeFromPlaylist() }
}
