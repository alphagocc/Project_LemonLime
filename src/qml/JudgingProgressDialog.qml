/* SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "controls" as Controls
Controls.Dialog {
    id: dialog
    required property var controller
    title: qsTranslate("JudgingDialog", "Judging")
    width: 448; height: 276; minimumWidth: 448; minimumHeight: 263
    onRejected: controller.stop()
    Control {
        anchors.fill: parent
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 11; spacing: 6
            Controls.TextBrowser { id: log; Layout.fillWidth: true; Layout.fillHeight: true; text: dialog.controller.progressHtml; onTextChanged: Qt.callLater(scrollToEnd) }
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 20; radius: 5
                color: "transparent"; border.width: 2; border.color: palette.text
                Rectangle { x: 2; y: 2; height: parent.height - 4; width: Math.max(0, (parent.width - 4) * Math.min(1, dialog.controller.progressValue / Math.max(1, dialog.controller.progressMaximum))); radius: 3; color: "#00cc33" }
                Label { anchors.centerIn: parent; font.pointSize: 10; font.bold: true; font.italic: true; text: dialog.controller.progressValue + " / " + dialog.controller.progressMaximum + " ms (" + Math.floor(100 * dialog.controller.progressValue / Math.max(1, dialog.controller.progressMaximum)) + "%)" }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 6
                Controls.Button { text: qsTranslate("JudgingDialog", "Skip"); icon.source: "image://lemon-icons/media-skip-forward?palette=" + StyleMetrics.paletteRevision; Layout.minimumWidth: 120; font.pointSize: 10; enabled: false }
                Item { Layout.fillWidth: true }
                Controls.Button { text: qsTranslate("JudgingDialog", "Cancel"); icon.source: "image://lemon-icons/media-playback-stop?palette=" + StyleMetrics.paletteRevision; Layout.minimumWidth: 120; font.pointSize: 10; enabled: !dialog.controller.stopping; onClicked: dialog.controller.stop() }
            }
        }
    }
}
