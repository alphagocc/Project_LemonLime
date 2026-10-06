/* SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick
import QtQuick.Controls as Platform
import LemonLime
Platform.TextField {
    id: control
    implicitHeight: StyleMetrics.sizeFor("edit", text, font).height
    selectByMouse: true
    Binding { target: control; property: "topPadding"; value: 2; when: Qt.platform.os === "windows" }
    Binding { target: control; property: "bottomPadding"; value: 2; when: Qt.platform.os === "windows" }
    Binding { target: control; property: "leftPadding"; value: 3; when: Qt.platform.os === "windows" }
    Binding { target: control; property: "rightPadding"; value: 3; when: Qt.platform.os === "windows" }
}
