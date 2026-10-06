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
    objectName: "GeneralSettings"
    required property var controller
    readonly property var values: controller.general
    implicitWidth: fields.implicitWidth + 18
    implicitHeight: 325

    component NumberField: RowLayout {
        id: field
        Layout.fillHeight: false
        Layout.maximumHeight: implicitHeight
        required property string key
        property string unit: ""
        property int minimum: 1
        required property int maximum
        spacing: 0
        Controls.TextField {
            objectName: field.key
            maximumLength: field.key === "defaultFullScore" ? 9 : 32767
            Layout.preferredWidth: 96
            Layout.maximumWidth: 96
            text: String(root.values[field.key])
            validator: IntValidator { bottom: field.minimum; top: field.maximum }
            selectByMouse: true
            onTextEdited: root.controller.setGeneral(field.key, text)
        }
        Label {
            visible: field.unit.length > 0
            Layout.leftMargin: 12
            text: field.unit
        }
        Item { Layout.fillWidth: true }
    }

    GridLayout {
        id: fields
        anchors.fill: parent
        anchors.margins: 9
        columns: 4
        columnSpacing: 12
        rowSpacing: 10

        Label { Layout.row: 0; Layout.column: 0; text: qsTranslate("GeneralSettings", "Default Full Score") }
        NumberField { Layout.row: 0; Layout.column: 1; Layout.fillWidth: true; key: "defaultFullScore"; maximum: root.controller.limits.fullScore }
        Label { Layout.row: 1; Layout.column: 0; text: qsTranslate("GeneralSettings", "Default Time Limit") }
        NumberField { Layout.row: 1; Layout.column: 1; Layout.fillWidth: true; key: "defaultTimeLimit"; maximum: root.controller.limits.time; unit: qsTranslate("GeneralSettings", "ms") }
        Label { Layout.row: 2; Layout.column: 0; text: qsTranslate("GeneralSettings", "Default Memory Limit") }
        NumberField { Layout.row: 2; Layout.column: 1; Layout.fillWidth: true; key: "defaultMemoryLimit"; maximum: root.controller.limits.memory; unit: qsTranslate("GeneralSettings", "MiB") }
        Label { Layout.row: 3; Layout.column: 0; text: qsTranslate("GeneralSettings", "Compile Time Limit") }
        NumberField { Layout.row: 3; Layout.column: 1; Layout.fillWidth: true; key: "compileTimeLimit"; maximum: root.controller.limits.time; unit: qsTranslate("GeneralSettings", "ms") }
        Label { Layout.row: 4; Layout.column: 0; text: qsTranslate("GeneralSettings", "Special Judge Time Limit") }
        NumberField { Layout.row: 4; Layout.column: 1; Layout.fillWidth: true; key: "specialJudgeTimeLimit"; maximum: root.controller.limits.time; unit: qsTranslate("GeneralSettings", "ms") }
        Label { Layout.row: 0; Layout.column: 2; text: qsTranslate("GeneralSettings", "Default Extra Time Limit Ratio") }
        RowLayout {
            Layout.row: 0; Layout.column: 3; Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.maximumHeight: implicitHeight
            spacing: 0
            Controls.TextField {
                objectName: "defaultExtraTimeRatio"
                Layout.preferredWidth: 96; Layout.maximumWidth: 96
                text: String(root.values.defaultExtraTimeRatio)
                validator: DoubleValidator { bottom: 0; top: root.controller.limits.extraTime; decimals: 6 }
                selectByMouse: true
                onTextEdited: root.controller.setGeneral("defaultExtraTimeRatio", text)
            }
            Item { Layout.fillWidth: true }
        }
        Label { Layout.row: 1; Layout.column: 2; text: qsTranslate("GeneralSettings", "Maximum Judging Thread") }
        NumberField { Layout.row: 1; Layout.column: 3; Layout.fillWidth: true; key: "maxJudgingThreads"; maximum: root.controller.limits.threads }
        Label { Layout.row: 2; Layout.column: 2; text: qsTranslate("GeneralSettings", "Source File Size Limit") }
        NumberField { Layout.row: 2; Layout.column: 3; Layout.fillWidth: true; key: "fileSizeLimit"; maximum: root.controller.limits.fileSize; unit: qsTranslate("GeneralSettings", "KiB") }
        Label { Layout.row: 3; Layout.column: 2; text: qsTranslate("GeneralSettings", "Maximum Rejudge Times") }
        NumberField { Layout.row: 3; Layout.column: 3; Layout.fillWidth: true; key: "rejudgeTimes"; maximum: root.controller.limits.rejudge; minimum: 0 }
        Label { Layout.row: 7; Layout.column: 0; text: qsTranslate("GeneralSettings", "Input File Extensions") }
        Controls.TextField {
            Layout.row: 7; Layout.column: 1; Layout.fillWidth: true
            objectName: "inputFileExtensions"
            text: root.values.inputFileExtensions
            validator: RegularExpressionValidator { regularExpression: /(\w+;)*\w+/ }
            selectByMouse: true
            onTextEdited: root.controller.setGeneral("inputFileExtensions", text)
        }
        Label { Layout.row: 7; Layout.column: 2; text: qsTranslate("GeneralSettings", "Output File Extensions") }
        Controls.TextField {
            Layout.row: 7; Layout.column: 3; Layout.fillWidth: true
            objectName: "outputFileExtensions"
            text: root.values.outputFileExtensions
            validator: RegularExpressionValidator { regularExpression: /(\w+;)*\w+/ }
            selectByMouse: true
            onTextEdited: root.controller.setGeneral("outputFileExtensions", text)
        }
        Label {
            Layout.row: 9; Layout.column: 0; Layout.columnSpan: 2
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignRight
            text: qsTranslate("GeneralSettings", "(separated by \";\". Empty means no limitation.)")
        }
        // The original grid reserves its extra height for the two language rows.
        Label {
            id: languageLabel
            Layout.row: 10; Layout.column: 0
            Layout.fillHeight: true
            Layout.verticalStretchFactor: 1
            verticalAlignment: Text.AlignVCenter
            text: qsTranslate("GeneralSettings", "Language")
        }
        Item {
            Layout.row: 11; Layout.column: 0
            Layout.fillHeight: true
            Layout.verticalStretchFactor: 1
            implicitHeight: languageLabel.implicitHeight
        }
        Controls.ComboBox {
            objectName: "languageComboBox"
            Layout.row: 10; Layout.column: 1; Layout.rowSpan: 2; Layout.fillWidth: true
            model: root.controller.languages
            currentIndex: Math.max(0, root.controller.languages.indexOf(root.values.language))
            enabled: count > 0
            ToolTip.text: count === 0 ? "Cannot find any language providers." : ""
            ToolTip.visible: count === 0 && hovered
            onActivated: root.controller.setGeneral("language", currentText)
        }
        Controls.CheckBox {
            objectName: "preventSleepWhileJudging"
            Layout.row: 10; Layout.column: 2; Layout.columnSpan: 2; Layout.rowSpan: 2
            text: qsTranslate("GeneralSettings", "Prevent system sleep while judging")
            checked: root.values.preventSleepWhileJudging
            onToggled: root.controller.setGeneral("preventSleepWhileJudging", checked)
        }
    }
}
