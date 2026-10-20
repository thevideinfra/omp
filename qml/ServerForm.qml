import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Scale.js" as S

// A Subsonic server (Navidrome, Airsonic...): its songs join the local library.
// The password is kept in the system keyring, never in the config file.
ColumnLayout {
    id: root
    spacing: S.sp(10)

    property string testMessage: ""
    property bool confirmRemove: false

    UiText { text: qsTr("Subsonic server"); color: theme.dimForeground; px: 12 }

    ToggleRow {
        Layout.fillWidth: true
        label: qsTr("Use the server")
        checked: settingsStore.serverEnabled
        onToggled: (v) => settingsStore.set("serverEnabled", v)
    }

    component Field: TextField {
        Layout.fillWidth: true
        color: theme.foreground
        placeholderTextColor: theme.muted
        selectByMouse: true
        font.pixelSize: S.fs(13)
        font.family: ui.fontFamily !== "" ? ui.fontFamily : Qt.application.font.family
        background: Rectangle { radius: S.sp(6); color: theme.surface }
    }

    Field { id: urlField; placeholderText: qsTr("Server address, e.g. https://music.example.com"); text: settingsStore.serverUrl }
    Field { id: userField; placeholderText: qsTr("User name"); text: settingsStore.serverUser }
    Field {
        id: passField
        placeholderText: settingsStore.serverUrl !== "" ? qsTr("Password (stored in the keyring; type to change it)") : qsTr("Password")
        echoMode: TextInput.Password
    }

    RowLayout {
        spacing: S.sp(8)
        ChipButton {
            text: qsTr("Test connection")
            enabled: urlField.text !== "" && userField.text !== "" && passField.text !== ""
            onClicked: { root.testMessage = qsTr("Testing…"); subsonic.testConnection(urlField.text, userField.text, passField.text) }
        }
        ChipButton {
            text: qsTr("Save")
            enabled: urlField.text !== "" && userField.text !== "" && passField.text !== ""
            onClicked: { subsonic.saveServer(urlField.text, userField.text, passField.text, true); passField.text = ""; root.testMessage = "" }
        }
        ChipButton {
            visible: settingsStore.serverUrl !== ""
            danger: true
            text: root.confirmRemove ? qsTr("Really remove?") : qsTr("Remove")
            onClicked: {
                if (root.confirmRemove) { subsonic.remove(); root.confirmRemove = false; passField.text = "" }
                else { root.confirmRemove = true; confirmTimer.restart() }
            }
        }
    }

    Timer { id: confirmTimer; interval: 3000; onTriggered: root.confirmRemove = false }
    Connections {
        target: subsonic
        function onTestResult(ok, message) { root.testMessage = ok ? qsTr("Connected") : qsTr("Failed: %1").arg(message) }
    }

    UiText {
        Layout.fillWidth: true
        visible: root.testMessage !== "" || subsonic.status !== ""
        wrapMode: Text.WordWrap
        text: root.testMessage !== "" ? root.testMessage : subsonic.status
        color: theme.dimForeground
        px: 12
    }

    UiText {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: qsTr("The server's songs appear with your own in Albums, Artists, Songs and search. They are listed again after each sync (r), and kept when the server is offline.")
        color: theme.dimForeground
        px: 12
    }
}
