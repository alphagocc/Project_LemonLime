/* SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick
import "controls" as Controls
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
Control {
    id: root
    required property var controller
    property alias contestTitle: titleField.text
    property alias savingName: savingField.text
    property alias contestPath: pathField.text
    readonly property bool ready: contestTitle.length > 0 && savingName.length > 0 && contestPath.length > 0
    font.pointSize: 10
    padding: 9
    GridLayout {
        anchors.left: parent.left; anchors.top: parent.top; anchors.right: parent.right
        anchors.margins: 9
        columns: 2; columnSpacing: 12; rowSpacing: 15
        Label { text: qsTranslate("NewContestWidget", "Contest Title") }
        Controls.TextField { id: titleField; objectName: "contestTitle"; Layout.fillWidth: true }
        Label { text: qsTranslate("NewContestWidget", "Saving Name") }
        Controls.TextField { id: savingField; objectName: "savingName"; Layout.fillWidth: true; onTextEdited: pathField.text = root.controller.defaultContestPath(text) }
        Label { text: qsTranslate("NewContestWidget", "Contest Path") }
        RowLayout {
            Layout.fillWidth: true; spacing: 10
            Controls.TextField { id: pathField; objectName: "contestPath"; Layout.fillWidth: true }
            Controls.ToolButton { text: "..."; onClicked: folder.open() }
        }
    }
    FolderDialog {
        id: folder
        title: qsTranslate("NewContestWidget", "Select Contest Path")
        onAccepted: pathField.text = root.controller.localFile(selectedFolder)
    }
}
