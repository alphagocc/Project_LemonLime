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
    objectName: "AddCompilerWizard"
    font.pointSize: 10
    required property var controller
    property int pageIndex: 0
    property bool custom: false
    property var selected: [false, true, false, false, false, false]
    property var presets: []
    property var customDraft: ({})
    property var recommended: [true, true, true, false, false, false]
    property string javaMemory: "1024"
    property string summary: ""
    property string validationError: ""
    readonly property var presetNames: ["gcc", "g++", "fpc", "fbc", "Java", "Python"]
    title: qsTranslate("AddCompilerWizard", "Add Compilers Wizard")
    width: 542
    height: 555
    minimumWidth: 470
    minimumHeight: 450
    onOpened: {
        pageIndex = 0
        custom = false
        selected = [false, true, false, false, false, false]
        recommended = [true, true, true, false, false, false]
        const initialPresets = presetNames.map(name => JSON.parse(JSON.stringify(controller.compilerPreset(name))))
        initialPresets[3].compilerLocation = ""
        presets = initialPresets
        customDraft = Object.assign(JSON.parse(JSON.stringify(controller.compilerPreset(""))), {name: ""})
        javaMemory = "1024"
        summary = ""
    }
    Shortcut { sequence: "Escape"; onActivated: root.reject() }
    Shortcut { sequence: "Return"; onActivated: root.advance() }
    Shortcut { sequence: "Enter"; onActivated: root.advance() }

    function advance() {
        if (pageIndex === 0)
            pageIndex = custom ? 1 : 2
        else if (pageIndex === 3)
            finish()
        else if (validate())
            pageIndex = 3
    }

    function editCustom(key, value) {
        const changed = Object.assign({}, customDraft)
        changed[key] = value
        customDraft = changed
    }
    function editPreset(index, key, value) {
        const changed = JSON.parse(JSON.stringify(presets))
        changed[index][key] = value
        presets = changed
    }
    function toggleSelected(index, value) {
        const changed = selected.slice()
        changed[index] = value
        selected = changed
    }
    function toggleRecommended(index, value) {
        const changed = recommended.slice()
        changed[index] = value
        recommended = changed
    }
    function browse(index, field) {
        executableDialog.presetIndex = index
        executableDialog.field = field
        executableDialog.title = field === "compilerLocation"
            ? qsTranslate("AddCompilerWizard", "Select Compiler's Location")
            : qsTranslate("AddCompilerWizard", "Select Interpreter's Location")
        executableDialog.open()
    }
    function warn(text) {
        validationError = text
        errorDialog.open()
        return false
    }
    function validate() {
        if (custom) {
            if (!customDraft.name)
                return warn(qsTranslate("AddCompilerWizard", "Empty compiler name!"))
            if (customDraft.type !== 2 && !customDraft.compilerLocation)
                return warn(qsTranslate("AddCompilerWizard", "Empty compiler location!"))
            if (customDraft.type !== 0 && !customDraft.interpreterLocation)
                return warn(qsTranslate("AddCompilerWizard", "Empty interpreter location!"))
            if (!customDraft.sourceExtensions)
                return warn(qsTranslate("AddCompilerWizard", "Empty source file extensions!"))
            if (customDraft.type === 1 && !customDraft.bytecodeExtensions)
                return warn(qsTranslate("AddCompilerWizard", "Empty byte-code file extensions!"))
            summary = qsTranslate("AddCompilerWizard", "[Custom Compiler]") + "\n"
                + qsTranslate("AddCompilerWizard", "Compiler Name: ") + customDraft.name + "\n"
                + qsTranslate("AddCompilerWizard", "Compiler Type: ") + customType.currentText + "\n"
            if (customDraft.type !== 2)
                summary += qsTranslate("AddCompilerWizard", "Compiler's Location: ") + customDraft.compilerLocation + "\n"
            if (customDraft.type !== 0)
                summary += qsTranslate("AddCompilerWizard", "Interpreter's Location: ") + customDraft.interpreterLocation + "\n"
            summary += qsTranslate("AddCompilerWizard", "Source File Extensions: ") + customDraft.sourceExtensions + "\n"
            if (customDraft.type === 1)
                summary += qsTranslate("AddCompilerWizard", "Byte-code File Extensions: ") + customDraft.bytecodeExtensions + "\n"
            if (customDraft.type !== 2)
                summary += qsTranslate("AddCompilerWizard", "Default Compiler's Arguments: ") + customDraft.configurations[0].compilerArguments + "\n"
            if (customDraft.type !== 0)
                summary += qsTranslate("AddCompilerWizard", "Default Interpreter's Arguments: ") + customDraft.configurations[0].interpreterArguments + "\n"
        } else {
            const errors = [
                qsTranslate("AddCompilerWizard", "Empty gcc path!"),
                qsTranslate("AddCompilerWizard", "Empty g++ path!"),
                qsTranslate("AddCompilerWizard", "Empty fpc path!"),
                qsTranslate("AddCompilerWizard", "Empty fbc path!"),
                qsTranslate("AddCompilerWizard", "Empty javac path!"),
                qsTranslate("AddCompilerWizard", "Empty python path!")
            ]
            const titles = [
                qsTranslate("AddCompilerWizard", "[gcc Compiler]"),
                qsTranslate("AddCompilerWizard", "[g++ Compiler]"),
                qsTranslate("AddCompilerWizard", "[fpc Compiler]"),
                qsTranslate("AddCompilerWizard", "[fbc Compiler]"),
                qsTranslate("AddCompilerWizard", "[Java Compiler]"),
                qsTranslate("AddCompilerWizard", "[Python Compiler]")
            ]
            const labels = [
                qsTranslate("AddCompilerWizard", "gcc Path: "),
                qsTranslate("AddCompilerWizard", "g++ Path: "),
                qsTranslate("AddCompilerWizard", "fpc Path: "),
                qsTranslate("AddCompilerWizard", "fbc Path: "),
                qsTranslate("AddCompilerWizard", "javac Path: "),
                qsTranslate("AddCompilerWizard", "python Path: ")
            ]
            summary = ""
            for (let i = 0; i < selected.length; ++i) {
                if (!selected[i])
                    continue
                const location = presets[i][i === 5 ? "interpreterLocation" : "compilerLocation"]
                if (!location)
                    return warn(errors[i])
                summary += titles[i] + "\n" + labels[i] + location + "\n"
                if (i === 4) {
                    if (!presets[i].interpreterLocation)
                        return warn(qsTranslate("AddCompilerWizard", "Empty java path!"))
                    summary += qsTranslate("AddCompilerWizard", "java Path: ") + presets[i].interpreterLocation + "\n"
                    summary += qsTranslate("AddCompilerWizard", "Memory Limit: %1 MiB").arg(javaMemory) + "\n"
                }
                if (i < 3 && recommended[i])
                    summary += qsTranslate("AddCompilerWizard", "Add recommended configurations") + "\n"
                summary += "\n"
            }
        }
        return true
    }
    function finish() {
        let definitions = []
        if (custom) {
            definitions = [customDraft]
        } else {
            for (let i = 0; i < selected.length; ++i) {
                if (!selected[i])
                    continue
                const item = JSON.parse(JSON.stringify(presets[i]))
                if (i < 3 && !recommended[i])
                    item.configurations = item.configurations.slice(0, 1)
                if (i === 4)
                    item.configurations[0].interpreterArguments = "-Xmx" + javaMemory + "m %s"
                if (controller.windows && i < 2) {
                    item.environment = [{name: "PATH", value: controller.executableDirectory(item.compilerLocation)}]
                }
                definitions.push(item)
            }
        }
        if (controller.addCompilerDefinitions(definitions))
            accept()
        else
            warn(controller.error)
    }

    Rectangle {
        anchors.fill: parent
        anchors.bottomMargin: 50
        color: palette.base
    }
    Control {
        anchors.fill: parent
        font.pointSize: 10
        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 11
                currentIndex: root.pageIndex
                ColumnLayout {
                    spacing: 8
                    Label {
                        Layout.fillWidth: true
                        font.pointSize: 14
                        font.bold: true
                        text: qsTranslate("AddCompilerWizard", "Step 1/3: Setup compilers. Use built-in configuration or customize yourself.")
                        wrapMode: Text.WordWrap
                    }
                    Controls.RadioButton {
                        text: qsTranslate("AddCompilerWizard", "&Custom Configuration")
                        checked: root.custom
                        onClicked: root.custom = true
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Controls.RadioButton {
                            text: qsTranslate("AddCompilerWizard", "Built-&in Configuration (Tick the compilers you want to add)")
                            checked: !root.custom
                            onClicked: root.custom = false
                        }
                        RowLayout {
                            Layout.leftMargin: 29
                            Layout.topMargin: 9
                            Layout.bottomMargin: 9
                            enabled: !root.custom
                            spacing: 6
                            Repeater {
                                model: ["gcc", "g++", "fpc", "fbc", "jdk", "python"]
                                delegate: Controls.CheckBox {
                                    required property int index
                                    required property string modelData
                                    text: modelData
                                    checked: root.selected[index]
                                    onToggled: root.toggleSelected(index, checked)
                                }
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
                ColumnLayout {
                    spacing: 9
                    Label {
                        Layout.fillWidth: true
                        font.pointSize: 14
                        font.bold: true
                        text: qsTranslate("AddCompilerWizard", "Step 2/3: Select compiler's location to configure it.")
                        wrapMode: Text.WordWrap
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 3
                        columnSpacing: 10
                        rowSpacing: 9
                        Label { text: qsTranslate("AddCompilerWizard", "Compiler Name") }
                        Controls.TextField {
                            Layout.preferredWidth: 150
                            Layout.maximumWidth: 150
                            text: root.customDraft.name || ""
                            onTextEdited: root.editCustom("name", text)
                        }
                        Item {}
                        Label { text: qsTranslate("AddCompilerWizard", "Compiler Type") }
                        Controls.ComboBox {
                            id: customType
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            model: [qsTranslate("AddCompilerWizard", "Typical (Generate executable file)"),
                                qsTranslate("AddCompilerWizard", "Interpretive (Generate byte-code file)"),
                                qsTranslate("AddCompilerWizard", "Interpretive (Run source code directly)")]
                            currentIndex: root.customDraft.type || 0
                            onActivated: root.editCustom("type", currentIndex)
                        }
                        Label { text: qsTranslate("AddCompilerWizard", "Compiler's Location"); enabled: root.customDraft.type !== 2 }
                        Controls.TextField {
                            Layout.fillWidth: true
                            enabled: root.customDraft.type !== 2
                            text: root.customDraft.compilerLocation || ""
                            onTextEdited: root.editCustom("compilerLocation", text)
                        }
                        Controls.ToolButton { text: "..."; font.pointSize: 7; enabled: root.customDraft.type !== 2; onClicked: root.browse(-1, "compilerLocation") }
                        Label { text: qsTranslate("AddCompilerWizard", "Interpreter's Location"); enabled: root.customDraft.type !== 0 }
                        Controls.TextField {
                            Layout.fillWidth: true
                            enabled: root.customDraft.type !== 0
                            text: root.customDraft.interpreterLocation || ""
                            onTextEdited: root.editCustom("interpreterLocation", text)
                        }
                        Controls.ToolButton { text: "..."; font.pointSize: 7; enabled: root.customDraft.type !== 0; onClicked: root.browse(-1, "interpreterLocation") }
                        Label { text: qsTranslate("AddCompilerWizard", "Source File Extensions") }
                        Controls.TextField {
                            Layout.preferredWidth: 96; Layout.maximumWidth: 96
                            text: root.customDraft.sourceExtensions || ""
                            validator: RegularExpressionValidator { regularExpression: /(\w+;)*\w+/ }
                            onTextEdited: root.editCustom("sourceExtensions", text)
                        }
                        Item {}
                        Label { text: qsTranslate("AddCompilerWizard", "Byte-code File Extensions"); enabled: root.customDraft.type === 1 }
                        Controls.TextField {
                            Layout.preferredWidth: 96; Layout.maximumWidth: 96
                            enabled: root.customDraft.type === 1
                            text: root.customDraft.bytecodeExtensions || ""
                            validator: RegularExpressionValidator { regularExpression: /(\w+;)*\w+/ }
                            onTextEdited: root.editCustom("bytecodeExtensions", text)
                        }
                        Item {}
                        Label { text: qsTranslate("AddCompilerWizard", "Default Compiler's Arguments"); enabled: root.customDraft.type !== 2 }
                        Controls.TextField {
                            Layout.fillWidth: true; Layout.columnSpan: 2
                            enabled: root.customDraft.type !== 2
                            text: root.customDraft.configurations ? root.customDraft.configurations[0].compilerArguments : ""
                            onTextEdited: {
                                const configurations = JSON.parse(JSON.stringify(root.customDraft.configurations))
                                configurations[0].compilerArguments = text
                                root.editCustom("configurations", configurations)
                            }
                        }
                        Label { text: qsTranslate("AddCompilerWizard", "Default Interpreter's Arguments"); enabled: root.customDraft.type !== 0 }
                        Controls.TextField {
                            Layout.fillWidth: true; Layout.columnSpan: 2
                            enabled: root.customDraft.type !== 0
                            text: root.customDraft.configurations ? root.customDraft.configurations[0].interpreterArguments : ""
                            onTextEdited: {
                                const configurations = JSON.parse(JSON.stringify(root.customDraft.configurations))
                                configurations[0].interpreterArguments = text
                                root.editCustom("configurations", configurations)
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
                ColumnLayout {
                    spacing: 8
                    Label {
                        Layout.fillWidth: true
                        font.pointSize: 14
                        font.bold: true
                        text: qsTranslate("AddCompilerWizard", "Step 2/3: Select compilers' locations to configure them.")
                        wrapMode: Text.WordWrap
                    }
                    GridLayout {
                        id: builtGrid
                        Layout.fillWidth: true
                        columns: 2
                        uniformCellWidths: true
                        columnSpacing: 9
                        rowSpacing: 9
                        Repeater {
                            model: root.presetNames
                            delegate: Controls.GroupBox {
                                id: presetGroup
                                required property int index
                                required property string modelData
                                readonly property var definition: root.presets[index] || {}
                                Layout.fillWidth: true
                                topPadding: 32
                                enabled: root.selected[index]
                                title: modelData
                                ColumnLayout {
                                    anchors.fill: parent
                                    spacing: 6
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8
                                        Label { visible: presetGroup.index === 4; text: qsTranslate("AddCompilerWizard", "javac") }
                                        Controls.TextField {
                                            Layout.fillWidth: true
                                            text: presetGroup.definition[presetGroup.index === 5 ? "interpreterLocation" : "compilerLocation"] || ""
                                            onTextEdited: root.editPreset(presetGroup.index, presetGroup.index === 5 ? "interpreterLocation" : "compilerLocation", text)
                                        }
                                        Controls.ToolButton {
                                            text: "..."
                                            font.pointSize: 7
                                            onClicked: root.browse(presetGroup.index, presetGroup.index === 5 ? "interpreterLocation" : "compilerLocation")
                                        }
                                    }
                                    RowLayout {
                                        visible: presetGroup.index === 4
                                        Layout.fillWidth: true
                                        spacing: 8
                                        Label { text: qsTranslate("AddCompilerWizard", "java") }
                                        Controls.TextField {
                                            Layout.fillWidth: true
                                            text: presetGroup.definition.interpreterLocation || ""
                                            onTextEdited: root.editPreset(presetGroup.index, "interpreterLocation", text)
                                        }
                                        Controls.ToolButton { text: "..."; font.pointSize: 7; onClicked: root.browse(presetGroup.index, "interpreterLocation") }
                                    }
                                    Controls.CheckBox {
                                        visible: presetGroup.index < 3
                                        text: qsTranslate("AddCompilerWizard", "Add recommended configurations")
                                        checked: root.recommended[presetGroup.index]
                                        onToggled: root.toggleRecommended(presetGroup.index, checked)
                                    }
                                    RowLayout {
                                        visible: presetGroup.index === 4
                                        Layout.fillWidth: true
                                        spacing: 8
                                        Label { text: qsTranslate("AddCompilerWizard", "Memory Limit") }
                                        Controls.TextField {
                                            Layout.preferredWidth: 50; Layout.maximumWidth: 50
                                            text: root.javaMemory
                                            validator: IntValidator { bottom: 64; top: 2048 }
                                            onTextEdited: root.javaMemory = text
                                        }
                                        Label { text: qsTranslate("AddCompilerWizard", "MiB") }
                                        Item { Layout.fillWidth: true }
                                    }
                                    Item { visible: presetGroup.index === 3 || presetGroup.index === 5; Layout.fillHeight: true; implicitHeight: presetGroup.index === 3 ? 19 : 54 }
                                }
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
                ColumnLayout {
                    spacing: 10
                    Label {
                        Layout.fillWidth: true
                        font.pointSize: 14
                        font.bold: true
                        text: qsTranslate("AddCompilerWizard", "Step 3/3: Check the result and start using LemonLime.")
                        wrapMode: Text.WordWrap
                    }
                    Controls.Frame {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumWidth: 310
                        Layout.minimumHeight: 220
                        ScrollView {
                            anchors.fill: parent
                            TextArea {
                                text: root.summary
                                readOnly: true
                                selectByMouse: true
                                wrapMode: TextEdit.NoWrap
                                background: null
                            }
                        }
                    }
                }
            }
            Rectangle { Layout.fillWidth: true; implicitHeight: 2; color: palette.mid }
            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 11
                spacing: 6
                Item { Layout.fillWidth: true }
                Controls.Button {
                    enabled: root.pageIndex > 0
                    text: qsTranslate("QWizard", "< &Back")
                    onClicked: root.pageIndex = root.pageIndex === 3 ? (root.custom ? 1 : 2) : 0
                }
                Controls.Button {
                    visible: root.pageIndex < 3
                    text: qsTranslate("QWizard", "&Next >")
                    highlighted: true
                    onClicked: root.advance()
                }
                Controls.Button {
                    visible: root.pageIndex === 3
                    text: qsTranslate("QWizard", "&Finish")
                    highlighted: true
                    onClicked: root.finish()
                }
                Controls.Button { text: qsTranslate("QWizard", "Cancel"); onClicked: root.reject() }
            }
        }
    }
    SystemPalette { id: palette }
    Dialogs.FileDialog {
        id: executableDialog
        property int presetIndex: -1
        property string field: ""
        nameFilters: [root.controller.windows ? qsTranslate("AddCompilerWizard", "Executable files (*.exe)") : qsTranslate("AddCompilerWizard", "Executable files (*.*)")]
        onAccepted: {
            const file = root.controller.localFile(selectedFile)
            if (presetIndex < 0)
                root.editCustom(field, file)
            else
                root.editPreset(presetIndex, field, file)
        }
    }
    Dialogs.MessageDialog {
        id: errorDialog
        title: qsTranslate("AddCompilerWizard", "Error")
        text: root.validationError
        buttons: Dialogs.MessageDialog.Close
    }
}
