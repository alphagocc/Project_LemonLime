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
    readonly property var caseData: controller.subtask
    property var selectedPairs: []
    property int selectionAnchor: -1
    property int observedTask: -1
    property int observedSubtask: -1
    readonly property int rowHeaderWidth: Math.max(12, StyleMetrics.textWidth(String((caseData.files ?? []).length), Qt.font({family: StyleMetrics.applicationFont.family, pointSize: 10})) + 5)
    SystemPalette { id: colors }
    Connections {
        target: root.controller
        function onChanged() {
            if (root.observedTask !== root.controller.selectedTask || root.observedSubtask !== root.controller.selectedSubtask) {
                root.observedTask = root.controller.selectedTask
                root.observedSubtask = root.controller.selectedSubtask
                root.selectedPairs = []
                root.selectionAnchor = -1
            }
        }
    }
    function update(key, value) {
        if (String(caseData[key]) === String(value)) return
        let change = {}
        change[key] = value
        controller.updateSubtasks([controller.selectedSubtask], change)
    }
    function selectPair(index, modifiers) {
        if (modifiers & Qt.ShiftModifier && selectionAnchor >= 0) {
            let selected = []
            for (let i = Math.min(selectionAnchor, index); i <= Math.max(selectionAnchor, index); ++i) selected.push(i)
            selectedPairs = selected
        } else { selectedPairs = [index]; selectionAnchor = index }
    }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 9; spacing: 10
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            color: colors.base; border.color: colors.mid
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 1; spacing: 0
                Row {
                    Layout.fillWidth: true; Layout.preferredHeight: 22
                    Repeater {
                        model: ["", qsTranslate("TestCaseEditWidget", "Input Files"), qsTranslate("TestCaseEditWidget", "Output Files")]
                        delegate: StyleItem {
                            required property int index
                            required property string modelData
                            width: index === 0 ? root.rowHeaderWidth : (parent.width - root.rowHeaderWidth) / 2; height: 22
                            kind: "header"; text: modelData; font.pointSize: 10
                            first: index === 0; last: index === 2
                        }
                    }
                }
                ListView {
                    id: files
                    Layout.fillWidth: true; Layout.fillHeight: true
                    model: root.caseData.files ?? []; clip: true
                    Keys.onDeletePressed: { root.controller.removeFilePairs(root.selectedPairs); root.selectedPairs = [] }
                    ScrollBar.vertical: Controls.ScrollBar {}
                    delegate: Item {
                        id: row
                        required property int index
                        required property var modelData
                        width: files.width; height: 27
                        Row {
                            anchors.fill: parent
                            StyleItem { width: root.rowHeaderWidth; height: 27; kind: "header"; vertical: true; text: String(row.index + 1); font.pointSize: 10; first: row.index === 0; last: row.index + 1 === (root.caseData.files ?? []).length }
                            Repeater {
                                model: [row.modelData.input, row.modelData.output]
                                delegate: Rectangle {
                                    id: cell
                                    required property int index
                                    required property string modelData
                                    width: (files.width - root.rowHeaderWidth) / 2; height: 27
                                    color: root.selectedPairs.indexOf(row.index) >= 0 ? colors.highlight : colors.base
                                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: colors.mid }
                                    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: colors.mid }
                                    Label { renderType: Text.NativeRendering; anchors.fill: parent; anchors.margins: 3; verticalAlignment: Text.AlignVCenter; text: cell.modelData; font.pointSize: 10; elide: Text.ElideRight; color: root.selectedPairs.indexOf(row.index) >= 0 ? colors.highlightedText : colors.text }
                                    MouseArea {
                                        anchors.fill: parent; acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        onClicked: mouse => { files.forceActiveFocus(); root.selectPair(row.index, mouse.modifiers) }
                                        onDoubleClicked: { editor.visible = true; editor.forceActiveFocus(); editor.selectAll() }
                                    }
                                    Controls.TextField { font.pointSize: 10;
                                        id: editor
                                        anchors.fill: parent; visible: false; text: cell.modelData
                                        onEditingFinished: {
                                            if (visible) root.controller.setFilePair(row.index, cell.index === 0 ? text : row.modelData.input, cell.index === 1 ? text : row.modelData.output)
                                            visible = false
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        GridLayout {
            Layout.fillWidth: true; columns: 3; columnSpacing: 10; rowSpacing: 10
            Label { renderType: Text.NativeRendering; text: qsTranslate("TestCaseEditWidget", "Input File Name"); font.pointSize: 10 }
            TaskFileField { id: input; Layout.fillWidth: true; controller: root.controller }
            Item { Layout.preferredWidth: 60 }
            Label { renderType: Text.NativeRendering; text: qsTranslate("TestCaseEditWidget", "Output File Name"); font.pointSize: 10 }
            TaskFileField { id: output; Layout.fillWidth: true; controller: root.controller }
            Controls.Button { font.pointSize: 10; Layout.preferredWidth: 60; icon.source: "image://lemon-icons/list-add?palette=" + StyleMetrics.paletteRevision; text: qsTranslate("TestCaseEditWidget", "Add"); onClicked: { if (root.controller.setFilePair(-1, input.text, output.text)) { input.text = ""; output.text = "" } } }
            Label { renderType: Text.NativeRendering; text: qsTranslate("TestCaseEditWidget", "Subtask Dependence"); font.pointSize: 10 }
            Controls.TextField { font.pointSize: 10; Layout.fillWidth: true; text: root.caseData.dependencies ?? ""; placeholderText: qsTranslate("TestCaseEditWidget", "seperate by \",\""); onEditingFinished: root.update("dependencies", text) }
            Controls.Button { font.pointSize: 10; Layout.preferredWidth: 60; icon.source: "image://lemon-icons/edit-clear?palette=" + StyleMetrics.paletteRevision; text: qsTranslate("TestCaseEditWidget", "Clear"); onClicked: root.update("dependencies", "") }
        }
        GridLayout {
            Layout.fillWidth: true; columns: 3; columnSpacing: 6; rowSpacing: 6
            Label { renderType: Text.NativeRendering; text: qsTranslate("TestCaseEditWidget", "Full Score"); font.pointSize: 10 }
            Controls.TextField { font.pointSize: 10; Layout.preferredWidth: 96; Layout.preferredHeight: 22; text: String(root.caseData.score ?? 0); validator: IntValidator { bottom: 1; top: root.controller.defaults.maxScore } onTextEdited: if (acceptableInput) root.update("score", text) }
            Item { Layout.fillWidth: true }
            Label { renderType: Text.NativeRendering; enabled: root.controller.task.type !== 1; text: qsTranslate("TestCaseEditWidget", "Time Limit"); font.pointSize: 10 }
            Controls.TextField { font.pointSize: 10; enabled: root.controller.task.type !== 1; Layout.preferredWidth: 96; Layout.preferredHeight: 22; text: String(root.caseData.time ?? 0); validator: IntValidator { bottom: 1; top: root.controller.defaults.maxTime } onTextEdited: if (acceptableInput) root.update("time", text) }
            Label { renderType: Text.NativeRendering; enabled: root.controller.task.type !== 1; text: qsTranslate("TestCaseEditWidget", "ms"); Layout.fillWidth: true; font.pointSize: 10 }
            Label { renderType: Text.NativeRendering; enabled: root.controller.task.type !== 1; text: qsTranslate("TestCaseEditWidget", "Memory Limit"); font.pointSize: 10 }
            Controls.TextField { font.pointSize: 10; enabled: root.controller.task.type !== 1; Layout.preferredWidth: 96; Layout.preferredHeight: 22; text: String(root.caseData.memory ?? 0); validator: IntValidator { bottom: 1; top: root.controller.defaults.maxMemory } onTextEdited: if (acceptableInput) root.update("memory", text) }
            Label { renderType: Text.NativeRendering; enabled: root.controller.task.type !== 1; text: qsTranslate("TestCaseEditWidget", "MiB"); Layout.fillWidth: true; font.pointSize: 10 }
        }
    }
}
