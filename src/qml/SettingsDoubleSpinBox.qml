/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

import QtQuick
import "controls" as Controls
import QtQuick.Controls

Controls.SpinBox {
    id: root
    property real realValue: 0
    property real minimum: 0
    property real maximum: 100
    property real step: 1
    property int decimals: 2
    readonly property real multiplier: Math.pow(10, decimals)
    signal realValueModified(real number)
    from: Math.round(minimum * multiplier)
    to: Math.round(maximum * multiplier)
    stepSize: Math.round(step * multiplier)
    value: Math.round(realValue * multiplier)
    editable: true
    validator: DoubleValidator { bottom: root.minimum; top: root.maximum; decimals: root.decimals }
    textFromValue: function(value, locale) { return Number(value / multiplier).toLocaleString(locale, 'f', decimals) }
    valueFromText: function(text, locale) { return Math.round(Number.fromLocaleString(locale, text) * multiplier) }
    onValueModified: realValueModified(value / multiplier)
}
