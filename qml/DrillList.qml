import QtQuick
import QtQuick.Layouts
import "Scale.js" as S

// An Albums/Artists tab: the list of albums or artists on the left, and the
// selected one's songs on the right (an artist shows its albums, each followed
// by its songs). `kind` is "album" or "artist"; `model` is the list of rows
// (with `key`). Right/Left arrows move the keyboard between the two panes.
Item {
    id: root
    property string kind: "album"
    property var model
    property bool rightFocus: false
    property string openTitle: ""
    signal clicked()

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
        if (kind === "album") {
            tracks.forEach((t, i) => out.push({ header: false, indent: false, title: t.title,
                subtitle: t.artist, meta: t.meta, cover: "", tracks: tracks, start: i }))
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
    }

    function playRow(row) {
        if (row < 0 || row >= rows.length) return
        const r = rows[row]
        player.playTracks(r.tracks, r.start)
    }

    Connections {
        target: library
        function onIndexChanged() { root.showRow(top.currentIndex) }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        BrowseList {
            id: top
            Layout.preferredWidth: Math.max(S.sp(260), root.width * 0.4)
            Layout.fillHeight: true
            model: root.model
            onCurrentIndexChanged: root.showRow(currentIndex)
            onClicked: { root.rightFocus = false; root.clicked() }
            onActivated: (row) => player.playTracks(root.tracksOf(row), 0)
            onEnqueued: (row) => player.enqueueTracks(root.tracksOf(row))
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
                    text: root.openTitle
                    font.bold: true
                    elide: Text.ElideRight
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
                Layout.preferredWidth: visible ? S.sp(240) : 0
                Layout.preferredHeight: visible ? S.sp(240) : 0
                Layout.bottomMargin: visible ? S.sp(8) : 0
                source: artSource
                sourceSize.width: 480
                sourceSize.height: 480
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

                delegate: Rectangle {
                    id: item
                    required property int index
                    required property string title
                    required property string subtitle
                    required property string meta
                    required property string cover
                    required property bool headerRow
                    required property bool indent
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
                        onClicked: { detail.currentIndex = item.index; root.rightFocus = true; root.clicked() }
                        onDoubleClicked: { root.rightFocus = true; root.playRow(item.index); root.clicked() }
                    }
                }
            }
        }
    }
}
