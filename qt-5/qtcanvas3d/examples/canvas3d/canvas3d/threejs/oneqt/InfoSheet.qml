/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtQuick.Layouts 1.1

Item {
    id: infoSheet
    anchors.topMargin: 59
    anchors.leftMargin: 48
    property alias headingText1: heading1.text
    property alias headingText2: heading2.text
    property alias text: description.text
    state: "hidden"

    states: [
        State {
            name: "hidden"
            when: !visible
            PropertyChanges { target: heading1; color: "#00000000"; }
            PropertyChanges { target: heading2; color: "#005caa15"; }
            PropertyChanges { target: description; color: "#00000000"; }
            PropertyChanges { target: infoSheet; visible: false; }
        },
        State {
            name: "visible"
            when: visible
            PropertyChanges { target: heading1; color: "#FF000000"; }
            PropertyChanges { target: heading2; color: "#FF5caa15"; }
            PropertyChanges { target: description; color: "#FF000000"; }
            PropertyChanges { target: infoSheet; visible: true; }
        }
    ]

    transitions: [
        Transition {
            from: "visible"
            to: "hidden"
            ColorAnimation {
                properties: "color"
                easing.type: Easing.InOutCubic
                duration: 300
            }
            PropertyAnimation {
                properties: "visible"
                duration: 300
            }
        },
        Transition {
            from: "hidden"
            to: "visible"
            ColorAnimation {
                properties: "color"
                easing.type: Easing.InOutCubic
                duration: 300
            }
        }
    ]

    ColumnLayout {
        Text {
            id: heading1
            text: ""
            font.family: "Helvetica"
            font.pixelSize: 3.0 * 16
            font.weight: Font.Light
            color: "black"

            Text {
                id: heading2
                anchors.top: heading1.top
                anchors.left: heading1.right
                text: ""
                font.family: "Helvetica"
                font.pixelSize: 3.0 * 16
                font.weight: Font.Light
                color: "#5caa15"
            }
        }

        Item {
            id: layoutHelper
            Layout.minimumHeight: 37
            Layout.preferredHeight: 37
            Layout.maximumHeight: 37
        }

        Text {
            id: description
            text: ""
            width: (infoSheet.width - infoSheet.anchors.leftMargin) * 0.3
            font.family: "Helvetica"
            font.pixelSize: 16
            font.weight: Font.Light
            lineHeight: 1.625 * 16
            lineHeightMode: Text.FixedHeight
        }
    }
}
