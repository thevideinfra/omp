import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Scale.js" as S

// An Albums/Artists tab: the list of albums or artists on the left, and the
// selected one's songs on the right (an artist shows its albums, each followed
// by its songs). `kind` is "album", "artist" or "playlist"; `model` is the list
// of rows (with `key`). Right/Left arrows move the keyboard between the two panes.
Item {
    id: root
    property string kind: "album"
    property var model
    property bool rightFocus: false
    property string openTitle: ""
    signal clicked()
    signal pickRequested(var tracks)   // add these songs to a playlist
    signal newPlaylistRequested()
    property bool renaming: false
    property bool confirmDelete: false

    // Detail rows: { header, indent, title, subtitle, meta, cover, tracks, start }
    // plays `tracks` from index `start`.
    property var rows: []
    property var allTracks: []

    function move(delta) {
        if (rightFocus) {
            detail.currentIndex = Math.max(0, Math.min(detail.count - 1, detail.currentIndex + delta))
            detail.positionViewAtIndex(detail.currentIndex, ListView.Contain)
        } else {
            top.move(delta)
        }
    }
    // Enter plays: the selected item from the list, or the row on the right.
    function activate() {
        if (rightFocus) playRow(detail.currentIndex)
        else if (top.count > 0) player.playTracks(tracksOf(top.currentIndex), 0)
    }
    function enqueue() {
        if (!rightFocus) {
            if (top.count > 0) player.enqueueTracks(tracksOf(top.currentIndex))
        } else if (rows.length > 0) {
            const r = rows[detail.currentIndex]
            player.enqueueTracks(r.header ? r.tracks : [r.tracks[r.start]])
        }
    }
    // The songs the current row stands for, for the playlist picker.
    function currentTracks() {
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
    // Delete removes the selected song from the open playlist.
    function removeSelected() {
        if (kind !== "playlist" || !rightFocus || rows.length === 0) return
        const r = rows[detail.currentIndex]
        if (r.header) return
        playlists.removeAt(model.get(top.currentIndex).key, r.plIndex)
    }
    function focusRight() {
        if (rows.length === 0) return false
        rightFocus = true
        return true
    }
    // Returns true when it moved the keyboard back to the list.
    function back() {
        if (!rightFocus) return false
        rightFocus = false
        return true
    }

    function tracksOf(row) {
        const key = model.get(row).key
        if (kind === "playlist") return library.tracksForPaths(playlists.paths(key))
        return kind === "album" ? library.albumTracks(key) : library.artistTracks(key)
    }

    function showRow(row) {
        if (row < 0 || row >= top.count) {
            rows = []; allTracks = []; openTitle = ""; detailModel.clear()
            return
        }
        openTitle = model.get(row).title
        const tracks = tracksOf(row)
        allTracks = tracks
        const out = []
        if (kind !== "artist") {
            tracks.forEach((t, i) => out.push({ header: false, indent: false, title: t.title,
                subtitle: t.artist, meta: t.meta, cover: "", tracks: tracks, start: i,
                plIndex: t.playlistIndex ?? -1 }))
        } else {
            let group = []
            const flush = () => {
                if (group.length === 0) return
                out.push({ header: true, indent: false, title: group[0].album, subtitle: group[0].artist,
                    meta: qsTr("%1 tracks").arg(group.length), cover: group[0].path, tracks: group, start: 0 })
                group.forEach((t, i) => out.push({ header: false, indent: true, title: t.title,
                    subtitle: t.artist, meta: t.meta, cover: "", tracks: group, start: i }))
                group = []
            }
            let album = null
            tracks.forEach(t => {
                const a = t.album.toLowerCase()
                if (a !== album) { flush(); album = a }
                group.push(t)
            })
            flush()
        }
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

    Connections {
        target: player
        function onCurrentChanged() { root.followPlaying() }
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
            root.renaming = false
            root.confirmDelete = false
            root.showRow(top.currentIndex)
            detail.currentIndex = Math.max(0, Math.min(keep, detail.count - 1))
        }
    }

    Timer {
        id: confirmTimer
        interval: 3000
        onTriggered: root.confirmDelete = false
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        ColumnLayout {
            Layout.preferredWidth: Math.max(S.sp(260), root.width * 0.4)
            Layout.fillHeight: true
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
                model: root.model
                onCurrentIndexChanged: root.showRow(currentIndex)
                onClicked: { root.rightFocus = false; root.clicked() }
                onActivated: (row) => player.playTracks(root.tracksOf(row), 0)
                onEnqueued: (row) => player.enqueueTracks(root.tracksOf(row))
                onMenuRequested: (row) => root.pickRequested(root.tracksOf(row))
            }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            color: Qt.alpha(root.rightFocus ? theme.accent : theme.foreground, root.rightFocus ? 0.45 : 0.12)
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: S.sp(8)
                spacing: S.sp(8)

                UiText {
                    Layout.fillWidth: true
                    visible: !root.renaming
                    text: root.openTitle
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
                    onAccepted: playlists.rename(root.openTitle, text)
                    Keys.onEscapePressed: root.renaming = false
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
                            if (root.renaming) playlists.rename(root.openTitle, renameField.text)
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

            // A larger cover for an album (artists show a cover on each album header).
            Image {
                id: bigCover
                readonly property string artSource: {
                    library.coverRevision
                    return root.kind === "album" && root.allTracks.length > 0 ? library.coverFor(root.allTracks[0].path) : ""
                }
                visible: artSource !== ""
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: visible ? S.sp(320) : 0
                Layout.preferredHeight: visible ? S.sp(320) : 0
                Layout.bottomMargin: visible ? S.sp(8) : 0
                source: artSource
                sourceSize.width: 640
                sourceSize.height: 640
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
