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

Item {
    id: root
    required property var controller
    readonly property var taskData: controller.task
    readonly property bool answersOnly: taskData.type === 1
    readonly property bool interaction: taskData.type === 2
    readonly property bool communication: taskData.type === 3 || taskData.type === 4
    property int selectedCompiler: controller.compilers.length > 0 ? 0 : -1
    property int observedTask: -1
    property int sourceSelection: -1
    property int graderSelection: -1
    SystemPalette { id: colors }
    function update(key, value) {
        if (String(taskData[key]) === String(value)) return
        let values = {}
        values[key] = value
        controller.updateTask(values)
    }
    function appendMapping(grader) {
        if (!mappingSource.text.length || !mappingName.text.length) return
        if (controller.setFileMapping(grader, -1, mappingSource.text, mappingName.text)) {
            mappingSource.text = ""
            mappingName.text = ""
        }
    }
    Connections {
        target: root.controller
        function onChanged() {
            if (root.observedTask !== root.controller.selectedTask) {
                root.observedTask = root.controller.selectedTask
                root.selectedCompiler = root.controller.compilers.length > 0 ? 0 : -1
                root.sourceSelection = -1
                root.graderSelection = -1
            }
        }
    }
    GridLayout {
        anchors.fill: parent
        anchors.margins: 9
        columns: 3
        columnSpacing: 6
        rowSpacing: 6

        Label { renderType: Text.NativeRendering; Layout.row: 0; Layout.column: 0; Layout.minimumWidth: 150; text: qsTranslate("TaskEditWidget", "Problem Title"); font.pointSize: 10 }
        Controls.TextField { font.pointSize: 10;
            objectName: "problemTitle"
            Layout.row: 0; Layout.column: 1; Layout.columnSpan: 2; Layout.fillWidth: true
            text: root.taskData.title ?? ""
            onTextEdited: root.update("title", text)
        }
        Label { renderType: Text.NativeRendering; Layout.row: 1; Layout.column: 0; text: qsTranslate("TaskEditWidget", "Task Type"); font.pointSize: 10 }
        ColumnLayout {
            Layout.row: 1; Layout.column: 1; Layout.columnSpan: 2; Layout.fillWidth: true
            spacing: 0
            ButtonGroup { id: typeGroup }
            RowLayout {
                Layout.fillWidth: true; spacing: 0; uniformCellSizes: true
                Controls.RadioButton { Layout.fillWidth: true; font.pointSize: 10; text: qsTranslate("TaskEditWidget", "Traditional"); checked: root.taskData.type === 0; ButtonGroup.group: typeGroup; onClicked: root.update("type", 0) }
                Controls.RadioButton { Layout.fillWidth: true; font.pointSize: 10; text: qsTranslate("TaskEditWidget", "Answers Only"); checked: root.taskData.type === 1; ButtonGroup.group: typeGroup; onClicked: root.update("type", 1) }
                Controls.RadioButton { Layout.fillWidth: true; font.pointSize: 10; text: qsTranslate("TaskEditWidget", "Interaction"); checked: root.taskData.type === 2; ButtonGroup.group: typeGroup; onClicked: root.update("type", 2) }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 0; uniformCellSizes: true
                Controls.RadioButton { Layout.fillWidth: true; font.pointSize: 10; text: qsTranslate("TaskEditWidget", "Communication (Part)"); checked: root.taskData.type === 3; ButtonGroup.group: typeGroup; onClicked: root.update("type", 3) }
                Controls.RadioButton { Layout.fillWidth: true; font.pointSize: 10; text: qsTranslate("TaskEditWidget", "Communication (Exec)"); checked: root.taskData.type === 4; ButtonGroup.group: typeGroup; onClicked: root.update("type", 4) }
            }
        }
        Label { renderType: Text.NativeRendering; Layout.row: 2; Layout.column: 0; text: qsTranslate("TaskEditWidget", "Source File Name"); font.pointSize: 10 }
        Controls.TextField { font.pointSize: 10; objectName: "sourceFileName"; Layout.row: 2; Layout.column: 1; Layout.fillWidth: true; text: root.taskData.source ?? ""; onTextEdited: root.update("source", text) }
        Controls.CheckBox { font.pointSize: 10; Layout.row: 2; Layout.column: 2; text: qsTranslate("TaskEditWidget", "Subfolder"); checked: root.taskData.subfolder ?? false; onClicked: root.update("subfolder", checked) }

        Label { renderType: Text.NativeRendering; visible: root.communication; Layout.row: 3; Layout.column: 0; text: qsTranslate("TaskEditWidget", "Source Files"); font.pointSize: 10 }
        TaskMappingTable { visible: root.communication; Layout.row: 3; Layout.column: 1; Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 60; model: root.taskData.sources ?? []; currentIndex: root.sourceSelection; onSelected: index => root.sourceSelection = index }
        ColumnLayout {
            visible: root.communication; Layout.row: 3; Layout.column: 2
            Controls.Button { font.pointSize: 10; text: qsTranslate("TaskEditWidget", "Append"); icon.source: "image://lemon-icons/list-add?palette=" + StyleMetrics.paletteRevision; onClicked: root.appendMapping(false) }
            Controls.Button { font.pointSize: 10; text: qsTranslate("TaskEditWidget", "Remove"); icon.source: "image://lemon-icons/list-remove?palette=" + StyleMetrics.paletteRevision; enabled: root.sourceSelection >= 0; onClicked: { root.controller.removeFileMapping(false, root.sourceSelection); root.sourceSelection = -1 } }
        }
        Label { renderType: Text.NativeRendering; visible: root.communication; Layout.row: 5; Layout.column: 0; text: qsTranslate("TaskEditWidget", "Grader Files"); font.pointSize: 10 }
        TaskMappingTable { visible: root.communication; Layout.row: 5; Layout.column: 1; Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 60; model: root.taskData.graders ?? []; currentIndex: root.graderSelection; onSelected: index => root.graderSelection = index }
        ColumnLayout {
            visible: root.communication; Layout.row: 5; Layout.column: 2
            Controls.Button { font.pointSize: 10; text: qsTranslate("TaskEditWidget", "Append"); icon.source: "image://lemon-icons/list-add?palette=" + StyleMetrics.paletteRevision; onClicked: root.appendMapping(true) }
            Controls.Button { font.pointSize: 10; text: qsTranslate("TaskEditWidget", "Remove"); icon.source: "image://lemon-icons/list-remove?palette=" + StyleMetrics.paletteRevision; enabled: root.graderSelection >= 0; onClicked: { root.controller.removeFileMapping(true, root.graderSelection); root.graderSelection = -1 } }
        }
        Label { renderType: Text.NativeRendering; visible: root.communication; Layout.row: 7; Layout.column: 0; text: qsTranslate("TaskEditWidget", "Path / Name"); font.pointSize: 10 }
        RowLayout {
            visible: root.communication; Layout.row: 7; Layout.column: 1; Layout.fillWidth: true
            Controls.TextField { font.pointSize: 10; id: mappingSource; Layout.fillWidth: true }
            Controls.TextField { font.pointSize: 10; id: mappingName; Layout.fillWidth: true }
        }
        Label { renderType: Text.NativeRendering; visible: root.answersOnly; Layout.row: 8; Layout.column: 0; text: qsTranslate("TaskEditWidget", "Answer Extension"); font.pointSize: 10 }
        Controls.TextField { font.pointSize: 10; visible: root.answersOnly; Layout.row: 8; Layout.column: 1; Layout.fillWidth: true; text: root.taskData.answerExtension ?? ""; onTextEdited: root.update("answerExtension", text) }
        Label { renderType: Text.NativeRendering; visible: !root.answersOnly; Layout.row: 9; Layout.column: 0; text: qsTranslate("TaskEditWidget", "Input File Name"); font.pointSize: 10 }
        Controls.TextField { font.pointSize: 10; objectName: "inputFileName"; visible: !root.answersOnly; Layout.row: 9; Layout.column: 1; Layout.fillWidth: true; enabled: !root.taskData.standardInput; text: root.taskData.input ?? ""; onTextEdited: root.update("input", text) }
        Controls.CheckBox { font.pointSize: 10; visible: !root.answersOnly; Layout.row: 9; Layout.column: 2; text: qsTranslate("TaskEditWidget", "Standard input"); checked: root.taskData.standardInput ?? false; onClicked: root.update("standardInput", checked) }
        Label { renderType: Text.NativeRendering; visible: !root.answersOnly; Layout.row: 10; Layout.column: 0; text: qsTranslate("TaskEditWidget", "Output File Name"); font.pointSize: 10 }
        Controls.TextField { font.pointSize: 10; objectName: "outputFileName"; visible: !root.answersOnly; Layout.row: 10; Layout.column: 1; Layout.fillWidth: true; enabled: !root.taskData.standardOutput; text: root.taskData.output ?? ""; onTextEdited: root.update("output", text) }
        Controls.CheckBox { font.pointSize: 10; visible: !root.answersOnly; Layout.row: 10; Layout.column: 2; text: qsTranslate("TaskEditWidget", "Standard output"); checked: root.taskData.standardOutput ?? false; onClicked: root.update("standardOutput", checked) }
        Label { renderType: Text.NativeRendering; visible: root.interaction; Layout.row: 11; Layout.column: 0; text: qsTranslate("TaskEditWidget", "Interactor Path"); font.pointSize: 10 }
        TaskFileField { visible: root.interaction; Layout.row: 11; Layout.column: 1; Layout.columnSpan: 2; Layout.fillWidth: true; controller: root.controller; text: root.taskData.interactor ?? ""; onEdited: value => root.update("interactor", value) }
        Label { renderType: Text.NativeRendering; visible: root.interaction; Layout.row: 12; Layout.column: 0; text: qsTranslate("TaskEditWidget", "Interactor Name"); font.pointSize: 10 }
        Controls.TextField { font.pointSize: 10; visible: root.interaction; Layout.row: 12; Layout.column: 1; Layout.columnSpan: 2; Layout.fillWidth: true; text: root.taskData.interactorName ?? ""; onTextEdited: root.update("interactorName", text) }
        Label { renderType: Text.NativeRendering; visible: root.interaction; Layout.row: 13; Layout.column: 0; text: qsTranslate("TaskEditWidget", "Grader Path"); font.pointSize: 10 }
        TaskFileField { visible: root.interaction; Layout.row: 13; Layout.column: 1; Layout.columnSpan: 2; Layout.fillWidth: true; controller: root.controller; text: root.taskData.grader ?? ""; onEdited: value => root.update("grader", value) }
        Label { renderType: Text.NativeRendering; Layout.row: 16; Layout.column: 0; text: qsTranslate("TaskEditWidget", "Comparison Mode"); font.pointSize: 10 }
        Controls.ComboBox { font.pointSize: 10;
            Layout.row: 16; Layout.column: 1; Layout.columnSpan: 2; Layout.fillWidth: true
            model: [qsTranslate("TaskEditWidget", "Line-by-line mode"), qsTranslate("TaskEditWidget", "Line-by-line mode (ignore extra spaces and tabs)"), qsTranslate("TaskEditWidget", "External tool mode (diff)"), qsTranslate("TaskEditWidget", "Real number mode"), qsTranslate("TaskEditWidget", "Special judge mode (lemon)"), qsTranslate("TaskEditWidget", "Special judge mode (testlib)")]
            currentIndex: root.taskData.comparison ?? 0
            onActivated: root.update("comparison", currentIndex)
        }
        Item {
            Layout.row: 17; Layout.column: 1; Layout.columnSpan: 2; Layout.fillWidth: true
            Layout.preferredHeight: 44
            RowLayout {
                visible: root.taskData.comparison === 2
                anchors.fill: parent; anchors.margins: 9
                Label { renderType: Text.NativeRendering; text: qsTranslate("TaskEditWidget", "Arguments:") }
                Controls.TextField { font.pointSize: 10; Layout.fillWidth: true; text: root.taskData.diffArguments ?? ""; onTextEdited: root.update("diffArguments", text) }
            }
            RowLayout {
                visible: root.taskData.comparison === 3
                anchors.fill: parent; anchors.margins: 9
                Label { renderType: Text.NativeRendering; text: qsTranslate("TaskEditWidget", "Absolute or Relative error does not exceed 10 ^ (minus") }
                Controls.SpinBox { font.pointSize: 10; from: 0; to: 18; value: root.taskData.precision ?? 3; onValueModified: root.update("precision", value) }
                Label { renderType: Text.NativeRendering; text: qsTranslate("TaskEditWidget", ")") }
                Item { Layout.fillWidth: true }
            }
            RowLayout {
                visible: root.taskData.comparison >= 4
                anchors.fill: parent; anchors.margins: 9
                Label { renderType: Text.NativeRendering; text: qsTranslate("TaskEditWidget", "Exec File Path:") }
                TaskFileField { Layout.fillWidth: true; controller: root.controller; text: root.taskData.specialJudge ?? ""; onEdited: value => root.update("specialJudge", value) }
            }
        }
        Label { renderType: Text.NativeRendering; visible: !root.answersOnly; Layout.row: 21; Layout.column: 0; Layout.alignment: Qt.AlignTop; text: qsTranslate("TaskEditWidget", "Compiler Settings"); font.pointSize: 10 }
        ColumnLayout {
            visible: !root.answersOnly; Layout.row: 21; Layout.column: 1; Layout.columnSpan: 2; Layout.fillWidth: true; Layout.fillHeight: true; spacing: 12
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 45
                color: colors.base; border.color: colors.mid
                ListView {
                    id: compilers
                    anchors.fill: parent; anchors.margins: 1; clip: true
                    model: root.controller.compilers
                    ScrollBar.vertical: Controls.ScrollBar {}
                    delegate: Rectangle {
                        required property int index
                        required property var modelData
                        width: compilers.width; height: 18
                        color: root.selectedCompiler === index ? colors.highlight : colors.base
                        Label { renderType: Text.NativeRendering; anchors.fill: parent; anchors.leftMargin: 3; verticalAlignment: Text.AlignVCenter; font.pointSize: 10; text: modelData.name; color: root.selectedCompiler === index ? colors.highlightedText : colors.text }
                        MouseArea { anchors.fill: parent; onClicked: root.selectedCompiler = index }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 15
                Label { renderType: Text.NativeRendering; text: qsTranslate("TaskEditWidget", "Configuration:"); enabled: root.selectedCompiler >= 0; font.pointSize: 10 }
                Controls.ComboBox { font.pointSize: 10;
                    Layout.fillWidth: true; enabled: root.selectedCompiler >= 0
                    readonly property var compiler: root.controller.compilers[root.selectedCompiler]
                    model: compiler ? compiler.names : []
                    currentIndex: compiler ? compiler.selected : -1
                    onActivated: root.controller.setCompilerConfiguration(root.selectedCompiler, compiler.keys[currentIndex])
                }
            }
        }
        Item { visible: root.answersOnly; Layout.row: 22; Layout.column: 0; Layout.columnSpan: 3; Layout.fillHeight: true }
    }
}
