import QtQuick
import QtQuick.Layouts
import "Scale.js" as S

Flickable {
    id: root

    // The main window's key handler calls these on the current stack page.
    function move(delta) {}
    function activate() {}
    function enqueue() {}

    implicitHeight: column.implicitHeight + S.sp(24)
    contentWidth: width
    contentHeight: implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    ColumnLayout {
        id: column
        x: S.sp(16)
        y: S.sp(12)
        width: root.width - S.sp(32)
        spacing: S.sp(12)

        UiText { text: qsTr("Look"); font.bold: true; color: theme.accent }

        UiText { text: qsTr("Density"); color: theme.dimForeground; px: 12 }
        Segmented {
            Layout.fillWidth: true
            options: ["compact", "normal", "comfortable"]
            selected: ui.density
            onPicked: (v) => ui.set("density", v)
        }

        UiText { text: qsTr("Font size"); color: theme.dimForeground; px: 12 }
        Segmented {
            Layout.fillWidth: true
            options: ["small", "normal", "large"]
            selected: ui.fontSize
            onPicked: (v) => ui.set("fontSize", v)
        }

        UiText { text: qsTr("Font"); color: theme.dimForeground; px: 12 }
        FontPicker {
            Layout.fillWidth: true
            selected: ui.fontFamily
            onPicked: (f) => ui.set("fontFamily", f)
        }

        UiText { text: qsTr("Accent"); color: theme.dimForeground; px: 12 }
        Flow {
            Layout.fillWidth: true
            spacing: S.sp(8)
            Repeater {
                model: theme.accentChoices
                delegate: Rectangle {
                    required property string modelData
                    // Reading a theme property makes the colour follow theme switches.
                    readonly property color swatch: { theme.background; return theme.paletteColor(modelData) }
                    width: S.sp(28)
                    height: S.sp(28)
                    radius: width / 2
                    color: swatch
                    border.color: ui.accent === modelData ? theme.foreground : theme.selection
                    border.width: ui.accent === modelData ? 2 : 1
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: ui.set("accent", modelData)
                    }
                }
            }
        }
        UiText {
            text: ui.accent === "theme" ? qsTr("Accent: theme") : qsTr("Accent: %1").arg(ui.accent)
            color: theme.dimForeground
            px: 12
        }

        UiText { text: qsTr("Tray panel"); font.bold: true; color: theme.accent }

        ToggleRow { Layout.fillWidth: true; label: qsTr("Cover"); checked: ui.panelCover; onToggled: (v) => ui.set("panelCover", v) }
        ToggleRow { Layout.fillWidth: true; label: qsTr("Seek bar"); checked: ui.panelSeek; onToggled: (v) => ui.set("panelSeek", v) }
        ToggleRow { Layout.fillWidth: true; label: qsTr("Volume"); checked: ui.panelVolume; onToggled: (v) => ui.set("panelVolume", v) }
        ToggleRow { Layout.fillWidth: true; label: qsTr("Up next list"); checked: ui.panelUpNext; onToggled: (v) => ui.set("panelUpNext", v) }

        UiText { text: qsTr("Up next rows"); color: theme.dimForeground; px: 12 }
        Stepper {
            value: ui.panelUpNextCount
            from: 0
            to: 10
            onPicked: (v) => ui.set("panelUpNextCount", v)
        }
    }
}
