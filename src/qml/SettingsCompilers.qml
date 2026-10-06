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

Item {
    id: root
    objectName: "CompilerSettings"
    required property var controller
    readonly property bool hasCompiler: controller.compilerIndex >= 0
    implicitWidth: 325
    implicitHeight: 325

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 9
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10
            Controls.Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ListView {
                    id: compilerList
                    objectName: "compilerList"
                    anchors.fill: parent
                    clip: true
                    model: root.controller.compilerNames
                    currentIndex: root.controller.compilerIndex
                    keyNavigationEnabled: true
                    onCurrentIndexChanged: {
                        if (currentIndex >= 0)
                            root.controller.compilerIndex = currentIndex
                    }
                    Keys.onDeletePressed: { if (root.hasCompiler) removeDialog.open() }
                    delegate: Rectangle {
                        required property int index
                        required property string modelData
                        width: compilerList.width
                        height: nameLabel.implicitHeight + 4
                        color: index === compilerList.currentIndex ? nameLabel.palette.highlight : nameLabel.palette.base
                        Label {
                            id: nameLabel
                            anchors.fill: parent
                            anchors.leftMargin: 3
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            font.pointSize: 10
                            color: index === compilerList.currentIndex ? palette.highlightedText : palette.text
                            text: modelData
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                compilerList.forceActiveFocus()
                                compilerList.currentIndex = index
                            }
                        }
                    }
                    ScrollBar.vertical: Controls.ScrollBar {}
                }
            }
            ColumnLayout {
                Layout.fillHeight: true
                spacing: 10
                Item { Layout.fillHeight: true }
                Controls.ToolButton {
                    Layout.preferredWidth: 27; Layout.preferredHeight: 27
                    icon.source: "image://lemon-icons/go-up?palette=" + StyleMetrics.paletteRevision
                    enabled: root.controller.compilerIndex > 0
                    onClicked: root.controller.moveCompiler(-1)
                }
                Controls.ToolButton {
                    Layout.preferredWidth: 27; Layout.preferredHeight: 27
                    icon.source: "image://lemon-icons/go-down?palette=" + StyleMetrics.paletteRevision
                    enabled: root.hasCompiler && root.controller.compilerIndex + 1 < root.controller.compilerNames.length
                    onClicked: root.controller.moveCompiler(1)
                }
                Item { Layout.fillHeight: true }
                Controls.ToolButton {
                    Layout.preferredWidth: 27; Layout.preferredHeight: 27
                    icon.source: "image://lemon-icons/list-add?palette=" + StyleMetrics.paletteRevision
                    onClicked: addWizard.open()
                }
                Controls.ToolButton {
                    Layout.preferredWidth: 27; Layout.preferredHeight: 27
                    icon.source: "image://lemon-icons/list-remove?palette=" + StyleMetrics.paletteRevision
                    enabled: root.hasCompiler
                    onClicked: removeDialog.open()
                }
                Item { Layout.fillHeight: true }
            }
        }
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 8
            rowSpacing: 8
            enabled: root.hasCompiler
            Label { text: qsTranslate("CompilerSettings", "Compiler Name") }
            Controls.TextField {
                Layout.fillWidth: true
                text: root.controller.compiler.name || ""
                selectByMouse: true
                onTextEdited: root.controller.setCompilerValue("name", text)
            }
            Label { text: qsTranslate("CompilerSettings", "Source Extensions") }
            Controls.TextField {
                Layout.fillWidth: true
                text: root.controller.compiler.sourceExtensions || ""
                validator: RegularExpressionValidator { regularExpression: /(\w+;)*\w+/ }
                selectByMouse: true
                onTextEdited: root.controller.setCompilerValue("sourceExtensions", text)
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Controls.Button {
                text: qsTranslate("CompilerSettings", "Advanced")
                enabled: root.hasCompiler
                onClicked: advanced.open()
            }
        }
    }
    AdvancedCompilerSettingsDialog { id: advanced; controller: root.controller }
    AddCompilerWizard { id: addWizard; controller: root.controller }
    Dialogs.MessageDialog {
        id: removeDialog
        title: qsTranslate("CompilerSettings", "LemonLime")
        text: qsTranslate("CompilerSettings", "Are you sure to delete compiler %1?").arg(root.controller.compiler.name || "")
        buttons: Dialogs.MessageDialog.Ok | Dialogs.MessageDialog.Cancel
        onAccepted: root.controller.deleteCompiler()
    }
}
