import QtQuick
import QtQuick.Layouts
import "Scale.js" as S

// An Albums/Artists tab: the list of albums or artists, and opening one shows
// its songs (an artist shows its albums, each followed by its songs).
// `kind` is "album" or "artist"; `model` is the list of rows (with `key`).
Item {
    id: root
    property string kind: "album"
    property var model
    property bool open: false
    property string openTitle: ""
    signal clicked()

    // Detail rows: { header, indent, title, subtitle, meta, cover, tracks, start }
    // plays `tracks` from index `start`.
    property var rows: []
    property var allTracks: []

    function move(delta) {
        if (open) {
            detail.currentIndex = Math.max(0, Math.min(detail.count - 1, detail.currentIndex + delta))
            detail.positionViewAtIndex(detail.currentIndex, ListView.Contain)
        } else {
            top.move(delta)
        }
    }
    function activate() { open ? playRow(detail.currentIndex) : openRow(top.currentIndex) }
    function enqueue() {
        if (!open) {
            if (top.count > 0) player.enqueueTracks(tracksOf(top.currentIndex))
        } else if (rows.length > 0) {
            const r = rows[detail.currentIndex]
            player.enqueueTracks(r.header ? r.tracks : [r.tracks[r.start]])
        }
    }
    // Returns true when it closed the detail view.
    function back() {
        if (!open) return false
        open = false
        return true
    }

    function tracksOf(row) {
        const key = model.get(row).key
        return kind === "album" ? library.albumTracks(key) : library.artistTracks(key)
    }

    function openRow(row) {
        if (row < 0 || row >= top.count) return
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
        open = true
    }

    function playRow(row) {
        if (row < 0 || row >= rows.length) return
        const r = rows[row]
        player.playTracks(r.tracks, r.start)
    }

    BrowseList {
        id: top
        anchors.fill: parent
        visible: !root.open
        model: root.model
        onClicked: root.clicked()
        onActivated: (row) => root.openRow(row)
        onEnqueued: (row) => player.enqueueTracks(root.tracksOf(row))
    }

    ColumnLayout {
        anchors.fill: parent
        visible: root.open
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: S.sp(8)
            spacing: S.sp(8)

            Rectangle {
                Layout.preferredWidth: backText.implicitWidth + S.sp(20)
                Layout.preferredHeight: S.sp(30)
                radius: S.sp(6)
                color: backArea.containsMouse ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
                border.width: 1
                border.color: backArea.containsMouse ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12)
                UiText { id: backText; anchors.centerIn: parent; text: "◀ " + qsTr("Back") }
                MouseArea {
                    id: backArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { root.back(); root.clicked() }
                }
            }

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
                    onClicked: { detail.currentIndex = item.index; root.clicked() }
                    onDoubleClicked: { root.playRow(item.index); root.clicked() }
                }
            }
        }
    }
}
