/****************************************************************************
**
** Copyright (C) 2012 Lorn Potter
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#include <UIKit/UIAccelerometer.h>

#include "iosaccelerometer.h"
#include "iosmotionmanager.h"

char const * const IOSAccelerometer::id("ios.accelerometer");

QT_BEGIN_NAMESPACE

IOSAccelerometer::IOSAccelerometer(QSensor *sensor)
    : QSensorBackend(sensor)
    , m_motionManager([QIOSMotionManager sharedManager])
    , m_timer(0)
{
    setReading<QAccelerometerReading>(&m_reading);
    addDataRate(1, 100); // 100Hz
    addOutputRange(-22.418, 22.418, 0.17651); // 2G
}

void IOSAccelerometer::start()
{
    int hz = sensor()->dataRate();
    m_timer = startTimer(1000 / (hz == 0 ? 60 : hz));
    [m_motionManager startAccelerometerUpdates];
}

void IOSAccelerometer::stop()
{
    [m_motionManager stopAccelerometerUpdates];
    killTimer(m_timer);
    m_timer = 0;
}

void IOSAccelerometer::timerEvent(QTimerEvent *)
{
    // Convert from NSTimeInterval to microseconds and G to m/s2, and flip axes:
    CMAccelerometerData *data = m_motionManager.accelerometerData;
    CMAcceleration acc = data.acceleration;
    // skip update if NaN
    if (acc.x != acc.x || acc.y != acc.y || acc.z != acc.z)
        return;
    static const qreal G = 9.8066;
    m_reading.setTimestamp(quint64(data.timestamp * 1e6));
    m_reading.setX(qreal(acc.x) * G * -1);
    m_reading.setY(qreal(acc.y) * G * -1);
    m_reading.setZ(qreal(acc.z) * G * -1);
    newReadingAvailable();
}

QT_END_NAMESPACE
