import QtQuick
import "Scale.js" as S

// Sort chips plus a reverse toggle. `options` is a list of [key, label].
Row {
    id: root
    property var options: []
    property string sortKey: ""
    property bool descending: false
    property real chipHeight: S.sp(30)
    signal keyPicked(string key)
    signal reversed()

    spacing: S.sp(4)

    Repeater {
        model: root.options
        delegate: Rectangle {
            required property var modelData
            readonly property bool on: root.sortKey === modelData[0]
            width: label.implicitWidth + S.sp(20)
            height: root.chipHeight
            radius: S.sp(6)
            color: on || area.containsMouse ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
            border.width: 1
            border.color: on ? theme.accent : (area.containsMouse ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12))
            UiText { id: label; anchors.centerIn: parent; text: modelData[1]; color: parent.on ? theme.accent : theme.foreground }
            MouseArea {
                id: area
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.keyPicked(modelData[0])
            }
        }
    }

    Rectangle {
        width: root.chipHeight
        height: root.chipHeight
        radius: S.sp(6)
        color: root.descending || flip.containsMouse ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
        border.width: 1
        border.color: root.descending ? theme.accent : (flip.containsMouse ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12))
        UiText { anchors.centerIn: parent; text: "⇅"; color: root.descending ? theme.accent : theme.foreground }
        MouseArea {
            id: flip
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.reversed()
        }
    }
}
