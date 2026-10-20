import QtQuick
import QtQuick.Controls
import "Scale.js" as S

// The sleep timer choices; opened from the moon button in the player bar.
Menu {
    id: root
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: S.sp(4)
    readonly property bool active: player.sleepRemaining >= 0 || player.sleepAfterSong

    // Opens with its top-left corner at (x, y) in the window.
    function openAt(x, y) { popup(x, y) }

    background: Rectangle {
        implicitWidth: S.sp(200)
        radius: S.sp(8)
        color: theme.background
        border.width: 1
        border.color: Qt.alpha(theme.accent, 0.5)
    }

    Repeater {
        model: [15, 30, 45, 60, 90]
        MenuEntry {
            required property int modelData
            objectName: "m" + modelData
            text: qsTr("Pause in %1 minutes").arg(modelData)
            onTriggered: player.startSleepTimer(modelData)
        }
    }
    MenuEntry { objectName: "afterSong"; text: qsTr("After this song"); onTriggered: player.sleepAfterCurrentSong() }
    MenuEntry { objectName: "cancelSleep"; wanted: root.active; text: qsTr("Cancel the timer"); onTriggered: player.cancelSleep() }
}
