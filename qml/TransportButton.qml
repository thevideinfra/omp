import QtQuick
import "Scale.js" as S

Rectangle {
    id: root
    property string glyph
    property bool big: false
    signal clicked()

    implicitWidth: big ? S.sp(44) : S.sp(34)
    implicitHeight: implicitWidth
    radius: width / 2
    color: area.containsMouse ? theme.selection : "transparent"

    UiText {
        anchors.centerIn: parent
        text: root.glyph
        px: root.big ? 20 : 15
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
