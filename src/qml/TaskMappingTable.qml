/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import "controls" as Controls

Rectangle {
    id: root
    property var model: []
    property int currentIndex: -1
    readonly property int rowHeaderWidth: Math.max(12, StyleMetrics.textWidth(String(model.length), Qt.font({family: StyleMetrics.applicationFont.family, pointSize: 10})) + 5)
    signal selected(int index)
    SystemPalette { id: colors }
    color: colors.base
    border.color: colors.mid
    ListView {
        id: list
        anchors.fill: parent
        anchors.margins: 1
        clip: true
        model: root.model
        ScrollBar.vertical: Controls.ScrollBar {}
        delegate: Item {
            id: row
            required property int index
            required property var modelData
            width: list.width
            height: 27
            Row {
                anchors.fill: parent
                Repeater {
                    model: [String(row.index + 1), row.modelData.source, row.modelData.name]
                    delegate: Rectangle {
                        required property int index
                        required property string modelData
                        width: index === 0 ? root.rowHeaderWidth : (list.width - root.rowHeaderWidth) / 2
                        height: 27
                        color: index === 0 ? colors.button : root.currentIndex === row.index ? colors.highlight : colors.base
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: colors.mid }
                        Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: colors.mid }
                        StyleItem { visible: index === 0; anchors.fill: parent; kind: "header"; vertical: true; text: modelData; font.pointSize: 10; first: row.index === 0; last: row.index + 1 === root.model.length }
                        Label { visible: index > 0; renderType: Text.NativeRendering; anchors.fill: parent; anchors.margins: 3; text: modelData; font.pointSize: 10; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; color: root.currentIndex === row.index ? colors.highlightedText : colors.text }
                    }
                }
            }
            MouseArea { anchors.fill: parent; onClicked: root.selected(index) }
        }
    }
}
