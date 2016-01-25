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

import "tst_render_simple.js" as Content

Item {
    id: top
    height: 300
    width: 300

    property var canvas3d: null
    property var activeContent: Content
    property bool initOk: false
    property bool renderOk: false

    function createCanvas() {
        canvas3d = Qt.createQmlObject("
        import QtQuick 2.2
        import QtCanvas3D 1.0
        Canvas3D {
            onInitializeGL: initOk = activeContent.initializeGL(canvas3d)
            onPaintGL: {
                renderOk = true
                activeContent.paintGL(canvas3d)
            }
        }", top)
        canvas3d.anchors.fill = top
    }

    TestCase {
        name: "Canvas3D_render_dynamic"
        when: windowShown

        function test_render_1_dynamic_creation() {
            verify(canvas3d === null)
            verify(initOk === false)
            verify(renderOk === false)
            createCanvas()
            verify(canvas3d !== null)
            waitForRendering(canvas3d)
            tryCompare(top, "initOk", true)
            tryCompare(top, "renderOk", true)
        }

        function test_render_2_dynamic_deletion() {
            verify(canvas3d !== null)
            verify(initOk === true)
            verify(renderOk === true)
            canvas3d.destroy()
            waitForRendering(canvas3d)
            verify(canvas3d === null)
        }

        function test_render_3_dynamic_recreation() {
            initOk = false
            renderOk = false
            createCanvas()
            tryCompare(top, "initOk", true)
            tryCompare(top, "renderOk", true)
            waitForRendering(canvas3d)
            verify(canvas3d !== null)

            canvas3d.destroy()
            waitForRendering(canvas3d)
            verify(canvas3d === null)

            initOk = false
            renderOk = false
            createCanvas()
            waitForRendering(canvas3d)
            verify(canvas3d !== null)
            tryCompare(top, "initOk", true)
            tryCompare(top, "renderOk", true)
        }
    }
}
