/* SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick
import LemonLime
import QtQuick.Controls.Basic as Basic
Basic.GroupBox {
    id: control
    property bool checkable: false
    property bool checked: true
    signal toggled()
    leftPadding: 12
    rightPadding: 12
    topPadding: StyleMetrics.textHeight(font) + 14
    bottomPadding: 12
    label: Item {}
    contentItem.enabled: !checkable || checked
    background: StyleItem {
        kind: "group"; text: control.title; font: control.font; flat: control.checkable; checked: control.checked; enabled: control.enabled
        MouseArea { x: 0; y: 0; width: StyleMetrics.textWidth(control.title, control.font) + 36; height: StyleMetrics.textHeight(control.font) + 4; enabled: control.checkable; onClicked: { control.checked = !control.checked; control.toggled() } }
    }
}
