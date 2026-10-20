import QtQuick
import QtQuick.Controls
import "Scale.js" as S

// One row of a menu: themed, with a switch (`wanted`) to leave it out and an arrow for submenus.
MenuItem {
    id: entry
    property bool wanted: true   // shown in the menu at all
    visible: wanted
    implicitHeight: wanted ? S.sp(34) : 0
    height: implicitHeight
    contentItem: UiText {
        leftPadding: S.sp(10)
        verticalAlignment: Text.AlignVCenter
        text: entry.text
        color: entry.enabled ? theme.foreground : theme.dimForeground
    }
    arrow: UiText {   // the little arrow of an entry that opens a submenu
        x: entry.width - width - S.sp(10)
        anchors.verticalCenter: parent.verticalCenter
        visible: entry.subMenu !== null
        text: "▸"
        color: theme.dimForeground
    }
    background: Rectangle {
        radius: S.sp(6)
        color: entry.highlighted ? Qt.alpha(theme.accent, 0.15) : "transparent"
    }
}
