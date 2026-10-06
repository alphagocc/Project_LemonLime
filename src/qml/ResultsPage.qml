/* SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "controls" as Controls
Control {
    id: root
    required property var controller
    property string detailName: ""
    property int currentRow: 0
    property int currentColumn: 0
    property int anchorRow: 0
    property int anchorColumn: 0
    property int sortedColumn: 0
    property bool ascending: true
    signal cleanupRequested()
    font.pointSize: 10
    font.bold: false
    function select(row, column, modifiers) {
        currentRow = row; currentColumn = column
        if (modifiers & Qt.ShiftModifier) controller.results.selectRange(anchorRow, anchorColumn, row, column, !!(modifiers & Qt.ControlModifier))
        else { anchorRow = row; anchorColumn = column; controller.results.selectCell(row, column, !!(modifiers & Qt.ControlModifier)) }
    }
    function details(name) { if (controller.judging || !name) return; detailName = name; controller.showDetails(name, -1); detailDialog.open() }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 11; spacing: 6
        Controls.Frame {
            id: tableFrame; Layout.fillWidth: true; Layout.fillHeight: true
            ColumnLayout {
                anchors.fill: parent; spacing: 0
                HorizontalHeaderView {
                    id: header; Layout.fillWidth: true; Layout.preferredHeight: StyleMetrics.sizeFor("header", qsTranslate("ResultViewer", "Rank"), root.font).height
                    syncView: table; clip: true
                    delegate: StyleItem {
                        required property var display
                        required property int column
                        kind: "header"; text: display; font: root.font
                        implicitWidth: 75; implicitHeight: header.height
                        checked: root.sortedColumn === column; value: root.ascending ? 0 : 1
                        first: column === 0; last: column === table.columns - 1
                        TapHandler { onTapped: { root.ascending = root.sortedColumn === column ? !root.ascending : true; root.sortedColumn = column; root.controller.results.sort(column, root.ascending ? Qt.AscendingOrder : Qt.DescendingOrder) } }
                    }
                }
                TableView {
                    id: table; objectName: "resultsTable"; Layout.fillWidth: true; Layout.fillHeight: true
                    clip: true; model: root.controller.results; rowSpacing: 0; columnSpacing: 0; reuseItems: true
                    columnWidthProvider: column => root.controller.results.columnWidth(column, root.font)
                    rowHeightProvider: row => 27
                    delegate: Rectangle {
                        id: cell
                        required property var display
                        required property color cellBackground
                        required property color cellForeground
                        required property bool cellSelected
                        required property string contestantName
                        required property int row
                        required property int column
                        implicitHeight: 27; implicitWidth: 75
                        color: cellSelected ? root.palette.highlight : cellBackground
                        Text {
                            anchors.fill: parent; anchors.margins: 3
                            text: cell.display; color: cell.cellSelected ? root.palette.highlightedText : cell.cellForeground
                            font.family: root.font.family; font.pointSize: root.font.pointSize; font.bold: cell.column === 2
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight; renderType: Text.NativeRendering
                        }
                        TapHandler {
                            id: tap; acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onTapped: function(point, button) {
                                table.forceActiveFocus(); root.detailName = cell.contestantName
                                if (button !== Qt.RightButton || !cell.cellSelected) root.select(cell.row, cell.column, tap.point.modifiers)
                                if (button === Qt.RightButton) contextMenu.popup()
                            }
                            onDoubleTapped: root.details(cell.contestantName)
                        }
                    }
                    ScrollBar.vertical: Controls.ScrollBar { objectName: "resultsVerticalScrollBar" }
                    ScrollBar.horizontal: Controls.ScrollBar { objectName: "resultsHorizontalScrollBar" }
                    Keys.onPressed: function(event) {
                        if (event.matches(StandardKey.SelectAll)) root.controller.results.selectAll()
                        else if (event.key === Qt.Key_Delete && root.controller.results.selectedCount) deleteDialog.open()
                        else if (event.key === Qt.Key_Return) root.details(root.controller.results.nameAt(root.currentRow))
                        else {
                            if (event.key === Qt.Key_Up) root.currentRow = Math.max(0, root.currentRow - 1)
                            else if (event.key === Qt.Key_Down) root.currentRow = Math.min(table.rows - 1, root.currentRow + 1)
                            else if (event.key === Qt.Key_Left) root.currentColumn = Math.max(0, root.currentColumn - 1)
                            else if (event.key === Qt.Key_Right) root.currentColumn = Math.min(table.columns - 1, root.currentColumn + 1)
                            else return
                            root.select(root.currentRow, root.currentColumn, event.modifiers)
                        }
                        event.accepted = true
                    }
                    Connections { target: root.controller.results; function onRefreshed() { table.forceLayout() } }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 6
            Controls.Button { objectName: "cleanupButton"; text: qsTranslate("LemonLime", "Clean Up Files"); icon.source: "image://lemon-icons/view-sort?palette=" + StyleMetrics.paletteRevision; Layout.minimumWidth: 120; onClicked: root.cleanupRequested() }
            Item { Layout.fillWidth: true }
            Controls.Button { objectName: "refreshButton"; text: qsTranslate("LemonLime", "Refresh"); icon.source: "image://lemon-icons/view-refresh?palette=" + StyleMetrics.paletteRevision; Layout.minimumWidth: 120; onClicked: root.controller.refreshContestants() }
            Controls.Button { objectName: "judgeUnjudgedButton"; text: qsTranslate("LemonLime", "Judge Unjudged"); icon.source: "image://lemon-icons/edit-find-replace?palette=" + StyleMetrics.paletteRevision; Layout.minimumWidth: 120; enabled: root.controller.results.count > 0; onClicked: root.controller.judge("unjudged") }
            Controls.Button { objectName: "judgeButton"; text: qsTranslate("LemonLime", "Judge Selected"); icon.source: "image://lemon-icons/edit-find?palette=" + StyleMetrics.paletteRevision; Layout.minimumWidth: 120; enabled: root.controller.results.selectedCount > 0; onClicked: root.controller.judge("selected") }
            Controls.Button { objectName: "judgeAllButton"; text: qsTranslate("LemonLime", "Judge All"); icon.source: "image://lemon-icons/code-function?palette=" + StyleMetrics.paletteRevision; Layout.minimumWidth: 120; enabled: root.controller.results.count > 0; onClicked: root.controller.judge("all") }
        }
    }
    Controls.Menu {
        id: contextMenu
        Controls.MenuItem { text: qsTranslate("ResultViewer", "Details"); visible: root.controller.results.selectedCount === 1; onTriggered: root.details(root.detailName) }
        Controls.MenuItem { text: qsTranslate("ResultViewer", "Judge"); onTriggered: root.controller.judge("selected") }
        Controls.MenuItem { text: qsTranslate("ResultViewer", "Delete"); onTriggered: deleteDialog.open() }
    }
    Controls.Dialog {
        id: deleteDialog; title: "LemonLime"; width: 410; height: 150; transientParent: root.Window.window
        Control {
            anchors.fill: parent; font: root.font
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 11; spacing: 6
                Label { text: qsTranslate("ResultViewer", "Are you sure to delete selected contestant(s)?"); Layout.fillWidth: true; wrapMode: Text.Wrap }
                Controls.CheckBox { id: deleteFiles; text: qsTranslate("ResultViewer", "Delete data in the disk as well") }
                Item { Layout.fillHeight: true }
                RowLayout { Item { Layout.fillWidth: true } Controls.Button { text: qsTranslate("QPlatformTheme", "OK"); onClicked: { root.controller.deleteSelected(deleteFiles.checked); deleteDialog.accept() } } Controls.Button { text: qsTranslate("QPlatformTheme", "Cancel"); onClicked: deleteDialog.reject() } }
            }
        }
    }
    Controls.Dialog {
        id: detailDialog; objectName: "detailDialog"; title: qsTranslate("DetailDialog", "Contestant: %1").arg(root.detailName)
        width: 709; height: 520; transientParent: root.Window.window
        Control {
            anchors.fill: parent; font.pointSize: 10
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 11; spacing: 6
                Controls.TextBrowser { Layout.fillWidth: true; Layout.fillHeight: true; text: root.controller.detailHtml; onLinkActivated: link => root.controller.detailLink(link) }
                RowLayout { Item { Layout.fillWidth: true } Controls.Button { text: qsTranslate("DetailDialog", "&Close"); icon.source: "image://lemon-icons/paint-none?palette=" + StyleMetrics.paletteRevision; onClicked: detailDialog.accept() } }
            }
        }
    }
}
