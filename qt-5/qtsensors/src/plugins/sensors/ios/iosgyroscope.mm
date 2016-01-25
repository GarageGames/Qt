/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#include "iosmotionmanager.h"
#include "iosgyroscope.h"

char const * const IOSGyroscope::id("ios.gyroscope");

QT_BEGIN_NAMESPACE

IOSGyroscope::IOSGyroscope(QSensor *sensor)
    : QSensorBackend(sensor)
    , m_motionManager([QIOSMotionManager sharedManager])
    , m_timer(0)
{
    setReading<QGyroscopeReading>(&m_reading);
    addDataRate(1, 100); // 100Hz is max it seems
    addOutputRange(-360, 360, 0.01);
}

void IOSGyroscope::start()
{
    int hz = sensor()->dataRate();
    m_timer = startTimer(1000 / (hz == 0 ? 60 : hz));
    [m_motionManager startGyroUpdates];
}

void IOSGyroscope::stop()
{
    [m_motionManager stopGyroUpdates];
    killTimer(m_timer);
    m_timer = 0;
}

void IOSGyroscope::timerEvent(QTimerEvent *)
{
    // Convert NSTimeInterval to microseconds and radians to degrees:
    CMGyroData *data = m_motionManager.gyroData;
    CMRotationRate rate = data.rotationRate;
    // skip update if NaN
    if (rate.x != rate.x || rate.y != rate.y || rate.z != rate.z)
        return;
    m_reading.setTimestamp(quint64(data.timestamp * 1e6));
    m_reading.setX((qreal(rate.x) / M_PI) * 180);
    m_reading.setY((qreal(rate.y) / M_PI) * 180);
    m_reading.setZ((qreal(rate.z) / M_PI) * 180);
    newReadingAvailable();
}

QT_END_NAMESPACE
