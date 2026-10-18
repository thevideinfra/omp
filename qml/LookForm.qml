import QtQuick
import QtQuick.Layouts
import "Scale.js" as S

// Density, font size, font and accent of one LookScope (`target`).
ColumnLayout {
    id: root
    property var target
    spacing: S.sp(12)

    UiText { text: qsTr("Density"); color: theme.dimForeground; px: 12 }
    Segmented {
        Layout.fillWidth: true
        options: ["compact", "normal", "comfortable"]
        selected: root.target.density
        onPicked: (v) => root.target.set("density", v)
    }

    UiText { text: qsTr("Font size"); color: theme.dimForeground; px: 12 }
    Segmented {
        Layout.fillWidth: true
        options: ["small", "normal", "large"]
        selected: root.target.fontSize
        onPicked: (v) => root.target.set("fontSize", v)
    }

    UiText { text: qsTr("Font"); color: theme.dimForeground; px: 12 }
    FontPicker {
        Layout.fillWidth: true
        selected: root.target.fontFamily
        onPicked: (f) => root.target.set("fontFamily", f)
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
                border.color: root.target.accent === modelData ? theme.foreground : theme.selection
                border.width: root.target.accent === modelData ? 2 : 1
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.target.set("accent", modelData)
                }
            }
        }
    }
    UiText {
        text: root.target.accent === "theme" ? qsTr("Accent: theme")
              : theme.accentChoices.indexOf(root.target.accent) >= 0 ? qsTr("Accent: %1").arg(root.target.accent)
              : qsTr("Accent: %1 (not in this theme)").arg(root.target.accent)
        color: theme.dimForeground
        px: 12
    }
}
