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
    property int selectedIndex: -1
    readonly property bool ready: selectedIndex >= 0 && selectedIndex < controller.recentEntries.length
    readonly property string selectedFile: ready ? controller.recentEntries[selectedIndex].file : ""
    signal activated()
    font.pointSize: 10
    Label { id: headerFont; visible: false; font.family: root.font.family; font.pointSize: 11; font.bold: true }
    readonly property int titleWidth: Math.max(100, StyleMetrics.textWidth(qsTranslate("OpenContestWidget", "Title"), headerFont.font) + 8)
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 9; spacing: 6
        Controls.Frame {
            Layout.fillWidth: true; Layout.fillHeight: true
            ColumnLayout {
                anchors.fill: parent; spacing: 0
                Row {
                    Layout.fillWidth: true; Layout.preferredHeight: StyleMetrics.textHeight(headerFont.font) + 4
                    StyleItem { kind: "header"; text: qsTranslate("OpenContestWidget", "Title"); width: root.titleWidth; height: parent.height; font: headerFont.font; first: true }
                    StyleItem { kind: "header"; text: qsTranslate("OpenContestWidget", "Location"); width: parent.width - root.titleWidth; height: parent.height; font: headerFont.font; last: true }
                }
                ListView {
                    id: list; Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                    model: root.controller.recentEntries
                    ScrollBar.vertical: Controls.ScrollBar {}
                    delegate: Rectangle {
                        id: row
                        required property int index
                        required property var modelData
                        width: list.width; height: 27
                        color: root.selectedIndex === index ? root.palette.highlight : root.palette.base
                        Row {
                            anchors.fill: parent
                            Label { width: root.titleWidth; height: parent.height; text: row.modelData.title; color: root.selectedIndex === row.index ? root.palette.highlightedText : root.palette.text; elide: Text.ElideLeft; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            Label { width: parent.width - root.titleWidth; height: parent.height; leftPadding: 3; text: row.modelData.file; color: root.selectedIndex === row.index ? root.palette.highlightedText : root.palette.text; elide: Text.ElideLeft; verticalAlignment: Text.AlignVCenter }
                        }
                        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: root.palette.mid }
                        Rectangle { x: root.titleWidth - 1; width: 1; height: parent.height; color: root.palette.mid }
                        TapHandler { onTapped: root.selectedIndex = row.index; onDoubleTapped: root.activated() }
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 6
            Item { Layout.fillWidth: true }
            Controls.ToolButton { text: qsTranslate("OpenContestWidget", "&Add"); icon.source: "image://lemon-icons/list-add?palette=" + StyleMetrics.paletteRevision; iconOnly: false; Layout.minimumWidth: 96; Layout.minimumHeight: 32; onClicked: addFile.open() }
            Controls.ToolButton { text: qsTranslate("OpenContestWidget", "&Hide"); icon.source: "image://lemon-icons/list-remove?palette=" + StyleMetrics.paletteRevision; iconOnly: false; Layout.minimumWidth: 96; Layout.minimumHeight: 32; enabled: root.ready; onClicked: { root.controller.removeRecent(root.controller.recentEntries[root.selectedIndex].index); root.selectedIndex = -1 } }
        }
    }
    FileDialog { id: addFile; title: qsTranslate("OpenContestWidget", "Add Contest"); nameFilters: [qsTranslate("OpenContestWidget", "Lemon contest data file (*.cdf)")]; onAccepted: { if (root.controller.addRecent(selectedFile)) root.selectedIndex = 0 } }
}
