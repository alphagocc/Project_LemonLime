/* SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick
import QtQuick.Controls as Platform
import LemonLime
Platform.SpinBox {
    id: control
    readonly property bool compactWindows: Qt.platform.os === "windows"
    readonly property int frameWidth: StyleMetrics.metric("spinFrame")
    readonly property int stepButtonWidth: StyleMetrics.metric("smallIcon")
    readonly property rect upRect: StyleMetrics.spinRect(width, height, font, mirrored, "up")
    readonly property rect downRect: StyleMetrics.spinRect(width, height, font, mirrored, "down")
    readonly property string widestText: {
        const candidates = [displayText,
            textFromValue ? textFromValue(from, locale) : from.toLocaleString(locale, "f", 0),
            textFromValue ? textFromValue(to, locale) : to.toLocaleString(locale, "f", 0)]
        return candidates.reduce((widest, candidate) =>
            StyleMetrics.textWidth(candidate, font) > StyleMetrics.textWidth(widest, font) ? candidate : widest, "")
    }
    implicitHeight: StyleMetrics.sizeFor("spin", displayText, font).height
    editable: true

    Binding {
        when: control.compactWindows
        control.implicitWidth: StyleMetrics.sizeFor("spin", control.widestText, control.font).width
        control.leftPadding: control.frameWidth + 1 + (control.mirrored ? control.stepButtonWidth : 0)
        control.rightPadding: control.frameWidth + 1 + (control.mirrored ? 0 : control.stepButtonWidth)
        control.topPadding: control.frameWidth
        control.bottomPadding: control.frameWidth
        control.background: spinBackground
    }
    Binding { when: control.compactWindows; target: control.up; property: "indicator"; value: upIndicator }
    Binding { when: control.compactWindows; target: control.down; property: "indicator"; value: downIndicator }

    StyleItem {
        id: spinBackground
        visible: control.compactWindows
        kind: "spin"
        font: control.font
        mirrored: control.mirrored
        enabled: control.enabled
        focused: control.activeFocus
        hovered: control.hovered || control.up.hovered || control.down.hovered
        pressed: control.up.pressed || control.down.pressed
        value: control.up.pressed ? 1 : control.down.pressed ? 2 : control.up.hovered ? 1 : control.down.hovered ? 2 : 0
        stepUpEnabled: upIndicator.enabled
        stepDownEnabled: downIndicator.enabled
    }
    Item {
        id: upIndicator
        visible: control.compactWindows
        x: control.upRect.x
        y: control.upRect.y
        width: control.stepButtonWidth
        height: control.upRect.height
    }
    Item {
        id: downIndicator
        visible: control.compactWindows
        x: control.downRect.x
        y: control.downRect.y
        width: control.stepButtonWidth
        height: control.downRect.height
    }
}
