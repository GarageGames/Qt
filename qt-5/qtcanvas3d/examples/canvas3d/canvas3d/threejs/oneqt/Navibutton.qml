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

Text {
    id: menubarItem
    text: ""
    font.family: "Helvetica"
    font.pixelSize: 1.125 * 16
    //font.pointSize: 20
    font.weight: Font.Light
    color: "#404244"
    Layout.alignment: Qt.AlignHCenter
    state: "default"
    verticalAlignment: Text.AlignVCenter
    property bool selected: false
    property string stateSelect: ""
    property Item stateTarget

    states: [
        State {
            name: "default"
            when: !menubarItem.selected
            PropertyChanges { target: menubarItem; color: "#404244"; }
            PropertyChanges { target: selectedHighlight; height: 0; }
        },
        State {
            name: "mouseover"
            PropertyChanges { target: menubarItem; color: "#5caa15"; }
            PropertyChanges { target: selectedHighlight; height: 4; }
        },
        State {
            name: "selected"
            when: menubarItem.selected
            PropertyChanges { target: menubarItem; color: "#5caa15"; }
            PropertyChanges { target: selectedHighlight; height: 4; }
        }
    ]

    onStateChanged: {
        if (state == "selected") {
            menubarItem.stateTarget.state = menubarItem.stateSelect;
        }
    }

    transitions: [
        Transition {
            from: "*"
            to: "*"
            ColorAnimation {
                properties: "color"
                easing.type: Easing.InOutCubic
                duration: 250
            }
        }
    ]

    Rectangle {
        id: selectedHighlight
        color: "#7cc54d"
        width: parent.width
        anchors.bottom: parent.bottom
        height: 0

        Behavior on height {

                    NumberAnimation {
                        easing.type: Easing.InOutCubic
                        duration: 250
                    }
                }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true

        onPressed: {
            menubarItem.state = "mouseover";
        }

        onReleased: {
            if (menubarItem.state == "mouseover") {
                menubarItem.selected = true;
            }
        }

        onEntered: {
            menubarItem.state = "mouseover";
        }

        onExited: {
            if (menubarItem.selected) {
                menubarItem.state = "selected";
            } else {
                menubarItem.state = "default";
            }
        }
    }
}

