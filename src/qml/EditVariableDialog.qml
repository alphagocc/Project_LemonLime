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

Controls.Dialog {
    id: root
    objectName: "EditVariableDialog"
    font.pointSize: 10
    property string variableName: ""
    property string variableValue: ""
    title: qsTranslate("EditVariableDialog", "Dialog")
    width: 325
    height: 128
    minimumWidth: 325
    maximumWidth: 325
    minimumHeight: 128
    maximumHeight: 128
    onOpened: {
        nameField.text = variableName
        valueField.text = variableValue
    }
    Shortcut { sequence: "Escape"; onActivated: root.reject() }
    Shortcut { sequence: "Return"; enabled: nameField.text.length > 0 && valueField.text.length > 0; onActivated: root.confirm() }
    Shortcut { sequence: "Enter"; enabled: nameField.text.length > 0 && valueField.text.length > 0; onActivated: root.confirm() }
    function confirm() {
        variableName = nameField.text
        variableValue = valueField.text
        accept()
    }

    Control {
        anchors.fill: parent
        font.pointSize: 10
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 11
            spacing: 8
            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: 2
                columnSpacing: 10
                rowSpacing: 8
                Label { text: qsTranslate("EditVariableDialog", "Variable Name") }
                Controls.TextField { id: nameField; Layout.fillWidth: true; selectByMouse: true }
                Label { text: qsTranslate("EditVariableDialog", "Variable Value") }
                Controls.TextField { id: valueField; Layout.fillWidth: true; selectByMouse: true }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Item { Layout.fillWidth: true }
                Controls.Button {
                    text: qsTranslate("QPlatformTheme", "OK")
                    highlighted: true
                    enabled: nameField.text.length > 0 && valueField.text.length > 0
                    onClicked: root.confirm()
                }
                Controls.Button { text: qsTranslate("QPlatformTheme", "Cancel"); onClicked: root.reject() }
            }
        }
    }
}
