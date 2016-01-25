/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include "osxbtconnectionmonitor_p.h"
#include "osxbtutility_p.h"

#include <QtCore/qdebug.h>

// Import, since these headers are not protected from the multiple inclusion.
#import <IOBluetooth/objc/IOBluetoothUserNotification.h>
#import <IOBluetooth/objc/IOBluetoothDevice.h>

QT_BEGIN_NAMESPACE

namespace OSXBluetooth {

ConnectionMonitor::~ConnectionMonitor()
{
}

}

QT_END_NAMESPACE

#ifdef QT_NAMESPACE
using namespace QT_NAMESPACE;
#endif

@implementation QT_MANGLE_NAMESPACE(OSXBTConnectionMonitor)

- (id)initWithMonitor:(OSXBluetooth::ConnectionMonitor *)aMonitor
{
    Q_ASSERT_X(aMonitor, "-initWithMonitor:", "invalid monitor (null)");

    if (self = [super init]) {
        monitor = aMonitor;
        discoveryNotification = [[IOBluetoothDevice registerForConnectNotifications:self
                                  selector:@selector(connectionNotification:withDevice:)] retain];
        foundConnections = [[NSMutableArray alloc] init];
    }

    return self;
}

- (void)dealloc
{
    [discoveryNotification unregister];
    [discoveryNotification release];

    for (IOBluetoothUserNotification *n in foundConnections)
        [n unregister];

    [foundConnections release];

    [super dealloc];
}

- (void)connectionNotification:(IOBluetoothUserNotification *)aNotification
        withDevice:(IOBluetoothDevice *)device
{
    Q_UNUSED(aNotification)

    typedef IOBluetoothUserNotification Notification;

    if (!device)
        return;

    QT_BT_MAC_AUTORELEASEPOOL;

    // All Obj-C objects are autoreleased.

    const QBluetoothAddress deviceAddress(OSXBluetooth::qt_address([device getAddress]));
    if (deviceAddress.isNull())
        return;

    if (foundConnections) {
        Notification *const notification = [device registerForDisconnectNotification:self
                                            selector: @selector(connectionClosedNotification:withDevice:)];
        if (notification)
            [foundConnections addObject:notification];
    }

    Q_ASSERT_X(monitor, "-connectionNotification:withDevice:", "invalid monitor (null)");
    monitor->deviceConnected(deviceAddress);
}

- (void)connectionClosedNotification:(IOBluetoothUserNotification *)notification
        withDevice:(IOBluetoothDevice *)device
{
    QT_BT_MAC_AUTORELEASEPOOL;

    [notification unregister];//?
    [foundConnections removeObject:notification];

    const QBluetoothAddress deviceAddress(OSXBluetooth::qt_address([device getAddress]));
    if (deviceAddress.isNull())
        return;

    Q_ASSERT_X(monitor, "-connectionClosedNotification:withDevice:", "invalid monitor (null)");
    monitor->deviceDisconnected(deviceAddress);
}

@end
