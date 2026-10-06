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
    property bool showingSubtask: false
    property var expanded: ({})
    property int newTaskCount: 0
    property int renameTask: -1
    property int observedTask: -1
    SystemPalette { id: colors }
    Connections {
        target: root.controller
        function onChanged() {
            if (root.observedTask !== root.controller.selectedTask || root.controller.selectedTask < 0 || root.controller.selectedSubtask < 0)
                root.showingSubtask = false
            root.observedTask = root.controller.selectedTask
        }
    }

    function addTasks() { discovery.begin() }
    function addNewTask() {
        if (controller.addTask(qsTranslate("SummaryTree", "Problem %1").arg(++newTaskCount))) {
            showingSubtask = false
            renameTask = controller.selectedTask
        }
    }
    function addTestCase() {
        if (controller.addEmptySubtask()) {
            let state = Object.assign({}, expanded)
            state[controller.selectedTask] = true
            expanded = state
            showingSubtask = true
        }
    }
    function moveSelection(offset) {
        if (controller.moveTask(offset)) expanded = ({})
    }
    function selectTask(index) {
        controller.selectTask(index)
        showingSubtask = false
        renameTask = -1
    }
    function selectCase(task, subtask) {
        controller.selectTask(task)
        controller.selectSubtask(subtask)
        showingSubtask = true
        renameTask = -1
    }
    RowLayout {
        anchors.fill: parent
        anchors.margins: 11
        spacing: 6
        Controls.GroupBox {
            title: qsTranslate("LemonLime", "Summary")
            font.pointSize: 10
            font.bold: true
            leftPadding: 14; rightPadding: 14; topPadding: 34; bottomPadding: 14
            Layout.preferredWidth: Math.max(176, (root.width - 28) / 4)
            Layout.minimumWidth: 176
            Layout.fillHeight: true
            enabled: root.controller.available && !root.controller.busy
            ColumnLayout {
                anchors.fill: parent
                spacing: 6
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    color: colors.base; border.color: colors.mid
                    ListView {
                        id: summary
                        objectName: "summaryTree"
                        anchors.fill: parent; anchors.margins: 1
                        focus: true; clip: true
                        model: root.controller.tasks
                        ScrollBar.vertical: Controls.ScrollBar {}
                        Keys.onPressed: event => {
                            if (event.key === Qt.Key_Insert) {
                                if (event.modifiers & Qt.ControlModifier) root.addNewTask()
                                else if (root.controller.selectedTask >= 0) root.addTestCase()
                                event.accepted = true
                            } else if (event.key === Qt.Key_Delete && root.controller.selectedTask >= 0) {
                                if (root.showingSubtask) root.controller.removeSubtasks([root.controller.selectedSubtask])
                                else removeTask.open()
                                event.accepted = true
                            } else if (event.key === Qt.Key_F2 && !root.showingSubtask) {
                                root.renameTask = root.controller.selectedTask
                                event.accepted = true
                            }
                        }
                        delegate: Column {
                            id: taskBranch
                            required property int index
                            required property var modelData
                            width: summary.width
                            Item {
                                width: parent.width; height: 18
                                Rectangle { anchors.fill: parent; visible: !root.showingSubtask && root.controller.selectedTask === taskBranch.index; color: colors.highlight }
                                StyleItem { x: 0; width: 20; height: parent.height; kind: "branch"; checked: root.expanded[taskBranch.index] ?? false; visible: taskBranch.modelData.count > 0 }
                                Label { renderType: Text.NativeRendering; x: 22; width: parent.width - 24; height: parent.height; verticalAlignment: Text.AlignVCenter; font.pointSize: 10; font.bold: false; text: taskBranch.modelData.title; elide: Text.ElideRight; color: !root.showingSubtask && root.controller.selectedTask === taskBranch.index ? colors.highlightedText : colors.text }
                                MouseArea {
                                    anchors.fill: parent
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    onClicked: mouse => {
                                        summary.forceActiveFocus()
                                        root.selectTask(taskBranch.index)
                                        if (mouse.x < 20 && mouse.button === Qt.LeftButton) {
                                            let state = Object.assign({}, root.expanded)
                                            state[taskBranch.index] = !state[taskBranch.index]
                                            root.expanded = state
                                        }
                                        if (mouse.button === Qt.RightButton) treeMenu.popup()
                                    }
                                    onDoubleClicked: mouse => {
                                        if (mouse.x >= 20) root.renameTask = taskBranch.index
                                    }
                                }
                                Controls.TextField {
                                    font.pointSize: 10; font.bold: false
                                    x: 20; width: parent.width - 20; height: parent.height
                                    visible: root.renameTask === taskBranch.index
                                    text: taskBranch.modelData.title
                                    onVisibleChanged: if (visible) { forceActiveFocus(); selectAll() }
                                    onEditingFinished: {
                                        if (visible && text.length) root.controller.updateTask({title: text})
                                        root.renameTask = -1
                                    }
                                }
                            }
                            Repeater {
                                model: root.expanded[taskBranch.index] ? taskBranch.modelData.count : 0
                                delegate: Rectangle {
                                    required property int index
                                    width: taskBranch.width; height: 18
                                    readonly property bool selected: root.showingSubtask && root.controller.selectedTask === taskBranch.index && root.controller.selectedSubtask === index
                                    color: selected ? colors.highlight : colors.base
                                    Label { renderType: Text.NativeRendering; x: 42; width: parent.width - 44; height: parent.height; verticalAlignment: Text.AlignVCenter; font.pointSize: 10; font.bold: false; text: qsTranslate("SummaryTree", "Test Case #%1").arg(index + 1); color: parent.selected ? colors.highlightedText : colors.text; elide: Text.ElideRight }
                                    MouseArea {
                                        anchors.fill: parent; acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        onClicked: mouse => { summary.forceActiveFocus(); root.selectCase(taskBranch.index, index); if (mouse.button === Qt.RightButton) treeMenu.popup() }
                                    }
                                }
                            }
                        }
                        MouseArea {
                            anchors.fill: parent; z: -1
                            acceptedButtons: Qt.RightButton
                            onClicked: treeMenu.popup()
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    Controls.ToolButton {
                        icon.source: "image://lemon-icons/go-down?palette=" + StyleMetrics.paletteRevision
                        text: qsTranslate("LemonLime", "Down")
                        display: AbstractButton.IconOnly
                        enabled: !root.showingSubtask && root.controller.selectedTask >= 0 && root.controller.selectedTask + 1 < root.controller.tasks.length
                        onClicked: root.moveSelection(1)
                    }
                    Controls.ToolButton {
                        icon.source: "image://lemon-icons/go-up?palette=" + StyleMetrics.paletteRevision
                        text: qsTranslate("LemonLime", "Up")
                        display: AbstractButton.IconOnly
                        enabled: !root.showingSubtask && root.controller.selectedTask > 0
                        onClicked: root.moveSelection(-1)
                    }
                }
            }
        }
        Controls.GroupBox {
            title: qsTranslate("LemonLime", "Detail")
            font.pointSize: 10
            font.bold: false
            leftPadding: 14; rightPadding: 14; topPadding: 34; bottomPadding: 14
            Layout.fillWidth: true; Layout.fillHeight: true
            enabled: !root.controller.busy
            Item {
                anchors.fill: parent
                TaskSettingsForm { anchors.fill: parent; visible: root.controller.selectedTask >= 0 && !root.showingSubtask; controller: root.controller }
                TestCaseEditor { anchors.fill: parent; visible: root.controller.selectedTask >= 0 && root.showingSubtask; controller: root.controller }
            }
        }
    }
    Menu {
        id: treeMenu
        MenuItem { text: qsTranslate("SummaryTree", "Add a New Task"); onTriggered: root.addNewTask() }
        MenuItem { text: qsTranslate("SummaryTree", "Delete Current Task"); visible: root.controller.selectedTask >= 0; height: visible ? implicitHeight : 0; onTriggered: removeTask.open() }
        MenuSeparator { visible: root.controller.selectedTask >= 0; height: visible ? implicitHeight : 0 }
        MenuItem { text: qsTranslate("SummaryTree", "Add a Test Case"); visible: root.controller.selectedTask >= 0; height: visible ? implicitHeight : 0; onTriggered: root.addTestCase() }
        MenuItem { text: qsTranslate("SummaryTree", "Add Test Cases ..."); visible: root.controller.selectedTask >= 0; height: visible ? implicitHeight : 0; onTriggered: testImport.begin() }
        MenuItem { text: qsTranslate("SummaryTree", "Delete Current Test Case"); visible: root.showingSubtask; height: visible ? implicitHeight : 0; onTriggered: root.controller.removeSubtasks([root.controller.selectedSubtask]) }
        MenuSeparator { visible: root.controller.selectedTask >= 0 && !root.showingSubtask; height: visible ? implicitHeight : 0 }
        MenuItem { text: qsTranslate("SummaryTree", "Advanced Test Case Modifier"); visible: root.controller.selectedTask >= 0 && !root.showingSubtask; height: visible ? implicitHeight : 0; onTriggered: modifier.begin() }
    }
    Controls.Dialog {
        id: removeTask
        title: qsTranslate("SummaryTree", "LemonLime")
        width: 340; height: 130
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 11
            Label { renderType: Text.NativeRendering; Layout.fillWidth: true; Layout.fillHeight: true; text: qsTranslate("SummaryTree", "Are you sure to delete this task?"); wrapMode: Text.Wrap }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Controls.Button { text: qsTranslate("QPlatformTheme", "&Yes"); onClicked: { root.controller.removeTask(); root.showingSubtask = false; removeTask.accept() } }
                Controls.Button { text: qsTranslate("QPlatformTheme", "Cancel"); onClicked: removeTask.reject() }
            }
        }
    }
    TestCaseImportDialog {
        id: testImport
        controller: root.controller
        onAccepted: {
            let state = Object.assign({}, root.expanded)
            state[root.controller.selectedTask] = true
            root.expanded = state
            root.showingSubtask = root.controller.selectedSubtask >= 0
        }
    }
    TaskDiscoveryDialog { id: discovery; controller: root.controller }
    TestCaseModifierDialog { id: modifier; controller: root.controller }
}
