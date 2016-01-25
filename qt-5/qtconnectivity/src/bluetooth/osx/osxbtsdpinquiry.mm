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

#include "qbluetoothserviceinfo.h"
#include "osxbtsdpinquiry_p.h"
#include "qbluetoothuuid.h"
#include "osxbtutility_p.h"


#include <QtCore/qvariant.h>
#include <QtCore/qstring.h>

#include "corebluetoothwrapper_p.h"

QT_BEGIN_NAMESPACE

namespace OSXBluetooth {

SDPInquiryDelegate::~SDPInquiryDelegate()
{
}

QVariant extract_attribute_value(IOBluetoothSDPDataElement *dataElement)
{
    Q_ASSERT_X(dataElement, Q_FUNC_INFO, "invalid data element (nil)");

    // TODO: error handling and diagnostic messages.

    // All "temporary" obj-c objects are autoreleased.
    QT_BT_MAC_AUTORELEASEPOOL;

    const BluetoothSDPDataElementTypeDescriptor typeDescriptor = [dataElement getTypeDescriptor];

    switch (typeDescriptor) {
    case kBluetoothSDPDataElementTypeNil:
        break;
    case kBluetoothSDPDataElementTypeUnsignedInt:
        return [[dataElement getNumberValue] unsignedIntValue];
    case kBluetoothSDPDataElementTypeSignedInt:
        return [[dataElement getNumberValue] intValue];
    case kBluetoothSDPDataElementTypeUUID:
        return QVariant::fromValue(qt_uuid([[dataElement getUUIDValue] getUUIDWithLength:16]));
    case kBluetoothSDPDataElementTypeString:
    case kBluetoothSDPDataElementTypeURL:
        return QString::fromNSString([dataElement getStringValue]);
    case kBluetoothSDPDataElementTypeBoolean:
        return [[dataElement getNumberValue] boolValue];
    case kBluetoothSDPDataElementTypeDataElementSequence:
    case kBluetoothSDPDataElementTypeDataElementAlternative: // TODO: check this!
        {
        QBluetoothServiceInfo::Sequence sequence;
        NSArray *const arr = [dataElement getArrayValue];
        for (IOBluetoothSDPDataElement *element in arr)
            sequence.append(extract_attribute_value(element));

        return QVariant::fromValue(sequence);
        }
        break;// Coding style.
    default:;
    }

    return QVariant();
}

void extract_service_record(IOBluetoothSDPServiceRecord *record, QBluetoothServiceInfo &serviceInfo)
{
    QT_BT_MAC_AUTORELEASEPOOL;

    if (!record)
        return;

    NSDictionary *const attributes = record.attributes;
    NSEnumerator *const keys = attributes.keyEnumerator;
    for (NSNumber *key in keys) {
        const quint16 attributeID = [key unsignedShortValue];
        IOBluetoothSDPDataElement *const element = [attributes objectForKey:key];
        const QVariant attributeValue = OSXBluetooth::extract_attribute_value(element);
        serviceInfo.setAttribute(attributeID, attributeValue);
    }
}

QList<QBluetoothUuid> extract_services_uuids(IOBluetoothDevice *device)
{
    QList<QBluetoothUuid> uuids;

    // All "temporary" obj-c objects are autoreleased.
    QT_BT_MAC_AUTORELEASEPOOL;

    if (!device || !device.services)
        return uuids;

    NSArray * const records = device.services;
    for (IOBluetoothSDPServiceRecord *record in records) {
        IOBluetoothSDPDataElement *const element =
            [record getAttributeDataElement:kBluetoothSDPAttributeIdentifierServiceClassIDList];

        if (element && [element getTypeDescriptor] == kBluetoothSDPDataElementTypeUUID)
            uuids.append(qt_uuid([[element getUUIDValue] getUUIDWithLength:16]));
    }

    return uuids;
}

}

QT_END_NAMESPACE


#ifdef QT_NAMESPACE
using namespace QT_NAMESPACE;
#endif

using namespace OSXBluetooth;

@implementation QT_MANGLE_NAMESPACE(OSXBTSDPInquiry)

- (id)initWithDelegate:(SDPInquiryDelegate *)aDelegate
{
    Q_ASSERT_X(aDelegate, Q_FUNC_INFO, "invalid delegate (null)");

    if (self = [super init]) {
        delegate = aDelegate;
        isActive = false;
    }

    return self;
}

- (void)dealloc
{
    //[device closeConnection]; //??? - synchronous, "In the future this API will be changed to allow asynchronous operation."
    [device release];
    [super dealloc];
}

- (IOReturn)performSDPQueryWithDevice:(const QBluetoothAddress &)address
{
    Q_ASSERT_X(!isActive, Q_FUNC_INFO, "SDP query in progress");

    QList<QBluetoothUuid> emptyFilter;
    return [self performSDPQueryWithDevice:address filters:emptyFilter];
}

- (IOReturn)performSDPQueryWithDevice:(const QBluetoothAddress &)address
                              filters:(const QList<QBluetoothUuid> &)qtFilters
{
    Q_ASSERT_X(!isActive, Q_FUNC_INFO, "SDP query in progress");
    Q_ASSERT_X(!address.isNull(), Q_FUNC_INFO, "invalid target device address");

    QT_BT_MAC_AUTORELEASEPOOL;

    // We first try to allocate "filters":
    ObjCScopedPointer<NSMutableArray> array;
    if (qtFilters.size()) {
        array.reset([[NSMutableArray alloc] init]);
        if (!array) {
            qCCritical(QT_BT_OSX) << Q_FUNC_INFO << "failed to allocate an uuid filter";
            return kIOReturnError;
        }

        foreach (const QBluetoothUuid &qUuid, qtFilters) {
            ObjCStrongReference<IOBluetoothSDPUUID> uuid(iobluetooth_uuid(qUuid));
            if (uuid)
                [array addObject:uuid];
        }

        if (int([array count]) != qtFilters.size()) {
            qCCritical(QT_BT_OSX) << Q_FUNC_INFO << "failed to create an uuid filter";
            return kIOReturnError;
        }
    }

    const BluetoothDeviceAddress iobtAddress(iobluetooth_address(address));
    ObjCScopedPointer<IOBluetoothDevice> newDevice([[IOBluetoothDevice deviceWithAddress:&iobtAddress] retain]);
    if (!newDevice) {
        qCCritical(QT_BT_OSX) << Q_FUNC_INFO << "failed to create an IOBluetoothDevice object";
        return kIOReturnError;
    }

    ObjCScopedPointer<IOBluetoothDevice> oldDevice(device);
    device = newDevice.data();

    IOReturn result = kIOReturnSuccess;
    if (qtFilters.size())
        result = [device performSDPQuery:self uuids:array];
    else
        result = [device performSDPQuery:self];

    if (result != kIOReturnSuccess) {
        qCCritical(QT_BT_OSX) << Q_FUNC_INFO << "failed to start an SDP query";
        device = oldDevice.take();
    } else {
        isActive = true;
        newDevice.take();
    }

    return result;
}

- (void)stopSDPQuery
{
    // There is no API to stop it,
    // but there is a 'stop' member-function in Qt and
    // after it's called sdpQueryComplete must be somehow ignored.

    [device release];
    device = nil;
}

- (void)sdpQueryComplete:(IOBluetoothDevice *)aDevice status:(IOReturn)status
{
    // Can happen - there is no legal way to cancel an SDP query,
    // after the 'reset' device can never be
    // the same as the cancelled one.
    if (device != aDevice)
        return;

    Q_ASSERT_X(delegate, Q_FUNC_INFO, "invalid delegate (null)");

    isActive = false;

    if (status != kIOReturnSuccess)
        delegate->SDPInquiryError(aDevice, status);
    else
        delegate->SDPInquiryFinished(aDevice);
}

@end
