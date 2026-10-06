/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Dialogs
import "controls" as Controls

Controls.Dialog {
    id: root
    objectName: "ThemeEditDialog"
    font.pointSize: 9
    required property var controller
    property var draft: ({})
    readonly property var colorRows: [
        { key: "maximum", label: qsTranslate("VisualSettings", "Color Max (HSL)"), hMax: 720 },
        { key: "minimum", label: qsTranslate("VisualSettings", "Color Min (HSL)"), hMax: 719 },
        { key: "noFile", label: qsTranslate("VisualSettings", "Color NoFile (HSL)"), hMax: 359 },
        { key: "compileError", label: qsTranslate("VisualSettings", "Color CE (HSL)"), hMax: 359 },
        { key: "compensation", label: qsTranslate("VisualSettings", "Grand Compensation"), hMax: 360 },
        { key: "rate", label: qsTranslate("VisualSettings", "Grand Rate"), hMax: 10 }
    ]
    title: qsTranslate("ThemeEditDialog", "Dialog")
    width: 533
    height: 300
    minimumWidth: 533
    minimumHeight: 300
    onOpened: draft = JSON.parse(JSON.stringify(controller.theme))

    function change(key, value) {
        const changed = Object.assign({}, draft)
        changed[key] = value
        draft = changed
    }
    function confirm() {
        editorContents.forceActiveFocus()
        if (controller.replaceTheme(draft))
            accept()
        else
            errorDialog.open()
    }

    Shortcut { sequence: "Escape"; onActivated: root.reject() }
    Shortcut { sequence: "Return"; onActivated: root.confirm() }
    Shortcut { sequence: "Enter"; onActivated: root.confirm() }

    Control {
        id: editorContents
        anchors.fill: parent
        font.pointSize: 9

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 11
            spacing: 6

            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 9
                columns: 2
                columnSpacing: 6
                rowSpacing: 6

                Label {
                    Layout.row: 0
                    Layout.column: 0
                    text: qsTranslate("VisualSettings", "Theme Name")
                }
                Controls.TextField {
                    objectName: "themeName"
                    Layout.row: 0
                    Layout.column: 1
                    Layout.fillWidth: true
                    text: root.draft.name || ""
                    onTextEdited: root.change("name", text)
                }
                Repeater {
                    model: root.colorRows
                    delegate: Label {
                        required property var modelData
                        required property int index
                        Layout.row: index + 1
                        Layout.column: 0
                        font.pointSize: 10
                        text: modelData.label
                    }
                }
                Repeater {
                    model: root.colorRows
                    delegate: RowLayout {
                        id: themeRow
                        required property var modelData
                        required property int index
                        Layout.row: index + 1
                        Layout.column: 1
                        Layout.fillWidth: true
                        spacing: 6

                        Repeater {
                            model: ["H", "S", "L"]
                            delegate: SettingsDoubleSpinBox {
                                required property string modelData
                                Layout.fillWidth: true
                                font.pointSize: 10
                                realValue: root.draft[themeRow.modelData.key + modelData] || 0
                                minimum: themeRow.modelData.key === "compensation" ? (modelData === "H" ? -360 : -100) : themeRow.modelData.key === "rate" ? -10 : 0
                                maximum: modelData === "H" ? themeRow.modelData.hMax : themeRow.modelData.key === "rate" ? 10 : 100
                                decimals: modelData === "H" && themeRow.index < 4 ? 0 : 2
                                step: themeRow.modelData.key === "rate" ? 0.1 : 1
                                onRealValueModified: number => root.change(themeRow.modelData.key + modelData, number)
                            }
                        }
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
                    onClicked: root.reject()
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
