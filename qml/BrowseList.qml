import QtQuick
import "Scale.js" as S
import "Columns.js" as Columns

ListView {
    id: root
    property int playingRow: -1
    property int artSize: S.sp(40)   // thumbnail edge
    property bool songRows: false    // rows are songs: a heart marks the favorites
    property bool showSource: true   // a "local" / "server" tag, when the row's songs are known
    property bool sections: false    // group rows under their "initial" (a letter header)
    // Song lists as a table: the chosen columns under a header row (Columns.js lists them).
    property bool table: false
    property var columns: ["title", "artist", "album", "year", "time"]
    property bool sortable: false     // a click on a header sorts by it
    property string sortKey: ""
    property bool sortDescending: false
    readonly property real badgeW: S.sp(120)
    readonly property real tableLeft: S.sp(8) + artSize + S.sp(10)
    // Each shown column with its x and width: fixed ones keep their width, the rest share what is left.
    readonly property var columnModel: {
        let fixedTotal = 0, weightTotal = 0
        columns.forEach(id => {
            const c = Columns.catalog[id]
            if (c.fixed) fixedTotal += S.sp(c.fixed)
            else weightTotal += c.weight
        })
        const space = Math.max(0, width - tableLeft - S.sp(12) - badgeW - S.sp(10))
        const flex = Math.max(0, space - fixedTotal)
        let x = tableLeft
        const out = []
        columns.forEach(id => {
            const c = Columns.catalog[id]
            const w = c.fixed ? S.sp(c.fixed) : flex * c.weight / weightTotal
            out.push({ id: id, label: c.label, sort: c.sort, x: x, w: w, right: !!c.right })
            x += w
        })
        return out
    }
    signal sortRequested(string key)
    signal columnsRequested(real x, real y)   // right-click on the header, with the window position
    signal activated(int row)
    signal enqueued(int row)
    signal clicked()
    signal menuRequested(int row)   // the m key
    signal contextRequested(int row, real x, real y)   // right-click, with the window position

    clip: true
    currentIndex: 0
    boundsBehavior: Flickable.StopAtBounds
    keyNavigationEnabled: false

    function move(delta) {
        currentIndex = Math.max(0, Math.min(count - 1, currentIndex + delta))
        positionViewAtIndex(currentIndex, ListView.Contain)
    }
    function activate() { if (count > 0) activated(currentIndex) }
    function enqueue() { if (count > 0) enqueued(currentIndex) }
    function pick() { if (count > 0) menuRequested(currentIndex) }

    FastWheel { view: root }

    headerPositioning: ListView.OverlayHeader
    header: Rectangle {
        z: 2
        width: root.width
        height: root.table ? S.sp(30) : 0
        visible: root.table
        color: theme.sunken
        MouseArea {   // right-click anywhere on the header: choose the columns
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            onClicked: (mouse) => { const p = mapToItem(null, mouse.x, mouse.y); root.columnsRequested(p.x, p.y) }
        }
        Repeater {
            model: root.table ? root.columnModel : []
            Item {
                id: cell
                required property var modelData
                required property int index
                readonly property bool clickable: root.sortable && modelData.sort !== ""
                readonly property bool sorted: clickable && root.sortKey === modelData.sort
                x: modelData.x
                width: modelData.w
                height: parent.height
                Rectangle {
                    anchors.fill: parent
                    color: headArea.containsMouse && cell.clickable ? Qt.alpha(theme.accent, 0.1) : "transparent"
                }
                UiText {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - S.sp(12)
                    horizontalAlignment: modelData.right ? Text.AlignRight : Text.AlignLeft
                    elide: Text.ElideRight
                    text: modelData.label + (cell.sorted ? (root.sortDescending ? "  ▼" : "  ▲") : "")
                    color: cell.sorted ? theme.accent : theme.dimForeground
                    px: 12
                }
                Rectangle {   // the divider before this column
                    visible: index > 0
                    x: -S.sp(6)
                    y: S.sp(6)
                    width: 1
                    height: parent.height - S.sp(12)
                    color: Qt.alpha(theme.foreground, 0.2)
                }
                MouseArea {
                    id: headArea
                    anchors.fill: parent
                    hoverEnabled: cell.clickable
                    cursorShape: cell.clickable ? Qt.PointingHandCursor : Qt.ArrowCursor
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton) {
                            const p = mapToItem(null, mouse.x, mouse.y)
                            root.columnsRequested(p.x, p.y)
                        } else if (cell.clickable) {
                            root.sortRequested(modelData.sort)
                        }
                    }
                }
            }
        }
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Qt.alpha(theme.accent, 0.5)
        }
    }

    section.property: sections ? "initial" : ""
    section.criteria: ViewSection.FullString
    // A letter heading: a darker band, a large accent letter and a line above it.
    section.delegate: Rectangle {
        required property string section
        width: ListView.view.width
        height: section === "" ? 0 : S.sp(38)
        visible: section !== ""
        color: theme.sunken
        Rectangle {
            width: parent.width
            height: 2
            color: Qt.alpha(theme.accent, 0.6)
        }
        UiText {
            x: S.sp(14)
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: S.sp(1)
            text: parent.section
            color: theme.accent
            px: 18
        }
    }

    delegate: Rectangle {
        required property int index
        required property string title
        required property string subtitle
        required property string meta
        required property string cover   // a track path; the art file is looked up lazily
        readonly property string artSource: { library.coverRevision; return cover !== "" ? library.coverFor(cover) : "" }
        readonly property string source: !root.showSource || !settingsStore.showSourceTags || cover === "" ? ""
                                         : cover.indexOf("subsonic:") === 0 ? qsTr("server") : qsTr("local")
        readonly property bool favorite: { library.starredRevision; return root.songRows && cover !== "" && library.isStarred(cover) }

        readonly property var info: root.table && cover !== "" ? library.trackInfo(cover) : ({})
        readonly property string playlistsText: { playlists.revision; return root.table && cover !== "" ? playlists.namesFor(cover).join(", ") : "" }
        readonly property int dash: subtitle.indexOf(" — ")
        readonly property string artistText: dash >= 0 ? subtitle.substring(0, dash) : subtitle
        readonly property string albumText: dash >= 0 ? subtitle.substring(dash + 3) : ""

        width: ListView.view.width
        height: root.table ? Math.max(S.sp(52), root.artSize + S.sp(12))
                           : Math.max(S.sp(60), root.artSize + S.sp(18), textColumn.implicitHeight + S.sp(14))
        color: ListView.isCurrentItem ? Qt.alpha(theme.accent, 0.12)
               : index % 2 === 1 ? Qt.alpha(theme.foreground, 0.045) : "transparent"   // alternating rows

        Image {
            id: art
            x: S.sp(8)
            anchors.verticalCenter: parent.verticalCenter
            width: artSource !== "" ? root.artSize : 0
            height: root.artSize
            source: artSource
            sourceSize.width: root.artSize * 2
            sourceSize.height: root.artSize * 2
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
        }

        function cellText(id) {
            switch (id) {
            case "title": return title
            case "artist": return artistText
            case "album": return albumText
            case "albumartist": return info.albumArtist ?? ""
            case "year": return (info.year ?? 0) > 0 ? String(info.year) : ""
            case "track": return (info.track ?? 0) > 0 ? ((info.disc ?? 0) > 1 ? info.disc + "." + info.track : String(info.track)) : ""
            case "added": return (info.mtime ?? 0) > 0 ? Qt.formatDate(new Date(info.mtime * 1000), "yyyy-MM-dd") : ""
            case "size": return (info.size ?? 0) > 0 ? (info.size / 1048576).toFixed(1) + " MB" : ""
            case "format": return info.format ?? ""
            case "playlists": return playlistsText
            case "time": return meta
            }
            return ""
        }

        // The table cells. A folder row has only a name and its path (spread over the other columns).
        Repeater {
            model: root.table ? root.columnModel : []
            UiText {
                required property var modelData
                required property int index
                readonly property bool folder: cover === ""
                visible: !folder || modelData.id === "title" || index === 1
                x: modelData.x
                width: folder && index === 1 ? root.width - modelData.x - S.sp(12) - root.badgeW
                                             : modelData.w - S.sp(12)
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: modelData.right ? Text.AlignRight : Text.AlignLeft
                text: folder ? (index === 1 ? subtitle : (modelData.id === "title" ? title : "")) : cellText(modelData.id)
                color: modelData.id === "title" && !folder ? (index === root.playingRow ? theme.accent : theme.foreground)
                       : folder && modelData.id === "title" ? theme.foreground : theme.dimForeground
                elide: Text.ElideRight
            }
        }
        Repeater {   // the same dividers down every row
            model: root.table ? root.columnModel : []
            Rectangle {
                required property var modelData
                required property int index
                visible: index > 0 && cover !== ""
                x: modelData.x - S.sp(6)
                width: 1
                height: parent.height
                color: Qt.alpha(theme.foreground, 0.1)
            }
        }

        Column {
            id: textColumn
            visible: !root.table
            anchors {
                left: art.right
                leftMargin: S.sp(10)
                right: badges.left
                rightMargin: S.sp(10)
                verticalCenter: parent.verticalCenter
            }
            UiText {
                width: parent.width
                text: title
                color: index === root.playingRow ? theme.accent : theme.foreground
                elide: Text.ElideRight
            }
            UiText {
                width: parent.width
                text: subtitle
                color: theme.dimForeground
                px: 12
                elide: Text.ElideRight
            }
        }

        Row {
            id: badges
            anchors {
                right: root.table ? parent.right : metaText.left
                rightMargin: root.table ? S.sp(12) : S.sp(10)
                verticalCenter: parent.verticalCenter
            }
            spacing: S.sp(8)
            UiText {
                visible: favorite
                anchors.verticalCenter: parent.verticalCenter
                text: "♥"
                color: theme.accent
                px: 13
            }
            Rectangle {
                visible: source !== ""
                anchors.verticalCenter: parent.verticalCenter
                width: tag.implicitWidth + S.sp(12)
                height: tag.implicitHeight + S.sp(4)
                radius: height / 2
                color: "transparent"
                border.width: 1
                border.color: source === qsTr("server") ? Qt.alpha(theme.accent, 0.5) : Qt.alpha(theme.foreground, 0.2)
                UiText {
                    id: tag
                    anchors.centerIn: parent
                    text: source
                    px: 11
                    color: source === qsTr("server") ? theme.accent : theme.dimForeground
                }
            }
        }

        UiText {
            id: metaText
            anchors {
                right: parent.right
                rightMargin: S.sp(12)
                verticalCenter: parent.verticalCenter
            }
            visible: !root.table
            horizontalAlignment: Text.AlignRight
            text: meta
            color: theme.dimForeground
            px: 12
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: (mouse) => {
                root.currentIndex = index
                if (mouse.button === Qt.RightButton) {
                    const p = mapToItem(null, mouse.x, mouse.y)
                    root.contextRequested(index, p.x, p.y)
                }
                root.clicked()
            }
            onDoubleClicked: { root.activated(index); root.clicked() }
        }
    }
}
