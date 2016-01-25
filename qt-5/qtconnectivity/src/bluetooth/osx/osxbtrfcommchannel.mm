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

#include "osxbtchanneldelegate_p.h"
#include "osxbtrfcommchannel_p.h"
#include "qbluetoothaddress.h"
#include "osxbtutility_p.h"

#ifdef QT_NAMESPACE
using namespace QT_NAMESPACE;
#endif

@implementation QT_MANGLE_NAMESPACE(OSXBTRFCOMMChannel)

- (id)initWithDelegate:(OSXBluetooth::ChannelDelegate *)aDelegate
{
    Q_ASSERT_X(aDelegate, Q_FUNC_INFO, "invalid delegate (null)");

    if (self = [super init]) {
        delegate = aDelegate;
        device = nil;
        channel = nil;
        connected = false;
    }

    return self;
}

- (id)initWithDelegate:(QT_PREPEND_NAMESPACE(OSXBluetooth::ChannelDelegate) *)aDelegate
      channel:(IOBluetoothRFCOMMChannel *)aChannel
{
    // This type of channel does not require connect, it's created with
    // already open channel.
    Q_ASSERT_X(aDelegate, Q_FUNC_INFO, "invalid delegate (null)");
    Q_ASSERT_X(aChannel, Q_FUNC_INFO, "invalid channel (nil)");

    if (self = [super init]) {
        delegate = aDelegate;
        channel = [aChannel retain];
        [channel setDelegate:self];
        device = [[channel getDevice] retain];
        connected = true;
    }

    return self;
}

- (void)dealloc
{
    if (channel) {
        [channel setDelegate:nil];
        [channel closeChannel];
        [channel release];
    }

    [device release];

    [super dealloc];
}

// A single async connection (you can not reuse this object).
- (IOReturn)connectAsyncToDevice:(const QBluetoothAddress &)address
            withChannelID:(BluetoothRFCOMMChannelID)channelID
{
    if (address.isNull()) {
        qCCritical(QT_BT_OSX) << Q_FUNC_INFO << "invalid peer address";
        return kIOReturnNoDevice;
    }

    // Can never be called twice.
    if (connected || device || channel) {
        qCCritical(QT_BT_OSX) << Q_FUNC_INFO << "connection is already active";
        return kIOReturnStillOpen;
    }

    QT_BT_MAC_AUTORELEASEPOOL;

    const BluetoothDeviceAddress iobtAddress = OSXBluetooth::iobluetooth_address(address);
    device = [IOBluetoothDevice deviceWithAddress:&iobtAddress];
    if (!device) { // TODO: do I always check this BTW??? Apple's docs say nothing about nil.
        qCCritical(QT_BT_OSX) << Q_FUNC_INFO << "failed to create a device";
        return kIOReturnNoDevice;
    }

    const IOReturn status = [device openRFCOMMChannelAsync:&channel
                             withChannelID:channelID delegate:self];
    if (status != kIOReturnSuccess) {
        qCCritical(QT_BT_OSX) << Q_FUNC_INFO << "failed to open L2CAP channel";
        // device is still autoreleased.
        device = nil;
        return status;
    }

    [channel retain];// What if we're closed already?
    [device retain];

    return kIOReturnSuccess;
}

- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel*)rfcommChannel
        data:(void *)dataPointer length:(size_t)dataLength
{
    Q_UNUSED(rfcommChannel)

    Q_ASSERT_X(delegate, Q_FUNC_INFO, "invalid delegate (null)");

    // Not sure if it can ever happen and if
    // assert is better.
    if (!dataPointer || !dataLength)
        return;

    delegate->readChannelData(dataPointer, dataLength);
}

- (void)rfcommChannelOpenComplete:(IOBluetoothRFCOMMChannel*)rfcommChannel
        status:(IOReturn)error
{
    Q_UNUSED(rfcommChannel)

    Q_ASSERT_X(delegate, Q_FUNC_INFO, "invalid delegate (null)");

    if (error != kIOReturnSuccess) {
        delegate->setChannelError(error);
    } else {
        connected = true;
        delegate->channelOpenComplete();
    }
}

- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel*)rfcommChannel
{
    Q_UNUSED(rfcommChannel)

    Q_ASSERT_X(delegate, Q_FUNC_INFO, "invalid delegate (null)");
    delegate->channelClosed();
    connected = false;
}

- (void)rfcommChannelControlSignalsChanged:(IOBluetoothRFCOMMChannel*)rfcommChannel
{
    Q_UNUSED(rfcommChannel)
}

- (void)rfcommChannelFlowControlChanged:(IOBluetoothRFCOMMChannel*)rfcommChannel
{
    Q_UNUSED(rfcommChannel)
}

- (void)rfcommChannelWriteComplete:(IOBluetoothRFCOMMChannel*)rfcommChannel
        refcon:(void*)refcon status:(IOReturn)error
{
    Q_UNUSED(rfcommChannel)
    Q_UNUSED(refcon)

    Q_ASSERT_X(delegate, Q_FUNC_INFO, "invalid delegate (null)");

    if (error != kIOReturnSuccess)
        delegate->setChannelError(error);
    else
        delegate->writeComplete();
}

- (void)rfcommChannelQueueSpaceAvailable:(IOBluetoothRFCOMMChannel*)rfcommChannel
{
    Q_UNUSED(rfcommChannel)
}

- (BluetoothRFCOMMChannelID)getChannelID
{
    if (channel)
        return [channel getChannelID];

    return 0;
}

- (BluetoothDeviceAddress)peerAddress
{
    const BluetoothDeviceAddress *const addr = device ? [device getAddress]
                                                      : Q_NULLPTR;
    if (addr)
        return *addr;

    return BluetoothDeviceAddress();
}

- (NSString *)peerName
{
    if (device)
        return device.name;

    return nil;
}

- (BluetoothRFCOMMMTU)getMTU
{
    if (channel)
        return [channel getMTU];

    return 0;
}

- (IOReturn) writeSync:(void*)data length:(UInt16)length
{
    Q_ASSERT_X(data, Q_FUNC_INFO, "invalid data (null)");
    Q_ASSERT_X(length, Q_FUNC_INFO, "invalid data size");
    Q_ASSERT_X(connected && channel, Q_FUNC_INFO, "invalid RFCOMM channel");

    return [channel writeSync:data length:length];
}

- (IOReturn) writeAsync:(void*)data length:(UInt16)length
{
    Q_ASSERT_X(data, Q_FUNC_INFO, "invalid data (null)");
    Q_ASSERT_X(length, Q_FUNC_INFO, "invalid data size");
    Q_ASSERT_X(connected && channel, Q_FUNC_INFO, "invalid RFCOMM channel");

    return [channel writeAsync:data length:length refcon:Q_NULLPTR];
}


@end
