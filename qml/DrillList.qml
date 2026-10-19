import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Scale.js" as S

// An Albums/Artists/Playlists tab: the list on the left, and the selected
// one's songs on the right. An artist gets a middle column of its albums with
// large thumbnails; picking one shows that album's songs, with its cover, on the right. `kind` is "album", "artist" or "playlist"; `model` is
// the list of rows (with `key`). Right/Left arrows move the keyboard between the panes.
Item {
    id: root
    property string kind: "album"
    property var model
    property bool rightFocus: false
    property bool midFocus: false      // artists: the albums column has the keyboard
    property int albumIndex: 0         // artists: the open album
    property var albumGroups: []       // artists: [{ title, artist, tracks }]
    property var artistTracks: []
    readonly property alias detailIndex: detail.currentIndex
    property string openTitle: ""
    signal clicked()
    signal pickRequested(var tracks)   // add these songs to a playlist
    signal newPlaylistRequested()
    property bool renaming: false
    property bool confirmDelete: false
    property string selectName: ""   // playlist to select after the next change

    // Detail rows: { header, indent, title, subtitle, meta, cover, tracks, start }
    // plays `tracks` from index `start`.
    property var rows: []
    property var allTracks: []

    function move(delta) {
        if (midFocus) {
            mid.move(delta)
        } else if (rightFocus) {
            detail.currentIndex = Math.max(0, Math.min(detail.count - 1, detail.currentIndex + delta))
            detail.positionViewAtIndex(detail.currentIndex, ListView.Contain)
        } else {
            top.move(delta)
        }
    }
    // Enter plays: the selected item from the list, or the row on the right.
    function activate() {
        if (midFocus) player.playTracks(albumTracksAt(mid.currentIndex), 0)
        else if (rightFocus) playRow(detail.currentIndex)
        else if (top.count > 0) player.playTracks(tracksOf(top.currentIndex), 0)
    }
    function enqueue() {
        if (midFocus) {
            player.enqueueTracks(albumTracksAt(mid.currentIndex))
        } else if (!rightFocus) {
            if (top.count > 0) player.enqueueTracks(tracksOf(top.currentIndex))
        } else if (rows.length > 0) {
            const r = rows[detail.currentIndex]
            player.enqueueTracks(r.header ? r.tracks : [r.tracks[r.start]])
        }
    }
    // The songs the current row stands for, for the playlist picker.
    function currentTracks() {
        if (midFocus) return albumTracksAt(mid.currentIndex)
        if (rightFocus && rows.length > 0) {
            const r = rows[detail.currentIndex]
            return r.header ? r.tracks : [r.tracks[r.start]]
        }
        return top.count > 0 ? tracksOf(top.currentIndex) : []
    }
    function pick() {
        const t = currentTracks()
        if (t.length > 0) pickRequested(t)
    }
    function doRename(text) {
        const name = text.trim()
        selectName = name   // set first: the change signal arrives during rename()
        if (!playlists.rename(openTitle, name)) {
            selectName = ""
            renaming = false
        }
        clicked()   // give the keyboard back to the window
    }
    // Delete removes the selected song from the open playlist.
    function removeSelected() {
        if (kind !== "playlist" || !rightFocus || rows.length === 0) return
        const r = rows[detail.currentIndex]
        if (r.header) return
        playlists.removeAt(model.get(top.currentIndex).key, r.plIndex)
    }
    function focusRight() {
        if (rows.length === 0) return false
        if (kind === "artist" && !midFocus && !rightFocus) {
            midFocus = true
            return true
        }
        midFocus = false
        rightFocus = true
        return true
    }
    // Returns true when it moved the keyboard back a pane.
    function back() {
        if (rightFocus) {
            rightFocus = false
            midFocus = kind === "artist"
            return true
        }
        if (midFocus) {
            midFocus = false
            return true
        }
        return false
    }

    // The songs of album n in the albums column.
    function albumTracksAt(n) {
        return n >= 0 && n < albumGroups.length ? albumGroups[n].tracks : artistTracks
    }
    // Picking an album shows its songs (and its cover) on the right.
    function selectAlbum(n) {
        if (n < 0 || n === albumIndex) return
        albumIndex = n
        if (mid.currentIndex !== n) mid.currentIndex = n
        rebuildDetail()
    }

    function tracksOf(row) {
        const key = model.get(row).key
        if (kind === "playlist") return library.tracksForPaths(playlists.paths(key))
        return kind === "album" ? library.albumTracks(key) : library.artistTracks(key)
    }

    function showRow(row) {
        renaming = false
        confirmDelete = false
        if (row < 0 || row >= top.count) {
            rows = []; allTracks = []; openTitle = ""; artistTracks = []; albumGroups = []
            albumModel.clear(); detailModel.clear()
            return
        }
        openTitle = model.get(row).title
        artistTracks = tracksOf(row)
        albumIndex = 0
        if (kind === "artist") {
            const groups = []
            let album = null
            artistTracks.forEach(t => {
                const a = t.album.toLowerCase()
                if (a !== album) { groups.push({ title: t.album, artist: t.artist, tracks: [] }); album = a }
                groups[groups.length - 1].tracks.push(t)
            })
            albumGroups = groups
            albumModel.clear()
            groups.forEach(g => albumModel.append({ key: g.title, title: g.title, subtitle: g.artist,
                meta: qsTr("%1 tracks").arg(g.tracks.length), cover: g.tracks[0].path }))
            mid.currentIndex = 0
        }
        rebuildDetail()
    }

    // The right pane: the open item's songs; for an artist, the open album's.
    function rebuildDetail() {
        const one = kind === "artist" && albumIndex < albumGroups.length
        const tracks = one ? albumGroups[albumIndex].tracks : artistTracks
        allTracks = tracks
        const out = []
        tracks.forEach((t, i) => out.push({ header: false, indent: false, title: t.title,
            subtitle: t.artist, meta: t.meta, cover: "", tracks: tracks, start: i,
            plIndex: t.playlistIndex ?? -1 }))
        rows = out
        detailModel.clear()
        out.forEach(r => detailModel.append({ title: r.title, subtitle: r.subtitle, meta: r.meta,
            cover: r.cover, headerRow: r.header, indent: r.indent }))
        detail.currentIndex = 0
        detail.positionViewAtBeginning()
        followPlaying()
    }

    // Path of the song a detail row stands for ("" for album headers).
    function rowPath(i) {
        const r = rows[i]
        return r && !r.header ? r.tracks[r.start].path : ""
    }

    // The highlighted song follows what is playing, when it is in this view.
    function followPlaying() {
        const path = player.current.path ?? ""
        if (path === "") return
        for (let i = 0; i < rows.length; ++i) {
            if (rowPath(i) === path) {
                detail.currentIndex = i
                detail.positionViewAtIndex(i, ListView.Contain)
                return
            }
        }
    }

    // Follow when the playing song changes, not on every queue change (which
    // would pull the selection back while you browse the pane).
    property string lastPlayingPath: ""
    Connections {
        target: player
        function onCurrentChanged() {
            const path = player.current.path ?? ""
            if (path === root.lastPlayingPath) return
            root.lastPlayingPath = path
            root.followPlaying()
        }
    }

    function playRow(row) {
        if (row < 0 || row >= rows.length) return
        const r = rows[row]
        // A song queues its album from that song, so Next/Previous walk the album;
        // an album header plays the whole album.
        player.playTracks(r.tracks, r.header ? 0 : r.start)
    }

    Connections {
        target: library
        function onIndexChanged() { root.showRow(top.currentIndex) }
    }

    Connections {
        target: playlists
        enabled: root.kind === "playlist"
        function onChanged() {
            const keep = detail.currentIndex
            // The list is sorted by name: stay on the same playlist (or the renamed one).
            const name = root.selectName !== "" ? root.selectName : root.openTitle
            root.selectName = ""
            let row = -1
            for (let i = 0; i < top.count; ++i) {
                if (root.model.get(i).key === name) { row = i; break }
            }
            if (row >= 0 && row !== top.currentIndex) top.currentIndex = row   // shows that row
            else root.showRow(top.currentIndex)
            detail.currentIndex = Math.max(0, Math.min(keep, detail.count - 1))
        }
    }

    Timer {
        id: confirmTimer
        interval: 3000
        onTriggered: root.confirmDelete = false
    }

    // The columns can be dragged wider or narrower by their dividers.
    SplitView {
        id: panes
        anchors.fill: parent
        orientation: Qt.Horizontal

        // The columns stay equal (half each, a third each for an artist) as the window
        // settles into its size, until the user drags a divider. Assigned, not bound: any
        // setting change (shuffle, repeat) re-evaluates bindings and would undo a dragged width.
        property bool userSized: false
        function equalize() {
            if (userSized || width <= 0) return
            const share = width / (root.kind === "artist" ? 3 : 2)
            listPane.SplitView.preferredWidth = share
            if (root.kind === "artist") mid.SplitView.preferredWidth = share
        }
        onWidthChanged: equalize()
        onResizingChanged: if (resizing) userSized = true

        handle: Rectangle {
            implicitWidth: S.sp(7)
            color: "transparent"
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 1
                height: parent.height
                color: SplitHandle.pressed || SplitHandle.hovered ? Qt.alpha(theme.accent, 0.6) : Qt.alpha(theme.foreground, 0.12)
            }
        }

        ColumnLayout {
            id: listPane
            objectName: "listPane"
            SplitView.minimumWidth: S.sp(140)
            spacing: 0

            Rectangle {
                visible: root.kind === "playlist"
                Layout.margins: visible ? S.sp(8) : 0
                Layout.preferredWidth: newText.implicitWidth + S.sp(20)
                Layout.preferredHeight: visible ? S.sp(30) : 0
                radius: S.sp(6)
                color: newArea.containsMouse ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
                border.width: 1
                border.color: newArea.containsMouse ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12)
                UiText { id: newText; anchors.centerIn: parent; text: qsTr("New playlist") }
                MouseArea {
                    id: newArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.newPlaylistRequested()
                }
            }

            BrowseList {
                id: top
                Layout.fillWidth: true
                Layout.fillHeight: true
                sections: root.kind === "artist"
                model: root.model
                onCurrentIndexChanged: root.showRow(currentIndex)
                onClicked: { root.rightFocus = false; root.midFocus = false; root.clicked() }
                onActivated: (row) => player.playTracks(root.tracksOf(row), 0)
                onEnqueued: (row) => player.enqueueTracks(root.tracksOf(row))
                onMenuRequested: (row) => root.pickRequested(root.tracksOf(row))
            }
        }

        // The artist's albums, with thumbnails.
        BrowseList {
            id: mid
            visible: root.kind === "artist"
            artSize: S.sp(96)
            SplitView.minimumWidth: S.sp(140)
            model: ListModel { id: albumModel }
            onCurrentIndexChanged: root.selectAlbum(currentIndex)
            onClicked: { root.rightFocus = false; root.midFocus = true; root.clicked() }
            onActivated: (row) => player.playTracks(root.albumTracksAt(row), 0)
            onEnqueued: (row) => player.enqueueTracks(root.albumTracksAt(row))
            onMenuRequested: (row) => root.pickRequested(root.albumTracksAt(row))
        }

        ColumnLayout {
            id: rightPane
            SplitView.fillWidth: true
            SplitView.minimumWidth: S.sp(200)
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: S.sp(8)
                spacing: S.sp(8)

                UiText {
                    Layout.fillWidth: true
                    visible: !root.renaming
                    text: root.kind === "artist" && root.albumIndex < root.albumGroups.length
                          ? root.openTitle + " — " + root.albumGroups[root.albumIndex].title : root.openTitle
                    font.bold: true
                    elide: Text.ElideRight
                }

                TextField {
                    id: renameField
                    Layout.fillWidth: true
                    visible: root.renaming
                    color: theme.foreground
                    selectByMouse: true
                    font.pixelSize: S.fs(13)
                    background: Rectangle { radius: S.sp(6); color: theme.surface }
                    onVisibleChanged: if (visible) { text = root.openTitle; forceActiveFocus(); selectAll() }
                    onAccepted: root.doRename(text)
                    Keys.onEscapePressed: { root.renaming = false; root.clicked() }
                }

                Rectangle {
                    visible: root.kind === "playlist" && top.count > 0
                    Layout.preferredWidth: renameText.implicitWidth + S.sp(20)
                    Layout.preferredHeight: S.sp(30)
                    radius: S.sp(6)
                    color: renameArea.containsMouse ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
                    border.width: 1
                    border.color: renameArea.containsMouse ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12)
                    UiText { id: renameText; anchors.centerIn: parent; text: root.renaming ? qsTr("Save") : qsTr("Rename") }
                    MouseArea {
                        id: renameArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.renaming) root.doRename(renameField.text)
                            else root.renaming = true
                        }
                    }
                }

                Rectangle {
                    visible: root.kind === "playlist" && top.count > 0
                    Layout.preferredWidth: deleteText.implicitWidth + S.sp(20)
                    Layout.preferredHeight: S.sp(30)
                    radius: S.sp(6)
                    color: deleteArea.containsMouse || root.confirmDelete ? Qt.alpha(theme.danger, 0.15) : Qt.alpha(theme.foreground, 0.05)
                    border.width: 1
                    border.color: deleteArea.containsMouse || root.confirmDelete ? Qt.alpha(theme.danger, 0.55) : Qt.alpha(theme.foreground, 0.12)
                    UiText {
                        id: deleteText
                        anchors.centerIn: parent
                        text: root.confirmDelete ? qsTr("Really delete?") : qsTr("Delete")
                        color: root.confirmDelete ? theme.danger : theme.foreground
                    }
                    MouseArea {
                        id: deleteArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.confirmDelete) playlists.remove(root.openTitle)
                            else { root.confirmDelete = true; confirmTimer.restart() }
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: playText.implicitWidth + S.sp(20)
                    Layout.preferredHeight: S.sp(30)
                    radius: S.sp(6)
                    color: playArea.containsMouse ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
                    border.width: 1
                    border.color: playArea.containsMouse ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12)
                    UiText { id: playText; anchors.centerIn: parent; text: qsTr("Play all") }
                    MouseArea {
                        id: playArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: { player.playTracks(root.allTracks, 0); root.clicked() }
                    }
                }
            }

            // A larger cover for the open album.
            Image {
                id: bigCover
                readonly property string artSource: {
                    library.coverRevision
                    return root.kind !== "playlist" && root.allTracks.length > 0 ? library.coverFor(root.allTracks[0].path) : ""
                }
                // As wide as the pane (so dragging the divider resizes it), but never more
                // than half its height, so the songs stay in view.
                readonly property real side: Math.max(S.sp(120), Math.min(rightPane.width - S.sp(16), rightPane.height * 0.5))
                visible: artSource !== ""
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: visible ? side : 0
                Layout.preferredHeight: visible ? side : 0
                Layout.bottomMargin: visible ? S.sp(8) : 0
                source: artSource
                sourceSize.width: 1200
                sourceSize.height: 1200
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }

            ListView {
                id: detail
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                keyNavigationEnabled: false
                model: ListModel { id: detailModel }
                FastWheel { view: detail }

                delegate: Rectangle {
                    id: item
                    required property int index
                    required property string title
                    required property string subtitle
                    required property string meta
                    required property string cover
                    required property bool headerRow
                    required property bool indent
                readonly property bool nowPlaying: !headerRow && (player.current.path ?? "") !== ""
                                                   && root.rowPath(index) === player.current.path
                    readonly property string artSource: { library.coverRevision; return cover !== "" ? library.coverFor(cover) : "" }

                    width: ListView.view.width
                    height: headerRow ? Math.max(S.sp(52), headCol.implicitHeight + S.sp(8)) : S.sp(34)
                    color: ListView.isCurrentItem ? Qt.alpha(theme.accent, 0.12)
                           : headerRow ? Qt.alpha(theme.foreground, 0.05) : "transparent"

                    Image {
                        id: art
                        x: S.sp(8)
                        anchors.verticalCenter: parent.verticalCenter
                        width: item.headerRow && item.artSource !== "" ? S.sp(40) : 0
                        height: S.sp(40)
                        source: item.headerRow ? item.artSource : ""
                        sourceSize.width: 80
                        sourceSize.height: 80
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                    }

                    Column {
                        id: headCol
                        anchors {
                            left: art.right
                            leftMargin: item.indent ? S.sp(30) : S.sp(10)
                            right: metaText.left
                            rightMargin: S.sp(10)
                            verticalCenter: parent.verticalCenter
                        }
                        UiText {
                            width: parent.width
                            text: item.title
                            color: item.nowPlaying ? theme.accent : theme.foreground
                            font.bold: item.headerRow
                            elide: Text.ElideRight
                        }
                        UiText {
                            visible: item.headerRow
                            width: parent.width
                            text: item.subtitle
                            color: theme.dimForeground
                            px: 12
                            elide: Text.ElideRight
                        }
                    }

                    UiText {
                        id: metaText
                        anchors.right: parent.right
                        anchors.rightMargin: S.sp(12)
                        anchors.verticalCenter: parent.verticalCenter
                        text: item.meta
                        color: theme.dimForeground
                        px: 12
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: (mouse) => {
                            detail.currentIndex = item.index
                            root.rightFocus = true
                            if (mouse.button === Qt.RightButton) root.pick()
                            root.clicked()
                        }
                        onDoubleClicked: { root.rightFocus = true; root.playRow(item.index); root.clicked() }
                    }
                }
            }
        }
    }
}
