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
    width: 284; height: 193
    minimumWidth: 284; minimumHeight: 193
    title: qsTranslate("AddTaskDialog", "Add Task")
    property int selectedTask: 0
    function begin() {
        if (!controller.scanTasks()) return
        candidates.clear()
        for (let i = 0; i < controller.discoveredTasks.length; ++i) {
            const item = controller.discoveredTasks[i]
            candidates.append({title: item.title, score: String(item.score), time: String(item.time), memory: String(item.memory)})
        }
        selectedTask = 0
        open()
    }
    function save() {
        let values = []
        for (let i = 0; i < candidates.count; ++i) {
            const item = candidates.get(i)
            values.push({index: i, selected: true, score: item.score, time: item.time, memory: item.memory})
        }
        if (controller.importTasks(values)) accept()
    }
    ListModel { id: candidates }
    GridLayout {
        anchors.fill: parent; anchors.margins: 11
        columns: 4; columnSpacing: 9; rowSpacing: 10
        Label { renderType: Text.NativeRendering; text: qsTranslate("AddTaskDialog", "Task"); font.pointSize: 10 }
        Controls.ComboBox { font.pointSize: 10; Layout.columnSpan: 3; Layout.fillWidth: true; model: candidates; textRole: "title"; currentIndex: root.selectedTask; onActivated: root.selectedTask = currentIndex }
        Label { renderType: Text.NativeRendering; text: qsTranslate("AddTaskDialog", "Full Score"); font.pointSize: 10 }
        Controls.TextField { font.pointSize: 10; Layout.preferredWidth: 96; Layout.preferredHeight: 22; text: candidates.count ? candidates.get(root.selectedTask).score : ""; validator: IntValidator { bottom: 1; top: root.controller.defaults.maxScore * 100 } onTextEdited: candidates.setProperty(root.selectedTask, "score", text) }
        Item { Layout.columnSpan: 2; Layout.fillWidth: true }
        Label { renderType: Text.NativeRendering; text: qsTranslate("AddTaskDialog", "Time Limit"); font.pointSize: 10 }
        Controls.TextField { font.pointSize: 10; Layout.preferredWidth: 96; Layout.preferredHeight: 22; text: candidates.count ? candidates.get(root.selectedTask).time : ""; validator: IntValidator { bottom: 1; top: root.controller.defaults.maxTime } onTextEdited: candidates.setProperty(root.selectedTask, "time", text) }
        Label { renderType: Text.NativeRendering; text: qsTranslate("AddTaskDialog", "ms"); font.pointSize: 10 }
        Item { Layout.fillWidth: true }
        Label { renderType: Text.NativeRendering; text: qsTranslate("AddTaskDialog", "Memory Limit"); font.pointSize: 10 }
        Controls.TextField { font.pointSize: 10; Layout.preferredWidth: 96; Layout.preferredHeight: 22; text: candidates.count ? candidates.get(root.selectedTask).memory : ""; validator: IntValidator { bottom: 1; top: root.controller.defaults.maxMemory } onTextEdited: candidates.setProperty(root.selectedTask, "memory", text) }
        Label { renderType: Text.NativeRendering; text: qsTranslate("AddTaskDialog", "MiB"); font.pointSize: 10 }
        Item { Layout.fillWidth: true }
        RowLayout {
            Layout.columnSpan: 4; Layout.alignment: Qt.AlignRight
            Controls.Button { font.pointSize: 10; text: qsTranslate("QPlatformTheme", "OK"); enabled: !root.controller.busy && candidates.count > 0; onClicked: root.save() }
            Controls.Button { font.pointSize: 10; text: qsTranslate("QPlatformTheme", "Cancel"); onClicked: root.reject() }
        }
    }
}
