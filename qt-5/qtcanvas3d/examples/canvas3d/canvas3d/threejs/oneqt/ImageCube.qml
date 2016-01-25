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
import QtCanvas3D 1.0

import "imagecube.js" as GLCode

Canvas3D {
    id: cube
    //! [0]
    state: "image6"
    property color backgroundColor: "#FCFCFC"
    property real angleOffset: -180 / 8.0
    property string image1: ""
    //! [0]
    property string image2: ""
    property string image3: ""
    property string image4: ""
    property string image5: ""
    property string image6: ""
    property real xRotation: 0
    property real yRotation: 0
    property real zRotation: 0

    onBackgroundColorChanged: { GLCode.setBackgroundColor(cube.backgroundColor); }

    //! [1]
    states: [
        State {
            name: "image1"
            PropertyChanges { target: cube; xRotation: 0; }
            PropertyChanges { target: cube; yRotation: 180 * 1.5 + angleOffset; }
            PropertyChanges { target: cube; zRotation: 0 }
        },
        //! [1]
        State {
            name: "image2"
            PropertyChanges { target: cube; xRotation: 0; }
            PropertyChanges { target: cube; yRotation: 180 * 1.0 + angleOffset; }
            PropertyChanges { target: cube; zRotation: 0 }
        },
        State {
            name: "image3"
            PropertyChanges { target: cube; xRotation: 0; }
            PropertyChanges { target: cube; yRotation: 180 * 0.5 + angleOffset; }
            PropertyChanges { target: cube; zRotation: 0 }
        },
        State {
            name: "image4"
            PropertyChanges { target: cube; xRotation: 0; }
            PropertyChanges { target: cube; yRotation: 0 + angleOffset; }
            PropertyChanges { target: cube; zRotation: 0 }
        },
        State {
            name: "image5"
            PropertyChanges { target: cube; xRotation: 180 / 2.0; }
            PropertyChanges { target: cube; yRotation: 0; }
            PropertyChanges { target: cube; zRotation: -angleOffset; }
        },
        State {
            name: "image6"
            PropertyChanges { target: cube; xRotation: -180 / 2.0; }
            PropertyChanges { target: cube; yRotation: 0; }
            PropertyChanges { target: cube; zRotation: angleOffset; }
        }
    ]

    //! [2]
    transitions: [
        Transition {
            id: turnTransition
            from: "*"
            to: "*"
            RotationAnimation {
                properties: "xRotation,yRotation,zRotation"
                easing.type: Easing.InOutCubic
                direction: RotationAnimation.Shortest
                duration: 450
            }
        }
    ]
    //! [2]

    //! [3]
    onInitializeGL: {
        GLCode.initializeGL(cube);
    }

    onPaintGL: {
        GLCode.paintGL(cube);
    }

    onResizeGL: {
        GLCode.resizeGL(cube);
    }
    //! [3]

    SwipeArea {
        id: swipeArea
        anchors.fill: parent

        onSwipeRight: {
            if (cube.state === "image1")
                cube.state = "image4";
            else if (cube.state == "image2")
                cube.state = "image1";
            else if (cube.state == "image3")
                cube.state = "image2";
            else if (cube.state == "image4")
                cube.state = "image3";
            else if (cube.state == "image5")
                cube.state = "image3";
            else if (cube.state == "image6")
                cube.state = "image3";
        }

        onSwipeLeft: {
            if (cube.state === "image1")
                cube.state = "image2";
            else if (cube.state == "image2")
                cube.state = "image3";
            else if (cube.state == "image3")
                cube.state = "image4";
            else if (cube.state == "image4")
                cube.state = "image1";
            else if (cube.state == "image5")
                cube.state = "image1";
            else if (cube.state == "image6")
                cube.state = "image1";
        }

        onSwipeUp: {
            if (cube.state === "image1")
                cube.state = "image6";
            else if (cube.state == "image2")
                cube.state = "image6";
            else if (cube.state == "image3")
                cube.state = "image6";
            else if (cube.state == "image4")
                cube.state = "image6";
            else if (cube.state == "image5")
                cube.state = "image4";
            else if (cube.state == "image6")
                cube.state = "image2";
        }

        onSwipeDown: {
            if (cube.state === "image1")
                cube.state = "image5";
            else if (cube.state == "image2")
                cube.state = "image5";
            else if (cube.state == "image3")
                cube.state = "image5";
            else if (cube.state == "image4")
                cube.state = "image5";
            else if (cube.state == "image5")
                cube.state = "image2";
            else if (cube.state == "image6")
                cube.state = "image4";
        }
    }

}

