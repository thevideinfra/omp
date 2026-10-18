import QtQuick
import "Scale.js" as S

Row {
    id: root
    property int value: 0
    property int from: 0
    property int to: 10
    signal picked(int value)

    spacing: S.sp(8)

    Rectangle {
        width: S.sp(30); height: S.sp(30); radius: S.sp(6)
        color: theme.surface
        UiText { anchors.centerIn: parent; text: "−" }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.picked(Math.max(root.from, root.value - 1))
        }
    }
    UiText {
        width: S.sp(30); height: S.sp(30)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        text: root.value
    }
    Rectangle {
        width: S.sp(30); height: S.sp(30); radius: S.sp(6)
        color: theme.surface
        UiText { anchors.centerIn: parent; text: "+" }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.picked(Math.min(root.to, root.value + 1))
        }
    }
}
