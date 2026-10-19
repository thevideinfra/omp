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
    ToggleRow {
        Layout.fillWidth: true
        label: qsTr("Accent colours")
        checked: root.target.showAccent
        onToggled: (v) => root.target.set("showAccent", v)
    }
    Flow {
        Layout.fillWidth: true
        spacing: S.sp(6)
        enabled: root.target.showAccent
        opacity: root.target.showAccent ? 1 : 0.4
        Repeater {
            model: theme.accentChoices
            delegate: Rectangle {
                id: swatch
                required property string modelData
                readonly property bool chosen: root.target.accent === modelData
                readonly property bool isTheme: modelData === "theme"
                // Reading a theme property makes the colour follow theme switches.
                readonly property color paletteColor: { theme.background; return theme.paletteColor(modelData) }
                width: isTheme ? themeLabel.implicitWidth + S.sp(34) : S.sp(26)
                height: S.sp(26)
                radius: height / 2
                color: isTheme ? Qt.alpha(theme.foreground, 0.06) : paletteColor
                border.width: chosen ? 2 : 1
                border.color: chosen ? theme.foreground : Qt.alpha(theme.foreground, isTheme ? 0.25 : 0.15)

                Rectangle {
                    visible: swatch.isTheme
                    anchors.left: parent.left
                    anchors.leftMargin: S.sp(8)
                    anchors.verticalCenter: parent.verticalCenter
                    width: S.sp(12)
                    height: width
                    radius: width / 2
                    color: swatch.paletteColor
                }
                UiText {
                    id: themeLabel
                    visible: swatch.isTheme
                    anchors.right: parent.right
                    anchors.rightMargin: S.sp(9)
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Theme")
                    px: 12
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.target.set("accent", swatch.modelData)
                }
            }
        }
    }
}
