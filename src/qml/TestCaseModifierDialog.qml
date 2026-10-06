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
    readonly property var cases: controller.subtasks
    property int selectionStart: -1
    property int selectionEnd: -1
    property int anchorRow: -1
    readonly property var rowOffsets: buildRowOffsets()
    readonly property int totalRows: rowOffsets.length ? rowOffsets[rowOffsets.length - 1] + rowCount(rowOffsets.length - 1) : 0
    readonly property var selection: selectionRanges()
    readonly property var selectedSubtasks: selection.whole
    readonly property var partialSelections: selection.partial
    readonly property bool pairSelection: partialSelections.length > 0
    readonly property bool hasSelection: selection.all.length > 0
    readonly property bool singlePartial: selection.all.length === 1 && partialSelections.length === 1
    readonly property int firstSelected: hasSelection ? selection.all[0] : -1
    readonly property int lastSelected: hasSelection ? selection.all[selection.all.length - 1] : -1
    readonly property int selectedPairSubtask: singlePartial ? partialSelections[0].subtask : -1
    readonly property var selectedPairs: singlePartial ? pairIndices(partialSelections[0]) : []
    readonly property bool canModify: hasSelection && (!pairSelection || (singlePartial && selectedPairs.length === 1))
    readonly property bool canAddCase: selection.all.length === 1 && !pairSelection
    readonly property bool canMoveUp: hasSelection && (pairSelection ? singlePartial && selectedPairs[0] > 0 : firstSelected > 0)
    readonly property bool canMoveDown: hasSelection && (pairSelection ? singlePartial && selectedPairs[selectedPairs.length - 1] + 1 < cases[selectedPairSubtask].count : lastSelected + 1 < cases.length)
    readonly property bool canMerge: !pairSelection && selectedSubtasks.length > 1
    readonly property bool canSplit: !pairSelection && selectedSubtasks.length > 0
    SystemPalette { id: colors }
    title: qsTranslate("ExtTestCaseModifierDialog", "Advanced Test Case Modifier")
    width: 800; height: 512; minimumWidth: 800; minimumHeight: 400
    function begin() {
        if (!controller.beginTaskEdit()) return
        clearSelection()
        open()
    }
    function rowCount(subtask) { return Math.max(1, cases[subtask].count) }
    function buildRowOffsets() {
        let offsets = []
        let row = 0
        for (let i = 0; i < cases.length; ++i) {
            offsets.push(row)
            row += rowCount(i)
        }
        return offsets
    }
    function selectionRanges() {
        let result = {whole: [], partial: [], all: []}
        if (selectionStart < 0 || selectionEnd < selectionStart) return result
        for (let i = 0; i < rowOffsets.length; ++i) {
            const first = rowOffsets[i]
            const last = first + rowCount(i) - 1
            if (last < selectionStart || first > selectionEnd) continue
            result.all.push(i)
            if (selectionStart <= first && last <= selectionEnd) result.whole.push(i)
            else result.partial.push({subtask: i, first: Math.max(0, selectionStart - first), last: Math.min(last - first, selectionEnd - first)})
        }
        return result
    }
    function pairIndices(partial) {
        let indices = []
        for (let i = partial.first; i <= partial.last; ++i) indices.push(i)
        return indices
    }
    function clearSelection() { selectionStart = -1; selectionEnd = -1; anchorRow = -1 }
    function selectRows(first, last, modifiers) {
        if (!totalRows) { clearSelection(); return }
        first = Math.max(0, Math.min(first, totalRows - 1))
        last = Math.max(first, Math.min(last, totalRows - 1))
        if ((modifiers & Qt.ShiftModifier) && anchorRow >= 0) {
            selectionStart = Math.min(anchorRow, first)
            selectionEnd = Math.max(anchorRow, last)
        } else if ((modifiers & Qt.ControlModifier) && hasSelection) {
            selectionStart = Math.min(selectionStart, first)
            selectionEnd = Math.max(selectionEnd, last)
        } else {
            selectionStart = first
            selectionEnd = last
            anchorRow = first
        }
    }
    function selectSubtask(index, modifiers) {
        selectRows(rowOffsets[index], rowOffsets[index] + rowCount(index) - 1, modifiers)
        table.forceActiveFocus()
    }
    function selectPair(subtask, pair, modifiers) {
        const row = rowOffsets[subtask] + pair
        selectRows(row, row, modifiers)
        table.forceActiveFocus()
    }
    function dragSelection(area, x, y) {
        const point = area.mapToItem(table, x, y)
        const row = Math.floor((point.y + table.contentY) / 30)
        selectRows(row, row, Qt.ShiftModifier)
        if (point.y < 0) table.contentY = Math.max(0, table.contentY - 30)
        else if (point.y > table.height) table.contentY = Math.min(Math.max(0, table.contentHeight - table.height), table.contentY + 30)
    }
    function pairSelected(subtask, pair) {
        const row = rowOffsets[subtask] + pair
        return selectionStart >= 0 && selectionStart <= row && row <= selectionEnd
    }
    function moveSelected(offset) {
        if (offset < 0 ? !canMoveUp : !canMoveDown) return
        const first = selectionStart
        const last = selectionEnd
        if (singlePartial) {
            controller.selectSubtask(selectedPairSubtask)
            if (controller.moveFilePairs(selectedPairs, offset)) selectRows(first + offset, last + offset, Qt.NoModifier)
        } else {
            const delta = offset < 0 ? -rowCount(firstSelected - 1) : rowCount(lastSelected + 1)
            if (controller.moveSubtasks(selectedSubtasks, offset)) selectRows(first + delta, last + delta, Qt.NoModifier)
        }
    }
    function removeSelected() {
        if (hasSelection && controller.removeTestCaseRows(selectionStart, selectionEnd)) clearSelection()
    }
    function mergeSelected() {
        if (!canMerge) return
        const first = selectionStart
        const last = selectionEnd
        if (controller.mergeSubtasks(selectedSubtasks)) selectRows(first, last, Qt.NoModifier)
    }
    function splitSelected() {
        if (!canSplit) return
        const first = selectionStart
        const last = selectionEnd
        if (controller.splitSubtasks(selectedSubtasks)) selectRows(first, last, Qt.NoModifier)
    }
    onAccepted: controller.finishTaskEdit(true)
    onRejected: controller.finishTaskEdit(false)
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 11; spacing: 6
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.margins: 9; spacing: 6
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 674; Layout.minimumWidth: 200
                color: colors.base; border.color: colors.mid
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 1; spacing: 0
                    Row {
                        Layout.fillWidth: true; Layout.preferredHeight: 25
                        Repeater {
                            model: [qsTranslate("ExtTestCaseTable", "Informations"), qsTranslate("ExtTestCaseTable", "Input"), qsTranslate("ExtTestCaseTable", "Output")]
                            delegate: StyleItem {
                                required property int index
                                required property string modelData
                                width: parent.width / 3; height: 25
                                kind: "header"; text: modelData; font.pointSize: 10
                                first: index === 0; last: index === 2
                            }
                        }
                    }
                    ListView {
                        id: table
                        objectName: "testCaseTable"
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                        model: root.cases
                        ScrollBar.vertical: Controls.ScrollBar {}
                        delegate: Item {
                            id: subtaskRow
                            required property int index
                            required property var modelData
                            width: table.width; height: Math.max(1, modelData.files.length) * 30
                            readonly property bool selected: root.selectedSubtasks.indexOf(index) >= 0
                            Rectangle {
                                width: table.width / 3; height: parent.height
                                color: subtaskRow.selected ? colors.highlight : colors.base
                                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: colors.mid }
                                Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: colors.mid }
                                Label { renderType: Text.NativeRendering;
                                    anchors.fill: parent; anchors.margins: 3
                                    text: root.controller.task.type === 1 ? qsTranslate("ExtTestCaseTable", "#%1 :: %2 pt, %3").arg(subtaskRow.index + 1).arg(subtaskRow.modelData.score).arg(subtaskRow.modelData.dependencies ? "(" + subtaskRow.modelData.dependencies.split(",").length + ")" : "(-)") : qsTranslate("ExtTestCaseTable", "#%1 :: %2 pt, TL %3 ms, ML %4 MiB, %5").arg(subtaskRow.index + 1).arg(subtaskRow.modelData.score).arg(subtaskRow.modelData.time).arg(subtaskRow.modelData.memory).arg(subtaskRow.modelData.dependencies ? "(" + subtaskRow.modelData.dependencies.split(",").length + ")" : "(-)")
                                    font.pointSize: 10; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                    color: subtaskRow.selected ? colors.highlightedText : colors.text
                                }
                                MouseArea {
                                    id: subtaskSelectionArea
                                    property real pressX: 0
                                    property real pressY: 0
                                    property bool draggingSelection: false
                                    anchors.fill: parent; preventStealing: true
                                    onPressed: mouse => { pressX = mouse.x; pressY = mouse.y; draggingSelection = false; root.selectSubtask(subtaskRow.index, mouse.modifiers) }
                                    onPositionChanged: mouse => {
                                        if (pressed && (draggingSelection || Math.abs(mouse.x - pressX) + Math.abs(mouse.y - pressY) > 5)) {
                                            draggingSelection = true
                                            root.dragSelection(subtaskSelectionArea, mouse.x, mouse.y)
                                        }
                                    }
                                }
                            }
                            Column {
                                x: table.width / 3; width: table.width * 2 / 3
                                Repeater {
                                    model: subtaskRow.modelData.files.length ? subtaskRow.modelData.files : [{input: "", output: ""}]
                                    delegate: Row {
                                        id: pairRow
                                        required property int index
                                        required property var modelData
                                        width: table.width * 2 / 3; height: 30
                                        Repeater {
                                            model: [pairRow.modelData.input, pairRow.modelData.output]
                                            delegate: Rectangle {
                                                required property string modelData
                                                width: table.width / 3; height: 30
                                                readonly property bool selected: root.pairSelected(subtaskRow.index, pairRow.index)
                                                color: selected ? colors.highlight : colors.base
                                                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: colors.mid }
                                                Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: colors.mid }
                                                Label { renderType: Text.NativeRendering; anchors.fill: parent; anchors.margins: 3; text: modelData; font.pointSize: 10; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: parent.selected ? colors.highlightedText : colors.text }
                                                MouseArea {
                                                    id: fileSelectionArea
                                                    property real pressX: 0
                                                    property real pressY: 0
                                                    property bool draggingSelection: false
                                                    anchors.fill: parent; preventStealing: true
                                                    onPressed: mouse => { pressX = mouse.x; pressY = mouse.y; draggingSelection = false; if (subtaskRow.modelData.files.length) root.selectPair(subtaskRow.index, pairRow.index, mouse.modifiers); else root.selectSubtask(subtaskRow.index, mouse.modifiers) }
                                                    onPositionChanged: mouse => {
                                                        if (pressed && (draggingSelection || Math.abs(mouse.x - pressX) + Math.abs(mouse.y - pressY) > 5)) {
                                                            draggingSelection = true
                                                            root.dragSelection(fileSelectionArea, mouse.x, mouse.y)
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            ColumnLayout {
                Layout.fillHeight: true; Layout.preferredWidth: 80; Layout.minimumWidth: 80; Layout.maximumWidth: 80; spacing: 6
                Controls.Button { font.pointSize: 10; Layout.fillWidth: true; text: qsTranslate("ExtTestCaseModifier", "New Subtask"); onClicked: updater.begin(true, []) }
                Controls.Button { font.pointSize: 10; Layout.fillWidth: true; text: qsTranslate("ExtTestCaseModifier", "New Case"); enabled: root.canAddCase; onClicked: updater.beginPair(root.lastSelected, -1) }
                Item { Layout.fillHeight: true }
                Controls.Button { font.pointSize: 10; Layout.fillWidth: true; text: qsTranslate("ExtTestCaseModifier", "Modify"); enabled: root.canModify; onClicked: { if (root.pairSelection) updater.beginPair(root.selectedPairSubtask, root.selectedPairs[0]); else updater.begin(false, root.selectedSubtasks) } }
                Controls.Button { font.pointSize: 10; Layout.fillWidth: true; text: qsTranslate("ExtTestCaseModifier", "Move Up"); enabled: root.canMoveUp; onClicked: root.moveSelected(-1) }
                Controls.Button { font.pointSize: 10; Layout.fillWidth: true; text: qsTranslate("ExtTestCaseModifier", "Move Down"); enabled: root.canMoveDown; onClicked: root.moveSelected(1) }
                Item { Layout.fillHeight: true }
                Controls.Button { font.pointSize: 10; Layout.fillWidth: true; text: qsTranslate("ExtTestCaseModifier", "Merge"); enabled: root.canMerge; onClicked: root.mergeSelected() }
                Controls.Button { font.pointSize: 10; Layout.fillWidth: true; text: qsTranslate("ExtTestCaseModifier", "Split"); enabled: root.canSplit; onClicked: root.splitSelected() }
                Item { Layout.fillHeight: true }
                Controls.Button { font.pointSize: 10; Layout.fillWidth: true; text: qsTranslate("ExtTestCaseModifier", "Remove"); enabled: root.hasSelection; onClicked: root.removeSelected() }
                Item { Layout.fillHeight: true }
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            Controls.Button { font.pointSize: 10; text: qsTranslate("QPlatformTheme", "OK"); onClicked: root.accept() }
            Controls.Button { font.pointSize: 10; text: qsTranslate("QPlatformTheme", "Cancel"); onClicked: root.reject() }
        }
    }
    TestCaseBatchDialog { id: updater; transientParent: root; controller: root.controller; onAccepted: root.clearSelection() }
}
