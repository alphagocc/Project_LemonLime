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
    objectName: "AdvancedCompilerSettingsDialog"
    font.pointSize: 10
    required property var controller
    property var draft: ({})
    property int configurationIndex: 0
    property int configurationCount: 0
    readonly property var configurations: draft.configurations || []
    readonly property var configuration: configurations[configurationIndex] || {}
    title: qsTranslate("AdvancedCompilerSettingsDialog", "Compiler Settings")
    width: 667
    height: Math.max(controller.windows ? 641 : 490, contents.implicitHeight + 22)
    minimumWidth: 667
    minimumHeight: Math.max(controller.windows ? 641 : 490, contents.implicitHeight + 22)

    onOpened: {
        draft = JSON.parse(JSON.stringify(controller.compiler))
        configurationIndex = 0
        configurationCount = 0
    }
    function change(key, value) {
        const changed = Object.assign({}, draft)
        changed[key] = value
        draft = changed
    }
    function changeConfiguration(key, value) {
        const changed = JSON.parse(JSON.stringify(configurations))
        if (configurationIndex >= 0 && configurationIndex < changed.length) {
            changed[configurationIndex][key] = value
            change("configurations", changed)
        }
    }
    function selectConfiguration(index) {
        if (index === configurations.length) {
            const changed = JSON.parse(JSON.stringify(configurations))
            changed.push({ name: qsTranslate("AdvancedCompilerSettingsDialog", "New configuration %1").arg(++configurationCount), compilerArguments: "", interpreterArguments: "" })
            change("configurations", changed)
        }
        configurationIndex = index
    }
    function browse(field) {
        executableDialog.field = field
        executableDialog.title = field === "compilerLocation"
            ? qsTranslate("AdvancedCompilerSettingsDialog", "Select Compiler's Location")
            : qsTranslate("AdvancedCompilerSettingsDialog", "Select Interpreter's Location")
        executableDialog.open()
    }
    function confirm() {
        editorContents.forceActiveFocus()
        if (controller.replaceCompiler(draft))
            accept()
        else
            errorDialog.open()
    }
    Shortcut { sequence: "Escape"; onActivated: root.reject() }
    Shortcut { sequence: "Return"; enabled: !sandboxDirectories.activeFocus; onActivated: root.confirm() }
    Shortcut { sequence: "Enter"; enabled: !sandboxDirectories.activeFocus; onActivated: root.confirm() }

    Control {
        id: editorContents
        anchors.fill: parent
        font.pointSize: 10
        ColumnLayout {
            id: contents
            anchors.fill: parent
            anchors.margins: 11
            spacing: 6
            Controls.ComboBox {
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                model: [qsTranslate("AdvancedCompilerSettingsDialog", "Typical (Generate executable file)"),
                    qsTranslate("AdvancedCompilerSettingsDialog", "Interpretive (Generate byte-code file)"),
                    qsTranslate("AdvancedCompilerSettingsDialog", "Interpretive (Run source code directly)")]
                currentIndex: root.draft.type || 0
                onActivated: root.change("type", currentIndex)
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                uniformCellSizes: true
                Controls.GroupBox {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    topPadding: 32
                    title: qsTranslate("AdvancedCompilerSettingsDialog", "Location")
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 6
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 3
                            columnSpacing: 9
                            rowSpacing: 6
                            Label { text: qsTranslate("AdvancedCompilerSettingsDialog", "Compiler"); enabled: root.draft.type !== 2 }
                            Controls.TextField {
                                Layout.fillWidth: true
                                text: root.draft.compilerLocation || ""
                                enabled: root.draft.type !== 2
                                onTextEdited: root.change("compilerLocation", text)
                            }
                            Controls.ToolButton {
                                text: "..."
                                enabled: root.draft.type !== 2
                                onClicked: root.browse("compilerLocation")
                            }
                            Label { text: qsTranslate("AdvancedCompilerSettingsDialog", "Interpreter"); enabled: root.draft.type !== 0 }
                            Controls.TextField {
                                Layout.fillWidth: true
                                text: root.draft.interpreterLocation || ""
                                enabled: root.draft.type !== 0
                                onTextEdited: root.change("interpreterLocation", text)
                            }
                            Controls.ToolButton {
                                text: "..."
                                enabled: root.draft.type !== 0
                                onClicked: root.browse("interpreterLocation")
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 9
                            enabled: root.draft.type === 1
                            Label { text: qsTranslate("AdvancedCompilerSettingsDialog", "Byte-code File Extensions") }
                            Item { Layout.fillWidth: true }
                            Controls.TextField {
                                Layout.preferredWidth: 50
                                Layout.maximumWidth: 50
                                text: root.draft.bytecodeExtensions || ""
                                validator: RegularExpressionValidator { regularExpression: /(\w+;)*\w+/ }
                                onTextEdited: root.change("bytecodeExtensions", text)
                            }
                        }
                    }
                }
                Controls.GroupBox {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    topPadding: 32
                    title: qsTranslate("AdvancedCompilerSettingsDialog", "Time and Memory Limit")
                    GridLayout {
                        anchors.fill: parent
                        columns: 3
                        columnSpacing: 8
                        rowSpacing: 6
                        Label { text: qsTranslate("AdvancedCompilerSettingsDialog", "Time Limit Ration") }
                        Item { Layout.fillWidth: true; implicitWidth: 20 }
                        SettingsDoubleSpinBox {
                            Layout.preferredWidth: 58
                            decimals: 1; minimum: 0.1; maximum: 999.9; step: 0.1
                            realValue: root.draft.timeLimitRatio || 1
                            onRealValueModified: number => root.change("timeLimitRatio", number)
                        }
                        Label { text: qsTranslate("AdvancedCompilerSettingsDialog", "Memory Limit Ration"); enabled: !root.draft.disableMemoryLimitCheck }
                        Item { Layout.fillWidth: true; implicitWidth: 20 }
                        SettingsDoubleSpinBox {
                            Layout.preferredWidth: 58
                            enabled: !root.draft.disableMemoryLimitCheck
                            decimals: 1; minimum: 0.1; maximum: 999.9; step: 0.1
                            realValue: root.draft.memoryLimitRatio || 1
                            onRealValueModified: number => root.change("memoryLimitRatio", number)
                        }
                        Controls.CheckBox {
                            text: qsTranslate("AdvancedCompilerSettingsDialog", "Disable Memory Limit")
                            checked: !!root.draft.disableMemoryLimitCheck
                            onToggled: root.change("disableMemoryLimitCheck", checked)
                        }
                    }
                }
            }
            Controls.GroupBox {
                Layout.fillWidth: true
                topPadding: 32
                title: qsTranslate("AdvancedCompilerSettingsDialog", "Arguments")
                GridLayout {
                    anchors.fill: parent
                    columns: 3
                    columnSpacing: 8
                    rowSpacing: 6
                    Label { Layout.row: 0; Layout.column: 0; text: qsTranslate("AdvancedCompilerSettingsDialog", "Configuration") }
                    Controls.ComboBox {
                        id: configurationSelect
                        Layout.row: 0; Layout.column: 1
                        Layout.fillWidth: true
                        model: root.configurations.map(item => item.name).concat([qsTranslate("AdvancedCompilerSettingsDialog", "Add new ...")])
                        currentIndex: root.configurationIndex
                        editable: true
                        onActivated: root.selectConfiguration(currentIndex)
                        Connections {
                            target: configurationSelect.contentItem
                            function onTextEdited() {
                                if (root.configurationIndex === 0)
                                    configurationSelect.editText = "default"
                                else
                                    root.changeConfiguration("name", configurationSelect.editText)
                            }
                        }
                    }
                    Controls.ToolButton {
                        Layout.row: 0; Layout.column: 2
                        Layout.preferredWidth: 27
                        Layout.preferredHeight: 27
                        icon.source: "image://lemon-icons/edit-delete?palette=" + StyleMetrics.paletteRevision
                        enabled: root.configurationIndex > 0
                        onClicked: {
                            const changed = JSON.parse(JSON.stringify(root.configurations))
                            changed.splice(root.configurationIndex, 1)
                            root.configurationIndex = Math.min(root.configurationIndex, changed.length - 1)
                            root.change("configurations", changed)
                        }
                    }
                    Label {
                        Layout.row: 1; Layout.column: 0
                        text: qsTranslate("AdvancedCompilerSettingsDialog", "Compiler's Arguments")
                        enabled: root.draft.type !== 2
                    }
                    Controls.TextField {
                        Layout.row: 2; Layout.column: 0; Layout.columnSpan: 3; Layout.fillWidth: true
                        text: root.configuration.compilerArguments || ""
                        enabled: root.draft.type !== 2
                        onTextEdited: root.changeConfiguration("compilerArguments", text)
                    }
                    Label {
                        Layout.row: 3; Layout.column: 0
                        text: qsTranslate("AdvancedCompilerSettingsDialog", "Interpreter's Arguments")
                        enabled: root.draft.type !== 0
                    }
                    Controls.TextField {
                        Layout.row: 4; Layout.column: 0; Layout.columnSpan: 3; Layout.fillWidth: true
                        text: root.configuration.interpreterArguments || ""
                        enabled: root.draft.type !== 0
                        onTextEdited: root.changeConfiguration("interpreterArguments", text)
                    }
                    Controls.CheckBox {
                        Layout.row: 5; Layout.column: 0
                        text: qsTranslate("AdvancedCompilerSettingsDialog", "Interpreter As Watcher")
                        enabled: root.draft.type !== 0
                        checked: !!root.draft.interpreterAsWatcher
                        onToggled: root.change("interpreterAsWatcher", checked)
                    }
                }
            }
            Controls.GroupBox {
                id: sandboxGroup
                Layout.fillWidth: true
                visible: root.controller.windows
                font.pointSize: 9
                topPadding: 31
                title: qsTranslate("AdvancedCompilerSettingsDialog", "Experimental Windows sandbox")
                checkable: true
                checked: !!root.draft.sandboxEnabled
                onToggled: root.change("sandboxEnabled", checked)
                GridLayout {
                    anchors.fill: parent
                    enabled: !!root.draft.sandboxEnabled
                    columns: 2
                    columnSpacing: 6
                    rowSpacing: 6
                    Label { text: qsTranslate("AdvancedCompilerSettingsDialog", "Runtime policy") }
                    Controls.ComboBox {
                        Layout.fillWidth: true
                        model: [qsTranslate("AdvancedCompilerSettingsDialog", "Automatic"),
                            qsTranslate("AdvancedCompilerSettingsDialog", "C / C++ and native programs"),
                            qsTranslate("AdvancedCompilerSettingsDialog", "Java"),
                            qsTranslate("AdvancedCompilerSettingsDialog", "Python")]
                        currentIndex: root.draft.sandboxRuntime || 0
                        onActivated: root.change("sandboxRuntime", currentIndex)
                    }
                    Label {
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredHeight: 28
                        verticalAlignment: Text.AlignVCenter
                        text: qsTranslate("AdvancedCompilerSettingsDialog", "Additional read-only directories")
                    }
                    Controls.Frame {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 70
                        ScrollView {
                            anchors.fill: parent
                            TextArea {
                                id: sandboxDirectories
                                text: root.draft.sandboxDirectories || ""
                                placeholderText: qsTranslate("AdvancedCompilerSettingsDialog", "Optional; one directory per line")
                                selectByMouse: true
                                wrapMode: TextEdit.NoWrap
                                background: null
                                onTextChanged: {
                                    if (activeFocus)
                                        root.change("sandboxDirectories", text)
                                }
                            }
                        }
                    }
                    Label { text: qsTranslate("AdvancedCompilerSettingsDialog", "Preparation time limit") }
                    Controls.SpinBox {
                        Layout.fillWidth: true
                        from: 1; to: 120
                        value: root.draft.sandboxTimeLimit || 15
                        textFromValue: function(value, locale) { return value.toLocaleString(locale, "f", 0) + qsTranslate("AdvancedCompilerSettingsDialog", " s") }
                        valueFromText: function(text, locale) { return Number.fromLocaleString(locale, text.replace(/\s*s$/, "")) }
                        onValueModified: root.change("sandboxTimeLimit", value)
                    }
                    Label {
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        text: qsTranslate("AdvancedCompilerSettingsDialog", "Disabled by default. Temporary runtime permissions are removed after judging. Each run has private files and no network access.")
                        wrapMode: Text.WordWrap
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Controls.Button {
                    text: qsTranslate("AdvancedCompilerSettingsDialog", "Environment Variables")
                    onClicked: environmentDialog.open()
                }
                Item { Layout.fillWidth: true }
                Controls.Button {
                    text: qsTranslate("QPlatformTheme", "OK")
                    highlighted: true
                    onClicked: root.confirm()
                }
                Controls.Button { text: qsTranslate("QPlatformTheme", "Cancel"); onClicked: root.reject() }
            }
        }
    }
    EnvironmentVariablesDialog {
        id: environmentDialog
        environment: root.draft.environment || []
        onEnvironmentAccepted: values => root.change("environment", values)
    }
    Dialogs.FileDialog {
        id: executableDialog
        property string field: ""
        nameFilters: [root.controller.windows ? qsTranslate("AdvancedCompilerSettingsDialog", "Executable files (*.exe)") : qsTranslate("AdvancedCompilerSettingsDialog", "Executable files (*.*)")]
        onAccepted: root.change(field, root.controller.localFile(selectedFile))
    }
    Dialogs.MessageDialog {
        id: errorDialog
        title: qsTranslate("AdvancedCompilerSettingsDialog", "Error")
        text: root.controller.error
        buttons: Dialogs.MessageDialog.Close
    }
}
