/* SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick
import LemonLime
import QtQuick.Controls as Platform
Platform.MenuBar {
    id: bar
    objectName: "mainMenuBar"
    spacing: 0
    leftPadding: StyleMetrics.metric("menuBarHMargin")
    rightPadding: leftPadding
    topPadding: 0
    bottomPadding: 0
    implicitHeight: StyleMetrics.textHeight(font) + 17
    delegate: Platform.MenuBarItem {
        id: entry
        readonly property size originalSize: StyleMetrics.sizeFor("menubaritem", text, font)
        implicitWidth: originalSize.width
        implicitHeight: bar.implicitHeight
        leftPadding: Math.max(0, Math.floor((originalSize.width - StyleMetrics.textWidth(text.replace(/&/g, ""), font) - 2) / 2))
        rightPadding: leftPadding
        topPadding: 8
        bottomPadding: 9
    }
}
