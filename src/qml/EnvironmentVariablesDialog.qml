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
import QtQuick.Dialogs as Dialogs

Controls.Dialog {
    id: root
    objectName: "EnvironmentVariablesDialog"
    font.pointSize: 9
    property var environment: []
    property var draft: []
    property int selectedIndex: -1
    signal environmentAccepted(var values)
    title: qsTranslate("EnvironmentVariablesDialog", "Extra Environment Variables")
    width: 298
    height: 196
    minimumWidth: 298
    minimumHeight: 196
    onOpened: {
        draft = JSON.parse(JSON.stringify(environment))
        selectedIndex = -1
    }
    onAccepted: environmentAccepted(draft)
    Shortcut { sequence: "Escape"; onActivated: root.reject() }
    Shortcut { sequence: "Return"; onActivated: root.accept() }
    Shortcut { sequence: "Enter"; onActivated: root.accept() }

    function edit(index) {
        variableDialog.entryIndex = index
        variableDialog.title = index < 0
            ? qsTranslate("EnvironmentVariablesDialog", "Add New Variable")
            : qsTranslate("EnvironmentVariablesDialog", "Edit Variable")
        variableDialog.variableName = index < 0 ? "" : draft[index].name
        variableDialog.variableValue = index < 0 ? "" : draft[index].value
        variableDialog.open()
    }

    Control {
        anchors.fill: parent
        font.pointSize: 10
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 11
            spacing: 8
            Controls.Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        StyleItem {
                            Layout.preferredWidth: 80
                            Layout.preferredHeight: 20
                            kind: "header"
                            text: qsTranslate("EnvironmentVariablesDialog", "Variable")
                            font: root.font
                            alignment: Qt.AlignLeft | Qt.AlignVCenter
                            first: true
                        }
                        StyleItem {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 20
                            kind: "header"
                            text: qsTranslate("EnvironmentVariablesDialog", "Value")
                            font: root.font
                            alignment: Qt.AlignLeft | Qt.AlignVCenter
                            last: true
                        }
                    }
                    ListView {
                        id: variables
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.draft
                        currentIndex: root.selectedIndex
                        keyNavigationEnabled: true
                        onCurrentIndexChanged: root.selectedIndex = currentIndex
                        delegate: Item {
                            required property int index
                            required property var modelData
                            width: variables.width
                            height: 27
                            Rectangle {
                                width: 80; height: parent.height
                                color: index === root.selectedIndex ? palette.highlight : palette.base
                                border.color: palette.mid
                                Label {
                                    anchors.fill: parent
                                    anchors.leftMargin: 3
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                    color: index === root.selectedIndex ? palette.highlightedText : palette.text
                                    text: modelData.name
                                }
                            }
                            Rectangle {
                                x: 80
                                width: variables.width - 80; height: parent.height
                                color: index === root.selectedIndex ? palette.highlight : palette.base
                                border.color: palette.mid
                                Label {
                                    anchors.fill: parent
                                    anchors.leftMargin: 3
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                    color: index === root.selectedIndex ? palette.highlightedText : palette.text
                                    text: modelData.value
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    variables.forceActiveFocus()
                                    root.selectedIndex = index
                                }
                            }
                        }
                        ScrollBar.vertical: Controls.ScrollBar {}
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Controls.Button {
                    text: qsTranslate("EnvironmentVariablesDialog", "&Add")
                    Layout.preferredWidth: 80
                    Layout.minimumWidth: 80
                    Layout.maximumWidth: 80
                    icon.source: "image://lemon-icons/list-add?palette=" + StyleMetrics.paletteRevision
                    onClicked: root.edit(-1)
                }
                Controls.Button {
                    text: qsTranslate("EnvironmentVariablesDialog", "&Edit")
                    Layout.preferredWidth: 80
                    Layout.minimumWidth: 80
                    Layout.maximumWidth: 80
                    icon.source: "image://lemon-icons/document-edit?palette=" + StyleMetrics.paletteRevision
                    enabled: root.selectedIndex >= 0
                    onClicked: root.edit(root.selectedIndex)
                }
                Controls.Button {
                    text: qsTranslate("EnvironmentVariablesDialog", "&Delete")
                    Layout.preferredWidth: 80
                    Layout.minimumWidth: 80
                    Layout.maximumWidth: 80
                    icon.source: "image://lemon-icons/edit-delete?palette=" + StyleMetrics.paletteRevision
                    enabled: root.selectedIndex >= 0
                    onClicked: deleteDialog.open()
                }
                Item { Layout.fillWidth: true }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Item { Layout.fillWidth: true }
                Controls.Button {
                    Layout.preferredWidth: 80
                    text: qsTranslate("QPlatformTheme", "OK")
                    highlighted: true
                    onClicked: root.accept()
                }
                Controls.Button {
                    Layout.preferredWidth: 80
                    text: qsTranslate("QPlatformTheme", "Cancel")
                    onClicked: root.reject()
                }
            }
        }
    }
    SystemPalette { id: palette }
    EditVariableDialog {
        id: variableDialog
        property int entryIndex: -1
        onAccepted: {
            const changed = JSON.parse(JSON.stringify(root.draft))
            const entry = {name: variableName, value: variableValue}
            if (entryIndex < 0)
                changed.push(entry)
            else
                changed[entryIndex] = entry
            root.draft = changed
        }
    }
    Dialogs.MessageDialog {
        id: deleteDialog
        title: qsTranslate("EnvironmentVariablesDialog", "LemonLime")
        text: qsTranslate("EnvironmentVariablesDialog", "Are you sure to delete variable %1?").arg(root.selectedIndex >= 0 ? root.draft[root.selectedIndex].name : "")
        buttons: Dialogs.MessageDialog.Ok | Dialogs.MessageDialog.Cancel
        onAccepted: {
            const changed = JSON.parse(JSON.stringify(root.draft))
            const selection = Math.min(root.selectedIndex, changed.length - 2)
            changed.splice(root.selectedIndex, 1)
            root.selectedIndex = -1
            root.draft = changed
            root.selectedIndex = selection
        }
    }
}
