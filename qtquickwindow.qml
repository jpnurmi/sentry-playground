import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SentryPlayground

ApplicationWindow {
    title: "QtQuick - Sentry Playground"
    width: 520
    height: 360
    visible: true

    menuBar: MenuBar {
        Menu {
            title: "&File"
            Action {
                text: "&Quit"
                onTriggered: Qt.quit()
            }
        }
        Menu {
            title: "&View"
            Action {
                text: "Qt&Widgets"
                onTriggered: SentryPlayground.showWidgets()
            }
            Action {
                text: "Qt&Quick"
                onTriggered: SentryPlayground.showQuick()
            }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent

        Label {
            text: SentryPlayground.backend
            horizontalAlignment: Qt.AlignHCenter
            Layout.fillWidth: true
        }
        Button {
            text: "Crash"
            onClicked: SentryPlayground.triggerCrash()
            Layout.fillWidth: true
        }
        Button {
            text: "Stack overflow"
            onClicked: SentryPlayground.triggerStackOverflow()
            Layout.fillWidth: true
        }
        Button {
            text: "Fast-fail"
            enabled: Qt.platform.os === "windows"
            onClicked: SentryPlayground.triggerFastfail()
            Layout.fillWidth: true
        }
        Button {
            text: "Assert"
            onClicked: SentryPlayground.triggerAssertFailure()
            Layout.fillWidth: true
        }
        Button {
            text: "Abort"
            onClicked: SentryPlayground.triggerAbort()
            Layout.fillWidth: true
        }
        CheckBox {
            text: "Worker"
            checked: SentryPlayground.worker
            onToggled: SentryPlayground.worker = checked
            Layout.fillWidth: true
        }
    }
}
