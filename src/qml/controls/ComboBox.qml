/* SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick
import QtQuick.Controls as Platform
import LemonLime
Platform.ComboBox {
    id: control
    implicitHeight: StyleMetrics.sizeFor("combo", displayText, font).height
    Binding { target: control; property: "topPadding"; value: 2; when: Qt.platform.os === "windows" }
    Binding { target: control; property: "bottomPadding"; value: 2; when: Qt.platform.os === "windows" }
}
