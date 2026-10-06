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

Item {
    id: root
    objectName: "VisualMainSettings"
    required property var controller
    implicitWidth: 400
    implicitHeight: 168

    GridLayout {
        anchors.fill: parent
        anchors.margins: 9
        columns: 2
        columnSpacing: 6
        rowSpacing: 6
        Label { text: qsTranslate("VisualMainSettings", "Appearance") }
        Controls.ComboBox {
            Layout.fillWidth: true
            model: [qsTranslate("VisualMainSettings", "System"), qsTranslate("VisualMainSettings", "Light"), qsTranslate("VisualMainSettings", "Dark")]
            currentIndex: root.controller.general.colorScheme
            onActivated: root.controller.setGeneral("colorScheme", currentIndex)
        }
        Label { text: qsTranslate("VisualMainSettings", "Theme") }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            Controls.ComboBox {
                Layout.fillWidth: true
                model: root.controller.themeNames
                currentIndex: root.controller.themeIndex
                onActivated: root.controller.themeIndex = currentIndex
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Controls.Button {
                    text: qsTranslate("VisualMainSettings", "Edit")
                    onClicked: themeEditor.open()
                }
                Controls.ToolButton {
                    icon.source: "image://lemon-icons/list-add?palette=" + StyleMetrics.paletteRevision
                    onClicked: root.controller.addTheme()
                }
                Item { Layout.fillWidth: true }
                Controls.ToolButton {
                    icon.source: "image://lemon-icons/edit-delete?palette=" + StyleMetrics.paletteRevision
                    onClicked: root.controller.deleteTheme()
                }
            }
        }
        Label { text: qsTranslate("VisualMainSettings", "Splash Time") }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Controls.TextField {
                Layout.preferredWidth: 96
                Layout.maximumWidth: 96
                text: String(root.controller.general.splashTime)
                validator: IntValidator { bottom: 0; top: 3000 }
                selectByMouse: true
                onTextEdited: root.controller.setGeneral("splashTime", text)
            }
            Label { text: qsTranslate("VisualMainSettings", "ms") }
            Item { Layout.fillWidth: true }
        }
    }
    ThemeEditDialog { id: themeEditor; controller: root.controller }
}
