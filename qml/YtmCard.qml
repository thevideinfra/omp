import QtQuick
import "Scale.js" as S

// A card: a square cover with the title and a line under it. A round Play button shows on hover.
// A click opens it, the Play button plays it, a right-click asks for its menu.
Item {
    id: root
    property string title
    property string subtitle
    property string cover
    property string icon: "albums"
    property bool round: false        // artists have a round picture
    property bool current: false      // the keyboard is on it
    property real cardWidth: S.sp(170)
    signal activated()
    signal played()
    signal context(real x, real y)

    readonly property real pad: S.sp(8)
    width: cardWidth
    height: art.height + textColumn.implicitHeight + pad * 3

    Rectangle {   // hover and keyboard highlight
        anchors.fill: parent
        radius: S.sp(10)
        color: root.current ? Qt.alpha(theme.accent, 0.14) : area.containsMouse ? Qt.alpha(theme.foreground, 0.08) : "transparent"
        border.width: root.current ? 1 : 0
        border.color: Qt.alpha(theme.accent, 0.5)
    }

    YtmCover {
        id: art
        x: root.pad
        y: root.pad
        width: root.width - root.pad * 2
        height: width
        cover: root.cover
        icon: root.icon
        round: root.round
    }

    Rectangle {   // the Play button
        id: playButton
        visible: area.containsMouse || playArea.containsMouse
        anchors { right: art.right; bottom: art.bottom; rightMargin: S.sp(8); bottomMargin: S.sp(8) }
        width: S.sp(40)
        height: width
        radius: width / 2
        color: playArea.containsMouse ? theme.accent : Qt.alpha(theme.accent, 0.9)
        UiText {
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: S.sp(1)
            text: "▶"
            color: theme.background
            px: 15
        }
        MouseArea {
            id: playArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.played()
        }
    }

    Column {
        id: textColumn
        x: root.pad + S.sp(2)
        y: art.y + art.height + root.pad
        width: root.width - root.pad * 2 - S.sp(4)
        spacing: S.sp(2)
        UiText { width: parent.width; text: root.title; font.bold: true; elide: Text.ElideRight }
        UiText { width: parent.width; text: root.subtitle; color: theme.dimForeground; px: 12; elide: Text.ElideRight; visible: text !== "" }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        z: -1   // under the Play button
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.PointingHandCursor
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) { const p = mapToItem(null, mouse.x, mouse.y); root.context(p.x, p.y) }
            else root.activated()
        }
    }
}
