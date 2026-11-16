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

    spacing: S.sp(4)

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: S.sp(12)
        Layout.rightMargin: S.sp(8)
        UiText { text: root.heading; font.bold: true; px: 16; Layout.fillWidth: true }
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
            width: root.cardWidth
            height: row.height
            YtmCard {
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
