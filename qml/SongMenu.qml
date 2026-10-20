import QtQuick
import QtQuick.Controls
import "Scale.js" as S

// The menu that opens where you right-click a song, album, artist or playlist.
// Set `tracks` (song maps with a path), then popup(x, y). Esc or a click elsewhere closes it.
Menu {
    id: root
    property var tracks: []
    property bool canRemove: false   // the songs sit in a local playlist
    property int queueRow: -1        // set for a row of the Queue tab: it can be removed or moved
    property int queueCount: 0
    property bool canGoArtist: true  // false when the Artists tab is hidden
    property bool canGoAlbum: true
    property string folderPath: ""   // set for a music folder at the top of Libraries: it can be removed
    signal newPlaylist(var tracks)                         // "New playlist…" in the submenu
    signal addToPlaylistNamed(string name, var tracks)   // an existing playlist in the submenu
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
    function removeFromQueue() { player.removeAt(queueRow) }
    function moveInQueue(delta) { player.moveRow(queueRow, queueRow + delta) }
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
        arrow: UiText {   // the little arrow of an entry that opens a submenu
            x: entry.width - width - S.sp(10)
            anchors.verticalCenter: parent.verticalCenter
            visible: entry.subMenu !== null
            text: "▸"
            color: theme.dimForeground
        }
        background: Rectangle {
            radius: S.sp(6)
            color: entry.highlighted ? Qt.alpha(theme.accent, 0.15) : "transparent"
        }
    }

    Entry { objectName: "play"; text: qsTr("Play"); enabled: root.tracks.length > 0; onTriggered: root.play() }
    Entry { objectName: "next"; text: qsTr("Play next"); enabled: root.tracks.length > 0; onTriggered: root.playNext() }
    Entry { objectName: "queue"; text: qsTr("Add to queue"); enabled: root.tracks.length > 0; onTriggered: root.enqueue() }
    Entry { objectName: "removeQueue"; wanted: root.queueRow >= 0; text: qsTr("Remove from queue"); onTriggered: root.removeFromQueue() }
    Entry { objectName: "moveUp"; wanted: root.queueRow >= 0; enabled: root.queueRow > 0; text: qsTr("Move up"); onTriggered: root.moveInQueue(-1) }
    Entry { objectName: "moveDown"; wanted: root.queueRow >= 0; enabled: root.queueRow < root.queueCount - 1; text: qsTr("Move down"); onTriggered: root.moveInQueue(1) }
    // Add to playlist: a submenu with a new playlist and the existing local ones.
    delegate: Component { Entry {} }   // how submenus are drawn in this menu
    Menu {
        id: playlistMenu
        objectName: "playlistMenu"
        title: qsTr("Add to playlist")
        enabled: root.tracks.length > 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: S.sp(4)
        delegate: Component { Entry {} }
        background: Rectangle {
            implicitWidth: S.sp(210)
            radius: S.sp(8)
            color: theme.background
            border.width: 1
            border.color: Qt.alpha(theme.accent, 0.5)
        }
        Entry { objectName: "newPlaylist"; text: qsTr("New playlist…"); onTriggered: root.newPlaylist(root.tracks) }
        MenuSeparator {
            contentItem: Rectangle { implicitHeight: 1; color: Qt.alpha(theme.foreground, 0.15) }
        }
        Repeater {
            model: { playlists.revision; return playlists.names() }
            Entry {
                required property string modelData
                objectName: "pl:" + modelData
                text: modelData
                onTriggered: root.addToPlaylistNamed(modelData, root.tracks)
            }
        }
    }
    Entry { objectName: "artist"; wanted: root.single && root.canGoArtist; text: qsTr("Go to artist"); onTriggered: root.goToArtist(root.tracks[0].path) }
    Entry { objectName: "album"; wanted: root.single && root.canGoAlbum; text: qsTr("Go to album"); onTriggered: root.goToAlbum(root.tracks[0].path) }
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
