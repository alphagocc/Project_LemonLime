/* SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick
import QtQuick.Controls as Platform
import LemonLime
Platform.CheckBox {
    id: control
    Binding {
        when: Qt.platform.os === "windows"
        control.topPadding: 0
        control.bottomPadding: 0
        control.leftPadding: 0
        control.rightPadding: 0
        control.spacing: 4
        control.implicitHeight: Math.max(StyleMetrics.sizeFor("check", control.text, control.font).height,
                                        control.implicitContentHeight, control.implicitIndicatorHeight)
        control.implicitWidth: control.implicitContentWidth
    }
}
