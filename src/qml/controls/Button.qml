/* SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick
import QtQuick.Controls as Platform
import LemonLime
Platform.Button {
    id: control
    implicitWidth: Math.max(80, StyleMetrics.sizeFor("button", text, font, icon.source.toString().length > 0).width)
    implicitHeight: StyleMetrics.sizeFor("button", text, font, icon.source.toString().length > 0).height
    icon.width: 16
    icon.height: 16
    icon.color: enabled ? "transparent" : palette.buttonText
    Binding { target: control; property: "topPadding"; value: 2; when: Qt.platform.os === "windows" }
    Binding { target: control; property: "bottomPadding"; value: 2; when: Qt.platform.os === "windows" }
    Binding { target: control; property: "leftPadding"; value: 6; when: Qt.platform.os === "windows" }
    Binding { target: control; property: "rightPadding"; value: 6; when: Qt.platform.os === "windows" }
    Binding { target: control; property: "spacing"; value: 4; when: Qt.platform.os === "windows" }
}
