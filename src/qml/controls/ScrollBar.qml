/* SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick
import QtQuick.Controls as Platform
Platform.ScrollBar {
    id: control
    SystemPalette { id: colors }
    // Fluent uses Fusion's scrollbar, whose Mid role is too dark with the
    // Windows dark palette. Keep its behavior and derive the thumb from Text.
    Binding {
        when: Qt.platform.os === "windows"
        target: control
        property: "palette.mid"
        value: Qt.alpha(colors.text, 0.6)
    }
    Binding {
        when: Qt.platform.os === "windows"
        target: control
        property: "palette.dark"
        value: Qt.alpha(colors.text, 0.8)
    }
}
