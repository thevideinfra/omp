import QtQuick
import QtQuick.Layouts
import "Scale.js" as S

// The equalizer presets and what each one does to the ten bands.
ColumnLayout {
    id: root
    spacing: S.sp(12)

    readonly property var freqs: ["31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"]
    readonly property var gains: player.eqGains(player.eqPreset)

    UiText { text: qsTr("Preset"); color: theme.dimForeground; px: 12 }
    Segmented {
        Layout.fillWidth: true
        options: player.eqPresets
        selected: player.eqPreset
        onPicked: (v) => player.eqPreset = v
    }

    UiText { text: qsTr("Bands (dB)"); color: theme.dimForeground; px: 12 }
    Item {
        id: chart
        Layout.fillWidth: true
        Layout.preferredHeight: S.sp(170)
        readonly property real half: (height - S.sp(34)) / 2   // bar room above/below the zero line
        readonly property real range: 6                        // dB at the edge

        Rectangle {   // zero line
            x: 0
            y: chart.half
            width: parent.width
            height: 1
            color: Qt.alpha(theme.foreground, 0.2)
        }

        Row {
            anchors.fill: parent
            Repeater {
                model: root.freqs
                Item {
                    required property int index
                    required property string modelData
                    width: chart.width / 10
                    height: chart.height
                    readonly property real gain: Number(root.gains[index] ?? 0)

                    Rectangle {
                        width: Math.min(parent.width * 0.5, S.sp(22))
                        anchors.horizontalCenter: parent.horizontalCenter
                        radius: S.sp(3)
                        color: Qt.alpha(theme.accent, 0.55)
                        border.width: 1
                        border.color: theme.accent
                        height: Math.max(2, Math.abs(parent.gain) / chart.range * chart.half)
                        y: parent.gain >= 0 ? chart.half - height : chart.half
                        visible: parent.gain !== 0
                    }
                    UiText {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: chart.half * 2 + S.sp(2)
                        text: modelData
                        color: theme.dimForeground
                        px: 11
                    }
                    UiText {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: chart.half * 2 + S.sp(16)
                        text: parent.gain > 0 ? "+" + parent.gain : String(parent.gain)
                        px: 11
                    }
                }
            }
        }
    }

    UiText {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: qsTr("Applies to everything omamusic plays. Boosts lower the overall level a little so loud tracks do not clip.")
        color: theme.dimForeground
        px: 12
    }
}
