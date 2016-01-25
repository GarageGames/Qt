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

import "planets.js" as GLCode

Item {
    id: mainview
    width: 1280
    height: 768
    visible: true
    property int focusedPlanet: 100
    property int oldPlanet: 0
    property real xLookAtOffset: 0
    property real yLookAtOffset: 0
    property real zLookAtOffset: 0
    property real xCameraOffset: 0
    property real yCameraOffset: 0
    property real zCameraOffset: 0
    property real cameraNear: 0
    property int sliderLength: (width < height) ? width / 2 : height / 2
    property real textSize: (sliderLength < 320) ? (sliderLength / 20) : 16
    property real planetButtonSize: (height < 768) ? (height / 11) : 70

    NumberAnimation {
        id: lookAtOffsetAnimation
        target: mainview
        properties: "xLookAtOffset, yLookAtOffset, zLookAtOffset"
        to: 0
        easing.type: Easing.InOutQuint
        duration: 1250
    }

    NumberAnimation {
        id: cameraOffsetAnimation
        target: mainview
        properties: "xCameraOffset, yCameraOffset, zCameraOffset"
        to: 0
        easing.type: Easing.InOutQuint
        duration: 2500
    }

    Behavior on cameraNear {
        PropertyAnimation {
            easing.type: Easing.InOutQuint
            duration: 2500
        }
    }
    //! [1]
    onFocusedPlanetChanged: {
        if (focusedPlanet == 100) {
            info.opacity = 0;
            updatePlanetInfo();
        } else {
            updatePlanetInfo();
            info.opacity = 0.5;
        }

        GLCode.setOldPlanet();
        GLCode.setLookAtOffset();
        GLCode.setCameraOffset();

        lookAtOffsetAnimation.restart();
        cameraOffsetAnimation.restart();
    }
    //! [1]
    //! [0]
    Canvas3D {
        id: canvas3d
        anchors.fill: parent
        //! [4]
        onInitializeGL: {
            GLCode.initializeGL(canvas3d, eventSource, mainview);
        }
        //! [4]
        onPaintGL: {
            GLCode.paintGL(canvas3d);
            fpsDisplay.fps = canvas3d.fps;
        }

        onResizeGL: {
            GLCode.onResizeGL(canvas3d);
        }
        //! [3]
        ControlEventSource {
            anchors.fill: parent
            focus: true
            id: eventSource
        }
        //! [3]
    }
    //! [0]
    ListModel {
        id: planetModel

        ListElement {
            name: "Sun"
            radius: "109 x Earth"
            temperature: "5 778 K"
            orbitalPeriod: ""
            distance: ""
            planetImageSource: "images/sun.png"
            planetNumber: 0
        }
        ListElement {
            name: "Mercury"
            radius: "0.3829 x Earth"
            temperature: "80-700 K"
            orbitalPeriod: "87.969 d"
            distance: "0.387 098 AU"
            planetImageSource: "images/mercury.png"
            planetNumber: 1
        }
        ListElement {
            name: "Venus"
            radius: "0.9499 x Earth"
            temperature: "737 K"
            orbitalPeriod: "224.701 d"
            distance: "0.723 327 AU"
            planetImageSource: "images/venus.png"
            planetNumber: 2
        }
        ListElement {
            name: "Earth"
            radius: "6 378.1 km"
            temperature: "184-330 K"
            orbitalPeriod: "365.256 d"
            distance: "149598261 km (1 AU)"
            planetImageSource: "images/earth.png"
            planetNumber: 3
        }
        ListElement {
            name: "Mars"
            radius: "0.533 x Earth"
            temperature: "130-308 K"
            orbitalPeriod: "686.971 d"
            distance: "1.523679 AU"
            planetImageSource: "images/mars.png"
            planetNumber: 4
        }
        ListElement {
            name: "Jupiter"
            radius: "11.209 x Earth"
            temperature: "112-165 K"
            orbitalPeriod: "4332.59 d"
            distance: "5.204267 AU"
            planetImageSource: "images/jupiter.png"
            planetNumber: 5
        }
        ListElement {
            name: "Saturn"
            radius: "9.4492 x Earth"
            temperature: "84-134 K"
            orbitalPeriod: "10759.22 d"
            distance: "9.5820172 AU"
            planetImageSource: "images/saturn.png"
            planetNumber: 6
        }
        ListElement {
            name: "Uranus"
            radius: "4.007 x Earth"
            temperature: "49-76 K"
            orbitalPeriod: "30687.15 d"
            distance: "19.189253 AU"
            planetImageSource: "images/uranus.png"
            planetNumber: 7
        }
        ListElement {
            name: "Neptune"
            radius: "3.883 x Earth"
            temperature: "55-72 K"
            orbitalPeriod: "60190.03 d"
            distance: "30.070900 AU"
            planetImageSource: "images/neptune.png"
            planetNumber: 8
        }
        ListElement {
            name: "Solar System"
            planetImageSource: ""
            planetNumber: 100 // Defaults to solar system
        }
    }

    Component {
        id: planetButtonDelegate
        PlanetButton {
            source: planetImageSource
            text: name
            focusPlanet: planetNumber
            planetSelector: mainview
            buttonSize: planetButtonSize
            fontSize: textSize
        }
    }

    ListView {
        id: planetButtonView
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: planetButtonSize / 5
        anchors.bottomMargin: planetButtonSize / 7
        spacing: planetButtonSize / 7
        width: planetButtonSize * 1.4
        interactive: false
        model: planetModel
        delegate: planetButtonDelegate
    }

    InfoSheet {
        id: info
        width: 400
        anchors.right: planetButtonView.left
        anchors.rightMargin: 10
        opacity: 0.5

        // Set initial information for Solar System
        planet: "Solar System"
        exampleDetails: "This example shows a 3D model of the Solar</p>" +
                        "<p>System comprised of the Sun and the eight</p>" +
                        "<p>planets orbiting the Sun.</p></br>" +
                        "<p>The example is implemented using QtCanvas3D,</p>" +
                        "<p>three.js and _RingGeometry() method from</p>" +
                        "<p>threex.planets extension.</p></br>" +
                        "<p>The textures and images used in the example</p>" +
                        "<p>are Copyright (c) by James Hastings-Trew,</p>" +
                        "<a href=\"http://planetpixelemporium.com/planets.html\">" +
                        "http://planetpixelemporium.com/planets.html</a>"
    }

    function updatePlanetInfo() {

        info.width = 200;

        if (focusedPlanet !== 100) {
            info.planet = planetModel.get(focusedPlanet).name
            info.radius = planetModel.get(focusedPlanet).radius
            info.temperature = planetModel.get(focusedPlanet).temperature
            info.orbitalPeriod = planetModel.get(focusedPlanet).orbitalPeriod
            info.distance = planetModel.get(focusedPlanet).distance
        }
    }

    StyledSlider {
        id: speedSlider
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        width: sliderLength
        value: 0.2
        minimumValue: 0
        maximumValue: 1
        onValueChanged: GLCode.onSpeedChanged(value);
    }
    Text {
        anchors.right: speedSlider.left
        anchors.verticalCenter: speedSlider.verticalCenter
        anchors.rightMargin: 10
        font.family: "Helvetica"
        font.pixelSize: textSize
        font.weight: Font.Light
        color: "white"
        text: "Rotation Speed"
    }

    StyledSlider {
        id: scaleSlider
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        width: sliderLength
        value: 1200
        minimumValue: 1
        maximumValue: 2000
        onValueChanged: GLCode.setScale(value);
    }
    Text {
        anchors.right: scaleSlider.left
        anchors.verticalCenter: scaleSlider.verticalCenter
        anchors.rightMargin: 10
        font.family: "Helvetica"
        font.pixelSize: textSize
        font.weight: Font.Light
        color: "white"
        text: "Planet Size"
    }

    StyledSlider {
        id: distanceSlider
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        orientation: Qt.Vertical
        height: sliderLength
        value: 1
        minimumValue: 1
        maximumValue: 2
        //! [2]
        onValueChanged: GLCode.setCameraDistance(value);
        //! [2]
    }
    Text {
        y: distanceSlider.y + distanceSlider.height + width + 10
        x: distanceSlider.x + 30 - textSize
        transform: Rotation {
            origin.x: 0;
            origin.y: 0;
            angle: -90
        }
        font.family: "Helvetica"
        font.pixelSize: textSize
        font.weight: Font.Light
        color: "white"
        text: "Viewing Distance"
    }

    // FPS display, initially hidden, clicking will show it
    FpsDisplay {
        id: fpsDisplay
        anchors.left: parent.left
        anchors.top: parent.top
        width: 32
        height: 64
        hidden: true
    }
}
