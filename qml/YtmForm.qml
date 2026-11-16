import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Scale.js" as S

// YouTube Music: log in with a browser that is already signed in to it (its cookies are read again on
// every sync, so they stay fresh), or with a pasted request. Then its songs and playlists join the library.
// What is kept in the system keyring is the browser's name, or the pasted login.
ColumnLayout {
    id: root
    spacing: S.sp(10)

    property bool pasting: false
    Component.onCompleted: ytm.findBrowsers()

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
                text: ytm.status !== "" ? ytm.status : qsTr("Not logged in")
                color: ytm.state === "connected" ? theme.foreground
                       : ytm.state === "syncing" || ytm.status === "" ? theme.dimForeground : theme.danger
            }
            Row {
                spacing: S.sp(8)
                ChipButton { visible: root.signedIn; text: qsTr("Refresh"); enabled: ytm.state !== "syncing"; onClicked: ytm.sync() }
                ChipButton { visible: root.signedIn; text: qsTr("Log out"); danger: true; onClicked: ytm.logout() }
            }
        }
    }

    readonly property bool needsLogin: !signedIn || ytm.state === "expired"

    UiText {
        visible: root.needsLogin
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: theme.dimForeground
        px: 12
        text: ytm.browsers.length > 0
              ? qsTr("Sign in to music.youtube.com in a browser first, then pick it here. OMP reads that browser's YouTube cookies each time it syncs.")
              : qsTr("No browser with a cookie file was found. Paste a request instead.")
    }

    Flow {
        visible: root.needsLogin
        Layout.fillWidth: true
        Layout.preferredHeight: implicitHeight
        spacing: S.sp(8)
        Repeater {
            model: ytm.browsers
            ChipButton {
                required property var modelData
                text: modelData.label
                enabled: ytm.state !== "syncing"
                onClicked: ytm.loginWithBrowser(modelData.spec)
            }
        }
        ChipButton { text: root.pasting ? qsTr("Hide pasting") : qsTr("Paste a request instead"); onClicked: root.pasting = !root.pasting }
    }

    UiText {
        visible: root.needsLogin && root.pasting
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: theme.dimForeground
        px: 12
        text: qsTr("In your browser on music.youtube.com press F12, open the Network tab and type \"browse\" in the filter. "
                   + "Click around in the page so a request shows up, right-click it, choose Copy > Copy as cURL, and paste it here.")
    }

    TextArea {
        id: paste
        visible: root.needsLogin && root.pasting
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

    ToggleRow {
        visible: settingsStore.ytmSeparateTab
        Layout.fillWidth: true
        label: qsTr("Show YouTube Music playlists in Playlists")
        checked: settingsStore.ytmPlaylistsInPlaylists
        onToggled: (v) => settingsStore.set("ytmPlaylistsInPlaylists", v)
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
