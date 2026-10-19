import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Scale.js" as S

// The equalizer, opened from the button beside the transport buttons.
Popup {
    id: root
    modal: true
    focus: true
    padding: S.sp(16)
    width: Math.min(S.sp(500), (parent ? parent.width : S.sp(500)) - S.sp(40))
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        radius: S.sp(8)
        color: theme.background
        border.width: 1
        border.color: Qt.alpha(theme.accent, 0.45)
    }

    contentItem: ColumnLayout {
        spacing: S.sp(12)
        UiText { text: qsTr("Equalizer"); px: 16 }
        EqualizerForm { Layout.fillWidth: true }
    }
}
