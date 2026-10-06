/* SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick
import LemonLime
import QtQuick.Controls
import "." as Controls
Frame {
    id: control
    property string text: ""
    signal linkActivated(string link)
    function scrollToEnd() { viewport.contentY = Math.max(0, viewport.contentHeight - viewport.height) }
    Flickable {
        id: viewport
        anchors.fill: parent; anchors.margins: control.padding
        clip: true
        contentWidth: Math.max(width, document.documentWidth)
        contentHeight: document.documentHeight
        boundsBehavior: Flickable.StopAtBounds
        TextDocumentItem {
            id: document
            x: viewport.contentX; y: viewport.contentY
            width: viewport.width; height: viewport.height
            offsetX: viewport.contentX; offsetY: viewport.contentY
            html: control.text; font: control.font
            onLinkActivated: function(link) { control.linkActivated(link) }
        }
        ScrollBar.vertical: Controls.ScrollBar {}
        ScrollBar.horizontal: Controls.ScrollBar {}
    }
}
