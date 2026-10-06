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
    property int page: 0
    property int selectedArgument: -1
    property var expanded: ({})
    width: 500; height: 378
    minimumWidth: 459; minimumHeight: 372
    title: qsTranslate("AddTestCasesWizard", "Add Test Cases")
    SystemPalette { id: colors }
    color: colors.base
    function begin() {
        page = 0
        score.text = String(controller.defaults.score)
        time.text = String(controller.defaults.time)
        memory.text = String(controller.defaults.memory)
        inputPattern.text = ""
        outputPattern.text = ""
        patternArguments.clear()
        selectedArgument = -1
        expanded = ({})
        open()
    }
    function next() {
        if (page === 0) {
            if (!score.acceptableInput) { score.forceActiveFocus(); return }
            if (!time.acceptableInput) { time.forceActiveFocus(); return }
            if (!memory.acceptableInput) { memory.forceActiveFocus(); return }
            page = 1
        } else if (page === 1) {
            let arguments = []
            for (let i = 0; i < patternArguments.count; ++i) arguments.push({expression: patternArguments.get(i).expression, group: patternArguments.get(i).group})
            if (controller.previewImport(inputPattern.text, outputPattern.text, arguments)) page = 2
        } else if (controller.importTestCases({score: score.text, time: time.text, memory: memory.text})) accept()
    }
    ListModel { id: patternArguments }
    Item {
        anchors.fill: parent
        StackLayout {
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: footer.top; anchors.margins: 11
            currentIndex: root.page
            ColumnLayout {
                spacing: 12
                Label { renderType: Text.NativeRendering; Layout.fillWidth: true; text: qsTranslate("AddTestCasesWizard", "Step 1/3: Input the full score, time limit and memory limit for each new test case."); wrapMode: Text.Wrap; font.pointSize: 14; font.bold: true }
                GridLayout {
                    Layout.fillWidth: true; columns: 2; rowSpacing: 10; columnSpacing: 12
                    Label { renderType: Text.NativeRendering; text: qsTranslate("AddTestCasesWizard", "Full Score"); font.pointSize: 10 }
                    RowLayout {
                        Layout.fillWidth: true
                        Controls.TextField { font.pointSize: 10; id: score; Layout.preferredWidth: 132; Layout.preferredHeight: 22; validator: IntValidator { bottom: 1; top: root.controller.defaults.maxScore } }
                        Item { Layout.fillWidth: true }
                    }
                    Label { renderType: Text.NativeRendering; enabled: root.controller.task.type !== 1; text: qsTranslate("AddTestCasesWizard", "Time Limit"); font.pointSize: 10 }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 10
                        Controls.TextField { font.pointSize: 10; id: time; enabled: root.controller.task.type !== 1; Layout.preferredWidth: 96; Layout.preferredHeight: 22; validator: IntValidator { bottom: 1; top: root.controller.defaults.maxTime } }
                        Label { renderType: Text.NativeRendering; enabled: root.controller.task.type !== 1; text: qsTranslate("AddTestCasesWizard", "ms"); font.pointSize: 10 }
                        Item { Layout.fillWidth: true }
                    }
                    Label { renderType: Text.NativeRendering; enabled: root.controller.task.type !== 1; text: qsTranslate("AddTestCasesWizard", "Memory Limit"); font.pointSize: 10 }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 10
                        Controls.TextField { font.pointSize: 10; id: memory; enabled: root.controller.task.type !== 1; Layout.preferredWidth: 96; Layout.preferredHeight: 22; validator: IntValidator { bottom: 1; top: root.controller.defaults.maxMemory } }
                        Label { renderType: Text.NativeRendering; enabled: root.controller.task.type !== 1; text: qsTranslate("AddTestCasesWizard", "MiB"); font.pointSize: 10 }
                        Item { Layout.fillWidth: true }
                    }
                }
                Item { Layout.fillHeight: true }
            }
            ColumnLayout {
                spacing: 10
                Label { renderType: Text.NativeRendering; Layout.fillWidth: true; text: qsTranslate("AddTestCasesWizard", "Step 2/3: Input patterns for input files and output files. Use argument <1>, <2>, etc. to represent a regular expression. Files will in the same subtask when their matched parts of checked expressions are identical."); wrapMode: Text.Wrap; textFormat: Text.PlainText; font.pointSize: 14; font.bold: true }
                GridLayout {
                    Layout.fillWidth: true; columns: 2; columnSpacing: 12; rowSpacing: 9
                    Label { renderType: Text.NativeRendering; text: qsTranslate("AddTestCasesWizard", "Input Files Pattern"); font.pointSize: 10 }
                    Controls.TextField { font.pointSize: 10; id: inputPattern; Layout.fillWidth: true }
                    Label { renderType: Text.NativeRendering; text: qsTranslate("AddTestCasesWizard", "Output Files Pattern"); font.pointSize: 10 }
                    Controls.TextField { font.pointSize: 10; id: outputPattern; Layout.fillWidth: true }
                }
                RowLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true; spacing: 10
                    Rectangle {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        color: colors.base; border.color: colors.mid
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 1; spacing: 0
                            Row {
                                Layout.fillWidth: true; Layout.preferredHeight: 22
                                StyleItem { width: 70; height: 22; kind: "header"; first: true; text: qsTranslate("AddTestCasesWizard", "Argument"); font.pointSize: 10 }
                                StyleItem { width: parent.width - 70; height: 22; kind: "header"; last: true; text: qsTranslate("AddTestCasesWizard", "Regular Expression"); font.pointSize: 10 }
                            }
                            ListView {
                                id: argumentsView
                                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                                model: patternArguments
                                ScrollBar.vertical: Controls.ScrollBar {}
                                delegate: Row {
                                    required property int index
                                    required property string expression
                                    required property bool group
                                    width: argumentsView.width; height: 27
                                    Controls.CheckBox { font.pointSize: 10; width: 70; height: 27; text: "<" + (index + 1) + ">"; checked: group; onClicked: { root.selectedArgument = index; patternArguments.setProperty(index, "group", checked) } }
                                    Controls.TextField { font.pointSize: 10; width: argumentsView.width - 70; height: 27; text: expression; onActiveFocusChanged: if (activeFocus) root.selectedArgument = index; onTextEdited: patternArguments.setProperty(index, "expression", text) }
                                }
                            }
                        }
                    }
                    ColumnLayout {
                        Layout.fillHeight: true; spacing: 10
                        Item { Layout.fillHeight: true }
                        Controls.ToolButton { font.pointSize: 10; Layout.preferredWidth: 27; Layout.preferredHeight: 27; icon.source: "image://lemon-icons/list-add?palette=" + StyleMetrics.paletteRevision; enabled: patternArguments.count < 9; onClicked: { patternArguments.append({expression: "", group: true}); root.selectedArgument = patternArguments.count - 1 } }
                        Controls.ToolButton { font.pointSize: 10; Layout.preferredWidth: 27; Layout.preferredHeight: 27; icon.source: "image://lemon-icons/list-remove?palette=" + StyleMetrics.paletteRevision; enabled: root.selectedArgument >= 0; onClicked: { patternArguments.remove(root.selectedArgument); root.selectedArgument = Math.min(root.selectedArgument, patternArguments.count - 1) } }
                        Item { Layout.fillHeight: true }
                    }
                }
            }
            ColumnLayout {
                spacing: 10
                Label { renderType: Text.NativeRendering; Layout.fillWidth: true; text: qsTranslate("AddTestCasesWizard", "Step 3/3: Preview the result and finish the wizard."); wrapMode: Text.Wrap; font.pointSize: 14; font.bold: true }
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    color: colors.base; border.color: colors.mid
                    ListView {
                        id: preview
                        anchors.fill: parent; anchors.margins: 1; clip: true
                        model: root.controller.importPreview
                        ScrollBar.vertical: Controls.ScrollBar {}
                        delegate: Column {
                            id: previewGroup
                            required property int index
                            required property var modelData
                            width: preview.width
                            Item {
                                width: preview.width; height: 22
                                StyleItem { x: 0; width: 20; height: parent.height; kind: "branch"; checked: root.expanded[previewGroup.index] ?? false }
                                Label { renderType: Text.NativeRendering; anchors.fill: parent; anchors.leftMargin: 22; text: qsTranslate("AddTestCasesWizard", "Test Case #%1").arg(previewGroup.index + 1); verticalAlignment: Text.AlignVCenter; font.pointSize: 10 }
                                MouseArea { anchors.fill: parent; onClicked: { let state = Object.assign({}, root.expanded); state[previewGroup.index] = !state[previewGroup.index]; root.expanded = state } }
                            }
                            Repeater {
                                model: root.expanded[previewGroup.index] ? previewGroup.modelData.files : []
                                delegate: Row {
                                    required property var modelData
                                    width: preview.width; height: 22
                                    Label { renderType: Text.NativeRendering; width: parent.width / 2; leftPadding: 22; text: modelData.input; elide: Text.ElideRight; font.pointSize: 10 }
                                    Label { renderType: Text.NativeRendering; width: parent.width / 2; text: modelData.output; elide: Text.ElideRight; font.pointSize: 10 }
                                }
                            }
                        }
                    }
                }
            }
        }
        Rectangle {
            id: footer
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 48; color: colors.window
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: colors.mid }
            RowLayout {
                anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 11; spacing: 6
                Controls.Button { font.pointSize: 9; text: qsTranslate("QWizard", "< &Back"); enabled: root.page > 0; onClicked: --root.page }
                Controls.Button { font.pointSize: 9; text: root.page === 2 ? qsTranslate("QWizard", "&Finish") : qsTranslate("QWizard", "&Next >"); onClicked: root.next() }
                Controls.Button { font.pointSize: 9; text: qsTranslate("QPlatformTheme", "Cancel"); onClicked: root.reject() }
            }
        }
    }
}
