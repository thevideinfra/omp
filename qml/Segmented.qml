import QtQuick
import "Scale.js" as S

Row {
    id: root
    property var options: []
    property string selected: ""
    signal picked(string value)

    spacing: S.sp(4)

    Repeater {
        model: root.options
        delegate: Rectangle {
            required property string modelData
            readonly property bool on: modelData === root.selected
            width: label.implicitWidth + S.sp(20)
            height: S.sp(30)
            radius: S.sp(6)
            color: on ? theme.selection : theme.surface
            border.color: on ? theme.accent : "transparent"
            border.width: 1

            UiText {
                id: label
                anchors.centerIn: parent
                text: modelData
                color: parent.on ? theme.accent : theme.foreground
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.picked(modelData)
            }
        }
    }
}
