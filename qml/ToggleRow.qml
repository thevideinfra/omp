import QtQuick
import "Scale.js" as S

Item {
    id: root
    property string label
    property bool checked: false
    signal toggled(bool value)

    implicitHeight: S.sp(30)

    UiText {
        anchors.verticalCenter: parent.verticalCenter
        text: root.label
    }

    Rectangle {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: S.sp(40)
        height: S.sp(22)
        radius: height / 2
        color: root.checked ? theme.accent : theme.surface
        border.color: theme.selection
        border.width: 1

        Rectangle {
            width: parent.height - S.sp(6)
            height: width
            radius: width / 2
            y: S.sp(3)
            x: root.checked ? parent.width - width - S.sp(3) : S.sp(3)
            color: theme.foreground
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.toggled(!root.checked)
    }
}
