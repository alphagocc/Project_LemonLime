/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

import QtQuick
import "controls" as Controls
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Dialogs

Control {
    id: root
    required property var controller
    signal closeRequested()
    implicitWidth: 533
    implicitHeight: 437
    font.pointSize: 10

    function reset() { tabs.currentIndex = 0 }
    function confirm() {
        forceActiveFocus()
        if (controller.apply()) {
            closeRequested()
        } else {
            tabs.currentIndex = controller.validationPage
            errorDialog.open()
        }
    }
    Shortcut { sequence: "Return"; onActivated: root.confirm() }
    Shortcut { sequence: "Enter"; onActivated: root.confirm() }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 11
        spacing: 6
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            Controls.TabBar {
                id: tabs
                objectName: "settingsTabs"
                Layout.fillWidth: true
                Controls.TabButton { text: qsTranslate("OptionsDialog", "General"); first: true }
                Controls.TabButton { text: qsTranslate("OptionsDialog", "Compiler") }
                Controls.TabButton { text: qsTranslate("OptionsDialog", "Visual"); last: true }
            }
            Control {
                Layout.fillWidth: true
                Layout.fillHeight: true
                padding: 2
                background: StyleItem { kind: "tabframe" }
                contentItem: StackLayout {
                    currentIndex: tabs.currentIndex
                    SettingsGeneral { id: general; controller: root.controller }
                    SettingsCompilers { controller: root.controller }
                    SettingsAppearance { controller: root.controller }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Item { Layout.fillWidth: true }
            Controls.Button {
                text: qsTranslate("QPlatformTheme", "OK")
                highlighted: true
                onClicked: root.confirm()
            }
            Controls.Button {
                text: qsTranslate("QPlatformTheme", "Cancel")
                onClicked: {
                    root.controller.cancel()
                    root.closeRequested()
                }
            }
        }
    }
    Dialogs.MessageDialog {
        id: errorDialog
        title: qsTranslate("GeneralSettings", "Error")
        text: root.controller.error
        buttons: Dialogs.MessageDialog.Close
    }
}
