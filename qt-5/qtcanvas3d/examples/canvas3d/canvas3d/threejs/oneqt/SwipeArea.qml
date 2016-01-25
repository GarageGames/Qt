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

MouseArea {
    id: mouseSwipeArea
    preventStealing: true

    property real prevX: 0
    property real prevY: 0
    property real velocityX: 0.0
    property real velocityY: 0.0
    property int startX: 0
    property int startY: 0
    property bool tracing: false

    signal swipeLeft()
    signal swipeRight()
    signal swipeUp()
    signal swipeDown()

    onPressed: {
        startX = mouse.x
        startY = mouse.y
        prevX = mouse.x
        prevY = mouse.y
        velocityX = 0
        velocityY = 0
        tracing = true
    }

    onPositionChanged: {
        if ( !tracing ) return
        var currVelX = (mouse.x-prevX)
        var currVelY = (mouse.y-prevY)

        velocityX = (velocityX + currVelX)/2.0;
        velocityY = (velocityY + currVelY)/2.0;

        prevX = mouse.x
        prevY = mouse.y

        if ( velocityX > 15 && mouse.x > mouseSwipeArea.width * 0.25 ) {
            tracing = false
            // Swipe Right
            mouseSwipeArea.swipeRight()
        } else if ( velocityX < -15 && mouse.x < mouseSwipeArea.width * 0.75 ) {
            tracing = false
            // Swipe Left
            mouseSwipeArea.swipeLeft()
        } else if (velocityY > 15 && mouse.y > mouseSwipeArea.height * 0.25 ) {
            tracing = false
            // Swipe Down
            mouseSwipeArea.swipeDown()
        } else if ( velocityY < -15 && mouse.y < mouseSwipeArea.height * 0.75 ) {
            tracing = false
            // Swipe Up
            mouseSwipeArea.swipeUp()
        }
    }
}
