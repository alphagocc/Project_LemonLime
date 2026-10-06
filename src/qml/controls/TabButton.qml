/* SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick
import LemonLime
import QtQuick.Controls.Basic as Basic
Basic.TabButton {
    id: control
    property bool vertical: false
    property bool first: false
    property bool last: false
    implicitWidth: StyleMetrics.sizeFor(vertical ? "westtab" : "tab", text, font).width
    implicitHeight: StyleMetrics.sizeFor(vertical ? "westtab" : "tab", text, font).height
    width: implicitWidth
    padding: 0
    contentItem: Item {}
    background: StyleItem { kind: "tab"; text: control.text; font: control.font; hovered: control.hovered; pressed: control.down; checked: control.checked; focused: control.visualFocus; vertical: control.vertical; first: control.first; last: control.last; enabled: control.enabled }
}
