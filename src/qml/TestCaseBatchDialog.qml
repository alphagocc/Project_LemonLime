/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import "controls" as Controls
import QtQuick.Controls
import QtQuick.Layouts

Controls.Dialog {
    id: root
    required property var controller
    property bool adding: false
    property bool fileOnly: false
    property int pairIndex: -1
    property var indices: []
    readonly property bool hasFiles: adding || fileOnly
    readonly property bool hasLimits: !fileOnly && controller.task.type !== 1
    readonly property bool hasDependencies: !fileOnly && (adding || indices.length === 1)
    width: 400
    height: 298
    minimumWidth: 400
    title: !fileOnly && (adding || indices.length === 1) ? qsTranslate("ExtTestCaseUpdaterDialog", "Configure Test Case #%1").arg(adding ? controller.subtasks.length + 1 : indices[0] + 1) : qsTranslate("ExtTestCaseUpdaterDialog", "Configure")
    function begin(add, selected) {
        adding = add
        fileOnly = false
        indices = selected.slice()
        const initial = add ? controller.defaults : selected.length === 1 ? controller.subtasks[selected[0]] : {}
        score.text = initial.score === undefined ? "" : String(initial.score)
        time.text = initial.time === undefined ? "" : String(initial.time)
        memory.text = initial.memory === undefined ? "" : String(initial.memory)
        dependencies.text = initial.dependencies ?? ""
        input.text = ""
        output.text = ""
        open()
    }
    function beginPair(subtask, pair) {
        adding = false
        fileOnly = true
        pairIndex = pair
        indices = [subtask]
        controller.selectSubtask(subtask)
        const initial = pair >= 0 ? controller.subtask.files[pair] : {}
        input.text = initial.input ?? ""
        output.text = initial.output ?? ""
        open()
    }
    function save() {
        if (fileOnly) {
            if (controller.setFilePair(pairIndex, input.text, output.text)) accept()
            return
        }
        let values = {}
        if (score.text.length) values.score = score.text
        if (time.text.length) values.time = time.text
        if (memory.text.length) values.memory = memory.text
        if (hasDependencies) values.dependencies = dependencies.text
        if (adding) {
            values.input = input.text
            values.output = output.text
            if (controller.task.type === 1) { values.time = controller.defaults.time; values.memory = controller.defaults.memory }
        }
        if (adding ? controller.addSubtask(values) : controller.updateSubtasks(indices, values)) accept()
    }
    ColumnLayout {
        id: content
        anchors.fill: parent; anchors.margins: 11; spacing: 6
        GridLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.margins: 9
            columns: 3; columnSpacing: 6; rowSpacing: 6
            Label { renderType: Text.NativeRendering; Layout.row: 0; Layout.column: 0; visible: !root.fileOnly; text: qsTranslate("ExtTestCaseUpdaterDialog", "Score"); font.pointSize: 10 }
            Controls.TextField { font.pointSize: 10; id: score; visible: !root.fileOnly; Layout.row: 0; Layout.column: 1; Layout.fillWidth: true; placeholderText: qsTranslate("ExtTestCaseUpdaterDialog", "No Change"); validator: IntValidator { bottom: 0; top: root.controller.defaults.maxScore } }
            Label { renderType: Text.NativeRendering; visible: root.hasFiles; Layout.row: 1; Layout.column: 0; text: qsTranslate("ExtTestCaseUpdaterDialog", "Input File"); font.pointSize: 10 }
            TaskFileField { id: input; visible: root.hasFiles; Layout.row: 1; Layout.column: 1; Layout.fillWidth: true; controller: root.controller }
            Controls.Button { font.pointSize: 10; visible: root.hasFiles; Layout.row: 1; Layout.column: 2; text: qsTranslate("ExtTestCaseUpdaterDialog", "View"); onClicked: input.browse() }
            Label { renderType: Text.NativeRendering; visible: root.hasFiles; Layout.row: 2; Layout.column: 0; text: qsTranslate("ExtTestCaseUpdaterDialog", "Output File"); font.pointSize: 10 }
            TaskFileField { id: output; visible: root.hasFiles; Layout.row: 2; Layout.column: 1; Layout.fillWidth: true; controller: root.controller }
            Controls.Button { font.pointSize: 10; visible: root.hasFiles; Layout.row: 2; Layout.column: 2; text: qsTranslate("ExtTestCaseUpdaterDialog", "View"); onClicked: output.browse() }
            Label { renderType: Text.NativeRendering; visible: root.hasLimits; Layout.row: 3; Layout.column: 0; text: qsTranslate("ExtTestCaseUpdaterDialog", "Time Limit"); font.pointSize: 10 }
            Controls.TextField { font.pointSize: 10; id: time; visible: root.hasLimits; Layout.row: 3; Layout.column: 1; Layout.fillWidth: true; placeholderText: qsTranslate("ExtTestCaseUpdaterDialog", "No Change"); validator: IntValidator { bottom: 1; top: root.controller.defaults.maxTime } }
            Label { renderType: Text.NativeRendering; visible: root.hasLimits; Layout.row: 3; Layout.column: 2; text: qsTranslate("ExtTestCaseUpdaterDialog", "ms"); font.pointSize: 10 }
            Label { renderType: Text.NativeRendering; visible: root.hasLimits; Layout.row: 4; Layout.column: 0; text: qsTranslate("ExtTestCaseUpdaterDialog", "Memory Limit"); font.pointSize: 10 }
            Controls.TextField { font.pointSize: 10; id: memory; visible: root.hasLimits; Layout.row: 4; Layout.column: 1; Layout.fillWidth: true; placeholderText: qsTranslate("ExtTestCaseUpdaterDialog", "No Change"); validator: IntValidator { bottom: 1; top: root.controller.defaults.maxMemory } }
            Label { renderType: Text.NativeRendering; visible: root.hasLimits; Layout.row: 4; Layout.column: 2; text: qsTranslate("ExtTestCaseUpdaterDialog", "MiB"); font.pointSize: 10 }
            Label { renderType: Text.NativeRendering; visible: root.hasDependencies; Layout.row: 5; Layout.column: 0; text: qsTranslate("ExtTestCaseUpdaterDialog", "Depends"); font.pointSize: 10 }
            Controls.TextField { font.pointSize: 10; id: dependencies; visible: root.hasDependencies; Layout.row: 5; Layout.column: 1; Layout.fillWidth: true; placeholderText: qsTranslate("ExtTestCaseUpdaterDialog", "seperate by \",\"") }
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            Controls.Button { font.pointSize: 10; text: qsTranslate("QPlatformTheme", "OK"); enabled: !root.controller.busy; onClicked: root.save() }
            Controls.Button { font.pointSize: 10; text: qsTranslate("QPlatformTheme", "Cancel"); onClicked: root.reject() }
        }
    }
}
