import QtQuick
import "Scale.js" as S

Item {
    id: root
    property string label
    property bool checked: false
    signal toggled(bool value)

    implicitHeight: S.sp(34)

    Rectangle {
        anchors.fill: parent
        radius: S.sp(6)
        color: area.containsMouse ? Qt.alpha(theme.accent, 0.08) : Qt.alpha(theme.foreground, 0.03)
    }

    UiText {
        anchors.verticalCenter: parent.verticalCenter
        x: S.sp(10)
        text: root.label
    }

    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: S.sp(10)
        anchors.verticalCenter: parent.verticalCenter
        width: S.sp(40)
        height: S.sp(22)
        radius: height / 2
        color: root.checked ? Qt.alpha(theme.accent, 0.3) : Qt.alpha(theme.foreground, 0.1)
        border.color: root.checked ? theme.accent : Qt.alpha(theme.foreground, 0.25)
        border.width: 1

        Rectangle {
            width: parent.height - S.sp(6)
            height: width
            radius: width / 2
            y: S.sp(3)
            x: root.checked ? parent.width - width - S.sp(3) : S.sp(3)
            color: root.checked ? theme.accent : Qt.darker(theme.foreground, 1.4)
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.toggled(!root.checked)
    }
}
