/* SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick
import "controls" as Controls
import QtQuick.Controls
import QtQuick.Layouts
Controls.Dialog {
    id: dialog
    required property var controller
    property string mode: "welcome"
    property alias currentIndex: tabs.currentIndex
    title: mode === "welcome" ? qsTranslate("WelcomeDialog", "Welcome") : mode === "new" ? qsTranslate("NewContestDialog", "New Contest") : qsTranslate("OpenContestDialog", "Open an Existing Contest")
    width: mode === "welcome" ? 470 : 450
    height: mode === "welcome" ? 350 : 320
    minimumWidth: mode === "welcome" ? 470 : 450
    minimumHeight: mode === "welcome" ? 350 : 320
    onOpened: { tabs.currentIndex = 0; openForm.selectedIndex = -1 }
    function submit() {
        const creating = mode === "new" || (mode === "welcome" && tabs.currentIndex === 1)
        const success = creating ? controller.newContest(newForm.contestTitle, controller.localUrl(newForm.contestPath), newForm.savingName) : controller.openContest(controller.localUrl(openForm.selectedFile))
        if (success) accept()
    }
    Control {
        anchors.fill: parent; font.pointSize: 10
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 11; spacing: 6
            Item {
                Layout.fillWidth: true; Layout.fillHeight: true
                Controls.TabBar {
                    id: tabs; visible: dialog.mode === "welcome"; anchors.left: parent.left; anchors.top: parent.top
                    Controls.TabButton { text: qsTranslate("WelcomeDialog", "Open"); first: true }
                    Controls.TabButton { text: qsTranslate("WelcomeDialog", "New"); last: true }
                }
                StyleItem { kind: "tabframe"; visible: tabs.visible; anchors.top: tabs.bottom; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom }
                StackLayout {
                    anchors.fill: parent
                    anchors.topMargin: tabs.visible ? tabs.height + 9 : 0
                    anchors.leftMargin: tabs.visible ? 11 : 0
                    anchors.rightMargin: tabs.visible ? 11 : 0
                    anchors.bottomMargin: tabs.visible ? 11 : 0
                    currentIndex: dialog.mode === "new" ? 1 : dialog.mode === "open" ? 0 : tabs.currentIndex
                    OpenContestForm { id: openForm; controller: dialog.controller; onActivated: dialog.submit() }
                    NewContestForm { id: newForm; controller: dialog.controller }
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 6
                Item { Layout.fillWidth: true }
                Controls.Button { text: qsTranslate("QPlatformTheme", "OK"); enabled: dialog.mode === "new" || (dialog.mode === "welcome" && tabs.currentIndex === 1) ? newForm.ready : openForm.ready; onClicked: dialog.submit() }
                Controls.Button { text: qsTranslate("QPlatformTheme", "Cancel"); onClicked: dialog.reject() }
            }
        }
    }
}
