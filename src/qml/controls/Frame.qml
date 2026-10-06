/* SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick
import LemonLime
import QtQuick.Controls.Basic as Basic
Basic.Frame {
    id: control
    padding: StyleMetrics.metric("frame")
    background: StyleItem { kind: "frame"; enabled: control.enabled }
}
