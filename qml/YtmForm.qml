import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Scale.js" as S

// YouTube Music: log in with the request headers of a signed-in browser, then its songs and playlists
// join the library. The login is kept in the system keyring.
ColumnLayout {
    id: root
    spacing: S.sp(10)

    readonly property bool signedIn: ytm.state === "connected" || ytm.state === "syncing" || ytm.state === "offline" || ytm.state === "expired"

    UiText { text: qsTr("YouTube Music"); color: theme.dimForeground; px: 12 }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: statusColumn.implicitHeight + S.sp(16)
        radius: S.sp(6)
        color: Qt.alpha(theme.foreground, 0.05)
        border.width: 1
        border.color: Qt.alpha(theme.foreground, 0.1)
        Column {
            id: statusColumn
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: S.sp(8) }
            spacing: S.sp(6)
            UiText {
                width: parent.width
                wrapMode: Text.WordWrap
                text: ytm.state === "off" ? qsTr("Not logged in") : ytm.status
                color: ytm.state === "connected" ? theme.foreground : ytm.state === "syncing" ? theme.dimForeground : theme.danger
            }
            Row {
                spacing: S.sp(8)
                ChipButton { visible: root.signedIn; text: qsTr("Refresh"); enabled: ytm.state !== "syncing"; onClicked: ytm.sync() }
                ChipButton { visible: root.signedIn; text: qsTr("Log out"); danger: true; onClicked: ytm.logout() }
            }
        }
    }

    UiText {
        visible: !root.signedIn || ytm.state === "expired"
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: theme.dimForeground
        px: 12
        text: qsTr("To log in: open music.youtube.com in your browser while signed in, press F12, open the Network tab and type \"browse\" in the filter. "
                   + "Click around in the page so a request shows up, right-click it, choose Copy > Copy as cURL, and paste it here.")
    }

    TextArea {
        id: paste
        visible: !root.signedIn || ytm.state === "expired"
        Layout.fillWidth: true
        Layout.preferredHeight: S.sp(80)
        wrapMode: TextArea.WrapAnywhere
        color: theme.foreground
        placeholderText: qsTr("Paste the copied cURL command here")
        placeholderTextColor: theme.muted
        selectByMouse: true
        font.pixelSize: S.fs(12)
        font.family: ui.fontFamily !== "" ? ui.fontFamily : Qt.application.font.family
        background: Rectangle { radius: S.sp(6); color: theme.surface }
    }

    ChipButton {
        visible: paste.visible
        text: qsTr("Log in")
        enabled: paste.text.trim() !== "" && ytm.state !== "syncing"
        onClicked: { ytm.login(paste.text); paste.text = "" }
    }

    ToggleRow {
        Layout.fillWidth: true
        label: qsTr("YouTube Music in its own tab")
        checked: settingsStore.ytmSeparateTab
        onToggled: (v) => settingsStore.set("ytmSeparateTab", v)
    }

    UiText {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: theme.dimForeground
        px: 12
        text: qsTr("Off: its songs are mixed into Songs, Albums and Artists with a YT tag. On: they have a tab of their own. ")
              + qsTr("Songs play through yt-dlp, which is not allowed by YouTube's terms of service and can stop working when YouTube changes. "
                   + "Playlists are read-only, and there is no ReplayGain or scrobbling for these songs.")
    }
}
