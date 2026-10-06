/* SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import "controls" as Controls
Control {
    id: root
    required property var controller
    font.pointSize: 10
    function exportReport() { output.open() }
    Controls.TextBrowser { anchors.fill: parent; anchors.margins: 20; text: root.controller.statisticsHtml }
    FileDialog { id: output; title: qsTranslate("StatisticsBrowser", "Export Statistics"); fileMode: FileDialog.SaveFile; nameFilters: [qsTranslate("StatisticsBrowser", "HTML Document (*.html)")]; defaultSuffix: "html"; onAccepted: root.controller.exportStatistics(selectedFile) }
    onVisibleChanged: if (visible && controller) controller.refresh()
}
