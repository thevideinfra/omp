import QtQuick

Rectangle {
    id: root
    property string glyph
    property bool big: false
    signal clicked()

    implicitWidth: big ? 44 : 34
    implicitHeight: implicitWidth
    radius: width / 2
    color: area.containsMouse ? theme.selection : "transparent"

    Text {
        anchors.centerIn: parent
        text: root.glyph
        color: theme.foreground
        font.pixelSize: root.big ? 20 : 15
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
