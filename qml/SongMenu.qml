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
    signal newServerPlaylist(string serverId, var tracks)                        // "New playlist on <server>…"
    signal addToServerPlaylist(string serverId, string playlistId, var tracks)   // a playlist that lives on a server
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

    MenuEntry { objectName: "play"; text: qsTr("Play"); enabled: root.tracks.length > 0; onTriggered: root.play() }
    MenuEntry { objectName: "next"; text: qsTr("Play next"); enabled: root.tracks.length > 0; onTriggered: root.playNext() }
    MenuEntry { objectName: "queue"; text: qsTr("Add to queue"); enabled: root.tracks.length > 0; onTriggered: root.enqueue() }
    MenuEntry { objectName: "removeQueue"; wanted: root.queueRow >= 0; text: qsTr("Remove from queue"); onTriggered: root.removeFromQueue() }
    MenuEntry { objectName: "moveUp"; wanted: root.queueRow >= 0; enabled: root.queueRow > 0; text: qsTr("Move up"); onTriggered: root.moveInQueue(-1) }
    MenuEntry { objectName: "moveDown"; wanted: root.queueRow >= 0; enabled: root.queueRow < root.queueCount - 1; text: qsTr("Move down"); onTriggered: root.moveInQueue(1) }
    // Add to playlist: a submenu with a new playlist and the existing local ones.
    delegate: Component { MenuEntry {} }   // how submenus are drawn in this menu
    Menu {
        id: playlistMenu
        objectName: "playlistMenu"
        title: qsTr("Add to playlist")
        enabled: root.tracks.length > 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: S.sp(4)
        delegate: Component { MenuEntry {} }
        background: Rectangle {
            implicitWidth: S.sp(210)
            radius: S.sp(8)
            color: theme.background
            border.width: 1
            border.color: Qt.alpha(theme.accent, 0.5)
        }
        // New playlist (here and on each server that is on), a divider, then the local playlists and the servers' own.
        readonly property var entries: {
            playlists.revision
            const out = [{ kind: "new", text: qsTr("New playlist…"), name: "newPlaylist" }]
            for (let i = 0; i < subsonic.servers.count; ++i) {
                const s = subsonic.servers.get(i)
                if (s.active)
                    out.push({ kind: "newOn", text: qsTr("New playlist on %1…").arg(s.title), serverId: s.id, name: "newOn:" + s.id })
            }
            out.push({ kind: "divider", text: "", name: "divider" })
            playlists.names().forEach(n => out.push({ kind: "local", text: n, name: "pl:" + n }))
            playlists.editableServerLists().forEach(l => out.push({ kind: "server", text: l.name + "  ·  " + l.source,
                serverId: l.serverId, playlistId: l.playlistId, name: "sp:" + l.serverId + "/" + l.playlistId }))
            return out
        }
        Repeater {
            model: playlistMenu.entries
            MenuEntry {
                required property var modelData
                objectName: modelData.name
                text: modelData.text
                divider: modelData.kind === "divider"
                onTriggered: {
                    if (modelData.kind === "new") root.newPlaylist(root.tracks)
                    else if (modelData.kind === "newOn") root.newServerPlaylist(modelData.serverId, root.tracks)
                    else if (modelData.kind === "local") root.addToPlaylistNamed(modelData.text, root.tracks)
                    else if (modelData.kind === "server") root.addToServerPlaylist(modelData.serverId, modelData.playlistId, root.tracks)
                }
            }
        }
    }
    MenuEntry { objectName: "artist"; wanted: root.single && root.canGoArtist; text: qsTr("Go to artist"); onTriggered: root.goToArtist(root.tracks[0].path) }
    MenuEntry { objectName: "album"; wanted: root.single && root.canGoAlbum; text: qsTr("Go to album"); onTriggered: root.goToAlbum(root.tracks[0].path) }
    MenuEntry {
        objectName: "favorite"
        wanted: root.remoteSingle
        text: root.starred ? qsTr("Remove from favorites") : qsTr("Add to favorites")
        onTriggered: root.toggleFavorite()
    }
    MenuEntry {
        objectName: "removeFolder"
        wanted: root.folderPath !== ""
        enabled: settingsStore.folders.length > 1          // the last folder stays
        text: qsTr("Remove from library")
        onTriggered: root.removeFolder(root.folderPath)
    }
    MenuEntry { objectName: "remove"; wanted: root.canRemove; text: qsTr("Remove from playlist"); onTriggered: root.removeFromPlaylist() }
}
