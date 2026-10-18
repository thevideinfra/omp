import QtQuick
import "Scale.js" as S

// "" means the system default font.
Item {
    id: root
    property string selected: ""
    property bool open: false
    signal picked(string family)

    implicitWidth: S.sp(240)
    implicitHeight: header.height + (open ? list.height + S.sp(4) : 0)

    Rectangle {
        id: header
        width: parent.width
        height: S.sp(32)
        radius: S.sp(6)
        color: theme.surface

        UiText {
            x: S.sp(10)
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - S.sp(36)
            elide: Text.ElideRight
            text: root.selected === "" ? qsTr("System default") : root.selected
        }
        UiText {
            anchors.right: parent.right
            anchors.rightMargin: S.sp(10)
            anchors.verticalCenter: parent.verticalCenter
            color: theme.dimForeground
            text: root.open ? "▴" : "▾"
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.open = !root.open
        }
    }

    ListView {
        id: list
        visible: root.open
        y: header.height + S.sp(4)
        width: parent.width
        height: Math.min(S.sp(200), contentHeight)
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: [""].concat(ui.fontFamilies())

        delegate: Rectangle {
            required property string modelData
            width: ListView.view.width
            height: S.sp(28)
            color: modelData === root.selected ? theme.selection
                   : (area.containsMouse ? theme.surface : "transparent")

            UiText {
                x: S.sp(10)
                anchors.verticalCenter: parent.verticalCenter
                text: modelData === "" ? qsTr("System default") : modelData
                font.family: modelData === "" ? Qt.application.font.family : modelData
            }
            MouseArea {
                id: area
                anchors.fill: parent
                hoverEnabled: true
                onClicked: { root.picked(modelData); root.open = false }
            }
        }
    }
}
