import QtQuick
import "Scale.js" as S

// A header strip of sort choices for a list (Albums, Artists): the same look as a table
// header. `options` is a list of [key, label]; a click asks for that sort.
Rectangle {
    id: root
    property var options: []
    property string sortKey: ""
    property bool descending: false
    signal sortRequested(string key)

    implicitHeight: S.sp(30)
    color: theme.sunken

    Row {
        anchors.fill: parent
        Repeater {
            model: root.options
            Item {
                id: cell
                required property var modelData
                required property int index
                readonly property bool sorted: root.sortKey === modelData[0]
                width: root.width / Math.max(1, root.options.length)
                height: root.height
                Rectangle {   // the hover shade stays between the dividers
                    anchors.fill: parent
                    anchors.leftMargin: index > 0 ? 1 : 0
                    color: area.containsMouse ? Qt.alpha(theme.accent, 0.1) : "transparent"
                }
                UiText {
                    anchors.centerIn: parent
                    width: parent.width - S.sp(12)
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    text: modelData[1] + (cell.sorted ? (root.descending ? "  ▼" : "  ▲") : "")
                    color: cell.sorted ? theme.accent : theme.dimForeground
                    px: 12
                }
                Rectangle {   // the divider before this column
                    visible: index > 0
                    y: S.sp(6)
                    width: 1
                    height: parent.height - S.sp(12)
                    color: Qt.alpha(theme.foreground, 0.2)
                }
                MouseArea {
                    id: area
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.sortRequested(modelData[0])
                }
            }
        }
    }
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Qt.alpha(theme.accent, 0.5)
    }
}
