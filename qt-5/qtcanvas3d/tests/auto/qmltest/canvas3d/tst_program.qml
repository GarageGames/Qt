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

import QtQuick 2.2
import QtCanvas3D 1.0
import QtTest 1.0

import "tst_program.js" as Content

Item {
    id: top
    height: 300
    width: 300

    property var canvas3d: null
    property var activeContent: Content

    function createCanvas() {
        canvas3d = Qt.createQmlObject("
        import QtQuick 2.2
        import QtCanvas3D 1.0
        Canvas3D {
            property bool heightChanged: false
            property bool widthChanged: false
            property int initStatus: -1
            property int renderStatus: -1

            anchors.fill: parent
            onInitializeGL: initStatus = activeContent.initializeGL(canvas3d)
            onPaintGL: {
                renderStatus = activeContent.paintGL(canvas3d)
            }
            onHeightChanged: heightChanged = true
            onWidthChanged: widthChanged = true
        }", top)
        canvas3d.anchors.fill = top
    }

    TestCase {
        name: "Canvas3D_test_program_and_shaders"
        when: windowShown

        function test_program_and_shaders() {
            createCanvas()
            verify(canvas3d !== null)
            waitForRendering(canvas3d)
            tryCompare(canvas3d, "initStatus", 0)
            tryCompare(canvas3d, "renderStatus", 0)
        }
    }
}
