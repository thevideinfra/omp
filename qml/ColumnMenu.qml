import QtQuick
import QtQuick.Controls
import "Scale.js" as S
import "Columns.js" as Columns

// Opened by a right-click on a table header: tick the columns to show.
Menu {
    id: root
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: S.sp(4)

    // Opens with its top-left corner at (x, y) in the window.
    function openAt(x, y) { popup(x, y) }

    background: Rectangle {
        implicitWidth: S.sp(210)
        radius: S.sp(8)
        color: theme.background
        border.width: 1
        border.color: Qt.alpha(theme.accent, 0.5)
    }

    Repeater {
        model: Columns.order
        MenuItem {
            id: entry
            required property string modelData
            checkable: true
            checked: settingsStore.songColumns.indexOf(modelData) >= 0
            enabled: modelData !== "title"          // there is always a title
            implicitHeight: S.sp(32)
            onTriggered: settingsStore.setSongColumn(modelData, checked)
            contentItem: UiText {
                leftPadding: S.sp(26)
                verticalAlignment: Text.AlignVCenter
                text: Columns.catalog[entry.modelData].label
                color: entry.enabled ? theme.foreground : theme.dimForeground
            }
            indicator: UiText {
                x: S.sp(8)
                anchors.verticalCenter: parent.verticalCenter
                visible: entry.checked
                text: "✓"
                color: theme.accent
            }
            background: Rectangle {
                radius: S.sp(6)
                color: entry.highlighted ? Qt.alpha(theme.accent, 0.15) : "transparent"
            }
        }
    }
}
