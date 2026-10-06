/* SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick
import LemonLime
import QtQuick.Controls as Platform
Platform.ApplicationWindow {
    id: dialog
    font.family: StyleMetrics.applicationFont.family
    font.pointSize: StyleMetrics.applicationFont.pointSize
    font.hintingPreference: StyleMetrics.applicationFont.hintingPreference
    property bool closingAccepted: false
    signal accepted()
    signal rejected()
    signal opened()
    flags: Qt.Dialog
    modality: Qt.ApplicationModal
    color: colors.window
    SystemPalette { id: colors }
    function open() { closingAccepted = false; show(); if (!(flags & Qt.WindowDoesNotAcceptFocus)) requestActivate(); opened() }
    function accept() { closingAccepted = true; hide(); accepted() }
    function reject() { closingAccepted = true; hide(); rejected() }
    onClosing: function(event) { event.accepted = false; if (!closingAccepted) reject(); else hide() }
}
