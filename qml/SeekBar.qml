import QtQuick
import "Scale.js" as S

Item {
    id: root
    property real value: 0
    signal seeked(real fraction)

    implicitHeight: S.sp(16)

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        height: S.sp(4)
        radius: height / 2
        color: theme.selection

        Rectangle {
            width: parent.width * Math.max(0, Math.min(1, root.value))
            height: parent.height
            radius: height / 2
            color: theme.accent
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: (m) => root.seeked(Math.max(0, Math.min(1, m.x / width)))
        onPositionChanged: (m) => {
            if (pressed)
                root.seeked(Math.max(0, Math.min(1, m.x / width)))
        }
    }
}
