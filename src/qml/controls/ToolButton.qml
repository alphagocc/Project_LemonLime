/* SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick
import QtQuick.Controls as Platform
// The original QToolButtons keep their frame when idle. Use the platform
// button background with compact tool-button dimensions for the same behavior.
Platform.Button {
    id: control
    property bool iconOnly: true
    display: icon.source.toString().length === 0 ? Platform.AbstractButton.TextOnly : iconOnly ? Platform.AbstractButton.IconOnly : Platform.AbstractButton.TextBesideIcon
    icon.width: 16
    icon.height: 16
    padding: 3
    implicitWidth: Math.max(22, contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(23, contentItem.implicitHeight + topPadding + bottomPadding)
    icon.color: enabled ? "transparent" : palette.buttonText
    Binding { target: control; property: "topPadding"; value: 3; when: Qt.platform.os === "windows" }
    Binding { target: control; property: "bottomPadding"; value: 3; when: Qt.platform.os === "windows" }
    Binding { target: control; property: "leftPadding"; value: 3; when: Qt.platform.os === "windows" }
    Binding { target: control; property: "rightPadding"; value: 3; when: Qt.platform.os === "windows" }
}
