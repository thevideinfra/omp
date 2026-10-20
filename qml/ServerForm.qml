import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Scale.js" as S

// The Subsonic servers (Navidrome, Airsonic...): all of them join the local library. Each
// password is kept in the system keyring, never in a file.
ColumnLayout {
    id: root
    spacing: S.sp(10)

    property string editId: ""        // the server being edited; empty = adding a new one
    property string confirmId: ""     // the server whose Remove button asks "Really remove?"
    property string testMessage: ""

    UiText { text: qsTr("Subsonic servers"); color: theme.dimForeground; px: 12 }

    Repeater {
        model: subsonic.servers
        Rectangle {
            id: card
            required property string id
            required property string title
            required property string host
            required property string url
            required property string user
            required property bool active
            required property string state
            required property string status
            Layout.fillWidth: true
            Layout.preferredHeight: cardColumn.implicitHeight + S.sp(16)
            radius: S.sp(6)
            color: Qt.alpha(theme.foreground, 0.05)
            border.width: 1
            border.color: root.editId === card.id ? Qt.alpha(theme.accent, 0.6) : Qt.alpha(theme.foreground, 0.1)

            Column {
                id: cardColumn
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: S.sp(8) }
                spacing: S.sp(6)
                UiText { width: parent.width; text: card.title + "  ·  " + card.host + "  ·  " + card.user; elide: Text.ElideRight }
                UiText {
                    width: parent.width
                    text: card.state === "off" ? qsTr("Switched off") : card.status
                    color: card.state === "offline" ? theme.danger : theme.dimForeground
                    elide: Text.ElideRight
                    px: 12
                }
                Row {
                    spacing: S.sp(8)
                    ChipButton {
                        text: card.active ? qsTr("On") : qsTr("Off")
                        onClicked: subsonic.setServerEnabled(card.id, !card.active)
                    }
                    ChipButton {
                        text: qsTr("Edit")
                        onClicked: { root.editId = card.id; urlField.text = card.url; userField.text = card.user; passField.text = ""; root.testMessage = "" }
                    }
                    ChipButton {
                        danger: true
                        text: root.confirmId === card.id ? qsTr("Really remove?") : qsTr("Remove")
                        onClicked: {
                            if (root.confirmId === card.id) {
                                subsonic.removeServer(card.id)
                                root.confirmId = ""
                                if (root.editId === card.id) root.cancelEdit()
                            } else {
                                root.confirmId = card.id
                                confirmTimer.restart()
                            }
                        }
                    }
                }
            }
        }
    }

    function cancelEdit() { editId = ""; urlField.text = ""; userField.text = ""; passField.text = ""; testMessage = "" }

    UiText { text: root.editId === "" ? qsTr("Add a server") : qsTr("Edit the server"); color: theme.dimForeground; px: 12 }

    component Field: TextField {
        Layout.fillWidth: true
        color: theme.foreground
        placeholderTextColor: theme.muted
        selectByMouse: true
        font.pixelSize: S.fs(13)
        font.family: ui.fontFamily !== "" ? ui.fontFamily : Qt.application.font.family
        background: Rectangle { radius: S.sp(6); color: theme.surface }
    }

    Field { id: urlField; placeholderText: qsTr("Server address, e.g. https://music.example.com") }
    Field { id: userField; placeholderText: qsTr("User name") }
    Field {
        id: passField
        placeholderText: root.editId !== "" ? qsTr("Password (stored in the keyring; leave empty to keep it)") : qsTr("Password")
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
            text: root.editId === "" ? qsTr("Add") : qsTr("Save")
            enabled: urlField.text !== "" && userField.text !== "" && (root.editId !== "" || passField.text !== "")
            onClicked: {
                if (root.editId === "") {
                    if (subsonic.addServer(urlField.text, userField.text, passField.text, true) === "")
                        root.testMessage = qsTr("That address or user cannot be used (the address must start with http:// or https://)")
                    else
                        root.cancelEdit()
                } else {
                    const ok = subsonic.updateServer(root.editId, urlField.text, userField.text, passField.text, true)
                    if (ok) root.cancelEdit()
                    else root.testMessage = qsTr("That address or user cannot be used (the address must start with http:// or https://)")
                }
            }
        }
        ChipButton {
            visible: root.editId !== ""
            text: qsTr("Cancel")
            onClicked: root.cancelEdit()
        }
    }

    Timer { id: confirmTimer; interval: 3000; onTriggered: root.confirmId = "" }
    Connections {
        target: subsonic
        function onTestResult(ok, message) { root.testMessage = ok ? qsTr("Connected") : qsTr("Failed: %1").arg(message) }
    }

    UiText {
        Layout.fillWidth: true
        visible: root.testMessage !== ""
        wrapMode: Text.WordWrap
        text: root.testMessage
        color: theme.dimForeground
        px: 12
    }

    UiText {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: qsTr("The songs of all servers appear with your own in Albums, Artists, Songs and search. They are listed again after each sync (r), and kept when a server is offline.")
        color: theme.dimForeground
        px: 12
    }
}
