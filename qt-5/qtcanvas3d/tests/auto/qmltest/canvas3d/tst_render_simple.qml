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

    Canvas3D {
        id: render_simple
        property bool heightChanged: false
        property bool widthChanged: false
        property bool initOk: false
        property bool renderOk: false
        anchors.fill: parent
        onInitializeGL: initOk = Content.initializeGL(render_simple)
        onPaintGL: {
            Content.paintGL(render_simple)
            renderOk = true
        }
        onHeightChanged: heightChanged = true
        onWidthChanged: widthChanged = true
    }

    TestCase {
        name: "Canvas3D_render_simple"
        when: windowShown

        function test_render_1_simple() {
            waitForRendering(render_simple)
            tryCompare(render_simple, "initOk", true)
            tryCompare(render_simple, "renderOk", true)
        }

        function test_render_2_resize() {
            render_simple.heightChanged = false
            render_simple.widthChanged = false
            render_simple.renderOk = false
            top.height = 200
            waitForRendering(render_simple)
            verify(render_simple.heightChanged === true)
            verify(render_simple.widthChanged === false)
            tryCompare(render_simple, "renderOk", true)

            render_simple.renderOk = false
            top.width = 200
            waitForRendering(render_simple)
            verify(render_simple.heightChanged === true)
            verify(render_simple.widthChanged === true)
            tryCompare(render_simple, "renderOk", true)
        }
    }
}
