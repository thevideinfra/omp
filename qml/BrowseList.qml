import QtQuick
import "Scale.js" as S

ListView {
    id: root
    property int playingRow: -1
    property int artSize: S.sp(40)   // thumbnail edge
    property bool songRows: false    // rows are songs: a heart marks the favorites
    property bool showSource: true   // a "local" / "server" tag, when the row's songs are known
    property bool sections: false    // group rows under their "initial" (a letter header)
    // Song lists as a table: Title, Artist, Album and Time in aligned columns under a header row.
    property bool table: false
    readonly property real timeW: S.sp(52)
    readonly property real badgeW: S.sp(120)
    readonly property real tableLeft: S.sp(8) + artSize + S.sp(10)
    readonly property real colSpace: Math.max(0, width - tableLeft - S.sp(12) - timeW - S.sp(10) - badgeW - S.sp(10))
    readonly property real titleW: colSpace * 0.44
    readonly property real artistW: colSpace * 0.28
    readonly property real albumW: colSpace * 0.28
    signal activated(int row)
    signal enqueued(int row)
    signal clicked()
    signal menuRequested(int row)   // right-click, or the m key

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
        Row {
            x: root.tableLeft
            height: parent.height
            Repeater {
                model: [[qsTr("Title"), root.titleW], [qsTr("Artist"), root.artistW], [qsTr("Album"), root.albumW]]
                UiText {
                    required property var modelData
                    width: modelData[1]
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData[0]
                    color: theme.dimForeground
                    px: 12
                }
            }
        }
        UiText {
            anchors.right: parent.right
            anchors.rightMargin: S.sp(12)
            anchors.verticalCenter: parent.verticalCenter
            width: root.timeW
            horizontalAlignment: Text.AlignRight
            text: qsTr("Time")
            color: theme.dimForeground
            px: 12
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
        readonly property string source: !root.showSource || cover === "" ? ""
                                         : cover.indexOf("subsonic:") === 0 ? qsTr("server") : qsTr("local")
        readonly property bool favorite: { library.starredRevision; return root.songRows && cover !== "" && library.isStarred(cover) }

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

        Row {
            id: tableRow
            visible: root.table
            x: root.tableLeft
            anchors.verticalCenter: parent.verticalCenter
            UiText {
                width: root.titleW
                text: title
                color: index === root.playingRow ? theme.accent : theme.foreground
                elide: Text.ElideRight
            }
            // A folder row has no artist or album: its path takes both columns.
            UiText {
                width: cover === "" ? root.artistW + root.albumW : root.artistW
                text: cover === "" ? subtitle : artistText
                color: theme.dimForeground
                elide: Text.ElideRight
            }
            UiText { visible: cover !== ""; width: root.albumW; text: albumText; color: theme.dimForeground; elide: Text.ElideRight }
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
                right: metaText.left
                rightMargin: S.sp(10)
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
            width: root.table ? root.timeW : implicitWidth
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
                if (mouse.button === Qt.RightButton) root.menuRequested(index)
                root.clicked()
            }
            onDoubleClicked: { root.activated(index); root.clicked() }
        }
    }
}
