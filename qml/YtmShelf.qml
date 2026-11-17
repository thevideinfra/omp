import QtQuick
import QtQuick.Layouts
import "Scale.js" as S

// A heading and a row of cards that scrolls sideways. `model` has key, title, subtitle and cover.
ColumnLayout {
    id: root
    property string heading
    property var model
    property bool round: false
    property string icon: "albums"
    readonly property real cardWidth: S.sp(160)
    property bool showSeeAll: false   // a "See all" chip next to the arrows
    signal opened(int row)
    signal played(int row)
    signal contextRequested(int row, real x, real y)
    signal seeAll()
    // Sorting chips in the heading: [[key, label]...], the key the list is sorted by and which way.
    property var sortOptions: []
    property string sortKey: ""
    property bool sortDescending: false
    signal sortRequested(string key)

    spacing: S.sp(4)

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: S.sp(12)
        Layout.rightMargin: S.sp(8)
        UiText { text: root.heading; font.bold: true; px: 16; Layout.fillWidth: true }
        Repeater {
            model: root.sortOptions
            ChipButton {
                required property var modelData
                text: modelData[1] + (root.sortKey === modelData[0] ? (root.sortDescending ? "  ▼" : "  ▲") : "")
                onClicked: root.sortRequested(modelData[0])
            }
        }
        ChipButton { visible: root.showSeeAll; text: qsTr("See all"); onClicked: root.seeAll() }
        ChipButton { text: "‹"; enabled: !row.atXBeginning; onClicked: row.scrollBy(-row.width * 0.8) }
        ChipButton { text: "›"; enabled: !row.atXEnd; onClicked: row.scrollBy(row.width * 0.8) }
    }

    ListView {
        id: row
        Layout.fillWidth: true
        Layout.preferredHeight: root.cardWidth + S.sp(58)
        orientation: ListView.Horizontal
        clip: true
        spacing: 0
        leftMargin: S.sp(8)
        boundsBehavior: Flickable.StopAtBounds
        model: root.model

        function scrollBy(dx) {
            const target = Math.max(0, Math.min(contentWidth - width, contentX + dx))
            contentX = target   // a short slide: set at once, the list is cheap to move
        }

        delegate: Item {
            id: cell
            required property int index
            required property string title
            required property string subtitle
            required property string cover
            readonly property bool isDivider: cover === "--divider--"   // a line with a label between two kinds of card
            width: isDivider ? S.sp(64) : root.cardWidth
            height: row.height
            Rectangle {
                visible: cell.isDivider
                x: parent.width / 2 + S.sp(8)
                y: S.sp(12)
                width: 1
                height: root.cardWidth - S.sp(16)
                color: Qt.alpha(theme.foreground, 0.28)
            }
            UiText {
                visible: cell.isDivider
                text: cell.title
                color: theme.dimForeground
                px: 12
                rotation: -90
                x: parent.width / 2 - S.sp(10) - width / 2
                y: S.sp(12) + (root.cardWidth - S.sp(16)) / 2 - height / 2
            }
            YtmCard {
                visible: !cell.isDivider
                cardWidth: root.cardWidth
                title: cell.title
                subtitle: cell.subtitle
                cover: cell.cover
                icon: root.icon
                round: root.round
                onActivated: root.opened(cell.index)
                onPlayed: root.played(cell.index)
                onContext: (x, y) => root.contextRequested(cell.index, x, y)
            }
        }
    }
}
