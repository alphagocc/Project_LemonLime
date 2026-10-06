/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import "controls" as Controls
import QtQuick.Controls
import QtQuick.Dialogs as NativeDialogs

Controls.TextField { font.pointSize: 10;
    id: root
    required property var controller
    property bool browseEnabled: true
    property var completions: []
    signal edited(string value)
    selectByMouse: true
    onEditingFinished: edited(text)
    onTextEdited: {
        completions = text.length ? controller.dataFiles(text) : []
        if (completions.length) completion.open()
        else completion.close()
    }
    function browse() { chooser.open() }
    Popup {
        id: completion
        x: 0
        y: root.height
        width: root.width
        height: Math.min(180, choices.contentHeight + 2)
        padding: 1
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        contentItem: ListView {
            id: choices
            clip: true
            model: root.completions
            delegate: ItemDelegate {
                required property string modelData
                width: choices.width
                height: 22
                text: modelData
                onClicked: {
                    root.text = modelData
                    root.edited(root.text)
                    completion.close()
                }
            }
        }
    }
    NativeDialogs.FileDialog {
        id: chooser
        title: qsTranslate("ExtTestCaseUpdaterDialog", "Choose Input File")
        currentFolder: root.controller.dataFolder
        onAccepted: {
            root.text = root.controller.relativeDataFile(selectedFile)
            root.edited(root.text)
        }
    }
}
