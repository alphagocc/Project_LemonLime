/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    font: StyleMetrics.applicationFont
    objectName: "OptionsDialog"
    required property var controller
    title: qsTranslate("OptionsDialog", "Options")
    flags: Qt.Dialog
    modality: Qt.ApplicationModal
    minimumWidth: 533
    minimumHeight: 437
    width: minimumWidth
    height: minimumHeight
    color: palette.window

    function open() {
        controller.startEditing()
        contents.reset()
        show()
        if (!(flags & Qt.WindowDoesNotAcceptFocus)) requestActivate()
    }

    onClosing: controller.cancel()

    Shortcut {
        sequence: "Escape"
        onActivated: {
            root.controller.cancel()
            root.hide()
        }
    }

    SettingsPage {
        id: contents
        anchors.fill: parent
        controller: root.controller
        onCloseRequested: root.hide()
    }
}
