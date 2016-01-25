/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
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
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtTest 1.0
import QtLocation 5.3
import QtPositioning 5.2
import QtLocation.test 5.0


Item {
    id: page
    x: 0; y: 0;
    width: 120
    height: 120

    Plugin { id: errorPlugin;
            name: "qmlgeo.test.plugin"
            allowExperimental: true
            parameters: [
                PluginParameter { name: "error"; value: "1"},
                PluginParameter { name: "errorString"; value: "This error was expected. No worries !"}
            ]
        }

    Map {
        id: map;
        x: 0; y: 0; width: 100; height: 100; plugin: errorPlugin;
        center {
            latitude: 20
            longitude: 20
        }

        MouseArea {
            id: mouseArea
            objectName: "mouseArea"
            x: 0; y: 0; width: 100; height: 100
            preventStealing: true
        }
    }

    SignalSpy {id: mouseClickedSpy; target: mouseArea; signalName: "clicked"}

    TestCase {
        name: "MouseAreaWithoutInitializedMap"
        when: windowShown

        function test_basic_click() {
            wait(50);
            mouseClick(map, 5, 25)
            mouseClick(map, 5, 25)
            mouseClick(map, 5, 25)
            compare(mouseClickedSpy.count, 3)
        }
    }
}
