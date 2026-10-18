import QtQuick
import QtQuick.Controls.Basic

ApplicationWindow {
    objectName: "mainWindow"
    title: "omamusic"
    width: 1000
    height: 640
    visible: false
    color: theme.background
    onClosing: (close) => { close.accepted = false; visible = false }

    Text { anchors.centerIn: parent; text: "omamusic"; color: theme.foreground }
}
