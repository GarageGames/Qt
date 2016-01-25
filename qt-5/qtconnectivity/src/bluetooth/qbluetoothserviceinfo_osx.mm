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

#include "osx/osxbtservicerecord_p.h"
#include "qbluetoothserver_osx_p.h"
#include "qbluetoothserviceinfo.h"
#include "qbluetoothdeviceinfo.h"
#include "osx/osxbtutility_p.h"

#include <QtCore/qloggingcategory.h>
#include <QtCore/qvariant.h>
#include <QtCore/qsysinfo.h>
#include <QtCore/qglobal.h>
#include <QtCore/qmutex.h>
#include <QtCore/qmap.h>
#include <QtCore/qurl.h>

#include <Foundation/Foundation.h>
// Only after Foundation.h:
#include "osx/corebluetoothwrapper_p.h"

QT_BEGIN_NAMESPACE

namespace {

// This is not in osxbtutility_p, since it's not required
// in general and just fixes the problem with SDK < 10.9,
// where we have to care about about IOBluetoothSDPServiceRecordRef.
class ServiceRecordRefGuard
{
public:
    ServiceRecordRefGuard()
        : recordRef(Q_NULLPTR)
    {
    }
    explicit ServiceRecordRefGuard(IOBluetoothSDPServiceRecordRef r)
        : recordRef(r)
    {
    }
    ~ServiceRecordRefGuard()
    {
        if (recordRef) // Requires non-NULL pointers.
            CFRelease(recordRef);
    }

    void reset(IOBluetoothSDPServiceRecordRef r)
    {
        if (recordRef)
            CFRelease(recordRef);
        // Take the ownership:
        recordRef = r;
    }

private:
    IOBluetoothSDPServiceRecordRef recordRef;

    Q_DISABLE_COPY(ServiceRecordRefGuard)
};

}

class QBluetoothServiceInfoPrivate
{
public:
    typedef QBluetoothServiceInfo QSInfo;
    QBluetoothServiceInfoPrivate(QBluetoothServiceInfo *q);

    bool registerService(const QBluetoothAddress &localAdapter = QBluetoothAddress());

    bool isRegistered() const;

    bool unregisterService();

    QBluetoothDeviceInfo deviceInfo;
    QMap<quint16, QVariant> attributes;

    QBluetoothServiceInfo::Sequence protocolDescriptor(QBluetoothUuid::ProtocolUuid protocol) const;
    int serverChannel() const;

private:
    QBluetoothServiceInfo *q_ptr;
    bool registered;

    typedef OSXBluetooth::ObjCScopedPointer<IOBluetoothSDPServiceRecord> SDPRecord;
    SDPRecord serviceRecord;
    BluetoothSDPServiceRecordHandle serviceRecordHandle;
};

QBluetoothServiceInfoPrivate::QBluetoothServiceInfoPrivate(QBluetoothServiceInfo *q)
                                 : q_ptr(q),
                                   registered(false),
                                   serviceRecordHandle(0)
{
    Q_ASSERT_X(q, Q_FUNC_INFO, "invalid q_ptr (null)");
}

bool QBluetoothServiceInfoPrivate::registerService(const QBluetoothAddress &localAdapter)
{
    Q_UNUSED(localAdapter)

    if (registered)
        return false;

    Q_ASSERT_X(!serviceRecord, Q_FUNC_INFO, "not registered, but serviceRecord is not nil");

    using namespace OSXBluetooth;

    ObjCStrongReference<NSMutableDictionary>
        serviceDict(iobluetooth_service_dictionary(*q_ptr));

    if (!serviceDict) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "failed to create a service dictionary";
        return false;
    }

    ServiceRecordRefGuard refGuard;
    SDPRecord newRecord;
#if QT_OSX_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_9)
    if (QSysInfo::MacintoshVersion >= QSysInfo::MV_10_9) {
        newRecord.reset([[IOBluetoothSDPServiceRecord
                          publishedServiceRecordWithDictionary:serviceDict] retain]);
    } else {
#else
    {
#endif
        IOBluetoothSDPServiceRecordRef recordRef = Q_NULLPTR;
        // With ARC this will require a different cast?
        const IOReturn status = IOBluetoothAddServiceDict((CFDictionaryRef)serviceDict.data(), &recordRef);
        if (status != kIOReturnSuccess) {
            qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "failed to register a service record";
            return false;
        }

        refGuard.reset(recordRef);
        newRecord.reset([[IOBluetoothSDPServiceRecord withSDPServiceRecordRef:recordRef] retain]);
        // It's weird, but ... it's not possible to release a record ref yet.
    }

    if (!newRecord) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "failed to register a service record";
        // In case of SDK < 10.9 it's not possible to remove a service record ...
        // no way to obtain record handle yet.
        return false;
    }

    BluetoothSDPServiceRecordHandle newRecordHandle = 0;
    if ([newRecord getServiceRecordHandle:&newRecordHandle] != kIOReturnSuccess) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "failed to register a service record";
#if QT_OSX_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_9)
        if (QSysInfo::MacintoshVersion >= QSysInfo::MV_10_9)
            [newRecord removeServiceRecord];
#endif
        // With SDK < 10.9 there is no way to unregister at this point ...
        return false;
    }

    const QSInfo::Protocol type = q_ptr->socketProtocol();
    quint16 realPort = 0;
    QBluetoothServerPrivate *server = Q_NULLPTR;
    bool configured = false;

    if (type == QBluetoothServiceInfo::L2capProtocol) {
        BluetoothL2CAPPSM psm = 0;
        server = QBluetoothServerPrivate::registeredServer(q_ptr->protocolServiceMultiplexer(), type);
        if ([newRecord getL2CAPPSM:&psm] == kIOReturnSuccess) {
            configured = true;
            realPort = psm;
        }
    } else if (type == QBluetoothServiceInfo::RfcommProtocol) {
        BluetoothRFCOMMChannelID channelID = 0;
        server = QBluetoothServerPrivate::registeredServer(q_ptr->serverChannel(), type);
        if ([newRecord getRFCOMMChannelID:&channelID] == kIOReturnSuccess) {
            configured = true;
            realPort = channelID;
        }
    }

    if (!configured) {
#if QT_OSX_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_9)
        if (QSysInfo::MacintoshVersion >= QSysInfo::MV_10_9) {
            [newRecord removeServiceRecord];
        } else {
#else
        {// Just to balance braces ...
#endif
            IOBluetoothRemoveServiceWithRecordHandle(newRecordHandle);
        }

        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "failed to register a service record";
        return false;
    }

    registered = true;
    serviceRecord.reset(newRecord.take());
    serviceRecordHandle = newRecordHandle;

    if (server)
        server->startListener(realPort);

    return true;
}

bool QBluetoothServiceInfoPrivate::isRegistered() const
{
    return registered;
}

bool QBluetoothServiceInfoPrivate::unregisterService()
{
    if (!registered)
        return false;

    Q_ASSERT_X(serviceRecord, Q_FUNC_INFO, "service registered, but serviceRecord is nil");

#if QT_OSX_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_9)
    if (QSysInfo::MacintoshVersion >= QSysInfo::MV_10_9) {
        [serviceRecord removeServiceRecord];
    } else {
#else
    {
#endif
    // Assert on newRecordHandle? Is 0 a valid/invalid handle?
        IOBluetoothRemoveServiceWithRecordHandle(serviceRecordHandle);
    }

    serviceRecord.reset(nil);

    const QSInfo::Protocol type = q_ptr->socketProtocol();
    QBluetoothServerPrivate *server = Q_NULLPTR;

    const QMutexLocker lock(&QBluetoothServerPrivate::channelMapMutex());
    if (type == QSInfo::RfcommProtocol)
        server = QBluetoothServerPrivate::registeredServer(q_ptr->serverChannel(), type);
    else if (type == QSInfo::L2capProtocol)
        server = QBluetoothServerPrivate::registeredServer(q_ptr->protocolServiceMultiplexer(), type);

    if (server)
        server->stopListener();

    registered = false;
    serviceRecordHandle = 0;

    return true;
}

bool QBluetoothServiceInfo::isRegistered() const
{
    return d_ptr->isRegistered();
}

bool QBluetoothServiceInfo::registerService(const QBluetoothAddress &localAdapter)
{
    return d_ptr->registerService(localAdapter);
}

bool QBluetoothServiceInfo::unregisterService()
{
    return d_ptr->unregisterService();
}

QBluetoothServiceInfo::QBluetoothServiceInfo()
    : d_ptr(QSharedPointer<QBluetoothServiceInfoPrivate>(new QBluetoothServiceInfoPrivate(this)))
{
}

QBluetoothServiceInfo::QBluetoothServiceInfo(const QBluetoothServiceInfo &other)
    : d_ptr(other.d_ptr)
{
}

QBluetoothServiceInfo::~QBluetoothServiceInfo()
{
}

bool QBluetoothServiceInfo::isValid() const
{
    return !d_ptr->attributes.isEmpty();
}

bool QBluetoothServiceInfo::isComplete() const
{
    return d_ptr->attributes.keys().contains(ProtocolDescriptorList);
}

QBluetoothDeviceInfo QBluetoothServiceInfo::device() const
{
    return d_ptr->deviceInfo;
}

void QBluetoothServiceInfo::setDevice(const QBluetoothDeviceInfo &device)
{
    d_ptr->deviceInfo = device;
}

void QBluetoothServiceInfo::setAttribute(quint16 attributeId, const QVariant &value)
{
    d_ptr->attributes[attributeId] = value;
}

QVariant QBluetoothServiceInfo::attribute(quint16 attributeId) const
{
    return d_ptr->attributes.value(attributeId);
}

QList<quint16> QBluetoothServiceInfo::attributes() const
{
    return d_ptr->attributes.keys();
}

bool QBluetoothServiceInfo::contains(quint16 attributeId) const
{
    return d_ptr->attributes.contains(attributeId);
}

void QBluetoothServiceInfo::removeAttribute(quint16 attributeId)
{
    d_ptr->attributes.remove(attributeId);
}

QBluetoothServiceInfo::Protocol QBluetoothServiceInfo::socketProtocol() const
{
    QBluetoothServiceInfo::Sequence parameters = protocolDescriptor(QBluetoothUuid::Rfcomm);
    if (!parameters.isEmpty())
        return RfcommProtocol;

    parameters = protocolDescriptor(QBluetoothUuid::L2cap);
    if (!parameters.isEmpty())
        return L2capProtocol;

    return UnknownProtocol;
}

int QBluetoothServiceInfo::protocolServiceMultiplexer() const
{
    QBluetoothServiceInfo::Sequence parameters = protocolDescriptor(QBluetoothUuid::L2cap);

    if (parameters.isEmpty())
        return -1;
    else if (parameters.count() == 1)
        return 0;
    else
        return parameters.at(1).toUInt();
}

int QBluetoothServiceInfo::serverChannel() const
{
    return d_ptr->serverChannel();
}

QBluetoothServiceInfo::Sequence QBluetoothServiceInfo::protocolDescriptor(QBluetoothUuid::ProtocolUuid protocol) const
{
    return d_ptr->protocolDescriptor(protocol);
}

QList<QBluetoothUuid> QBluetoothServiceInfo::serviceClassUuids() const
{
    QList<QBluetoothUuid> results;

    const QVariant var = attribute(QBluetoothServiceInfo::ServiceClassIds);
    if (!var.isValid())
        return results;

    const QBluetoothServiceInfo::Sequence seq = var.value<QBluetoothServiceInfo::Sequence>();
    for (int i = 0; i < seq.count(); i++)
        results.append(seq.at(i).value<QBluetoothUuid>());

    return results;
}

QBluetoothServiceInfo &QBluetoothServiceInfo::operator=(const QBluetoothServiceInfo &other)
{
    d_ptr = other.d_ptr;

    return *this;
}

static void dumpAttributeVariant(const QVariant &var, const QString indent)
{
    switch (int(var.type())) {
    case QMetaType::Void:
        qDebug("%sEmpty", indent.toLocal8Bit().constData());
        break;
    case QMetaType::UChar:
        qDebug("%suchar %u", indent.toLocal8Bit().constData(), var.toUInt());
        break;
    case QMetaType::UShort:
        qDebug("%sushort %u", indent.toLocal8Bit().constData(), var.toUInt());
    case QMetaType::UInt:
        qDebug("%suint %u", indent.toLocal8Bit().constData(), var.toUInt());
        break;
    case QMetaType::Char:
        qDebug("%schar %d", indent.toLocal8Bit().constData(), var.toInt());
        break;
    case QMetaType::Short:
        qDebug("%sshort %d", indent.toLocal8Bit().constData(), var.toInt());
        break;
    case QMetaType::Int:
        qDebug("%sint %d", indent.toLocal8Bit().constData(), var.toInt());
        break;
    case QMetaType::QString:
        qDebug("%sstring %s", indent.toLocal8Bit().constData(), var.toString().toLocal8Bit().constData());
        break;
    case QMetaType::Bool:
        qDebug("%sbool %d", indent.toLocal8Bit().constData(), var.toBool());
        break;
    case QMetaType::QUrl:
        qDebug("%surl %s", indent.toLocal8Bit().constData(), var.toUrl().toString().toLocal8Bit().constData());
        break;
    case QVariant::UserType:
        if (var.userType() == qMetaTypeId<QBluetoothUuid>()) {
            QBluetoothUuid uuid = var.value<QBluetoothUuid>();
            switch (uuid.minimumSize()) {
            case 0:
                qDebug("%suuid NULL", indent.toLocal8Bit().constData());
                break;
            case 2:
                qDebug("%suuid %04x", indent.toLocal8Bit().constData(), uuid.toUInt16());
                break;
            case 4:
                qDebug("%suuid %08x", indent.toLocal8Bit().constData(), uuid.toUInt32());
                break;
            case 16:
                qDebug("%suuid %s", indent.toLocal8Bit().constData(), QByteArray(reinterpret_cast<const char *>(uuid.toUInt128().data), 16).toHex().constData());
                break;
            default:
                qDebug("%suuid ???", indent.toLocal8Bit().constData());
                ;
            }
        } else if (var.userType() == qMetaTypeId<QBluetoothServiceInfo::Sequence>()) {
            qDebug("%sSequence", indent.toLocal8Bit().constData());
            const QBluetoothServiceInfo::Sequence *sequence = static_cast<const QBluetoothServiceInfo::Sequence *>(var.data());
            foreach (const QVariant &v, *sequence)
                dumpAttributeVariant(v, indent + QLatin1Char('\t'));
        } else if (var.userType() == qMetaTypeId<QBluetoothServiceInfo::Alternative>()) {
            qDebug("%sAlternative", indent.toLocal8Bit().constData());
            const QBluetoothServiceInfo::Alternative *alternative = static_cast<const QBluetoothServiceInfo::Alternative *>(var.data());
            foreach (const QVariant &v, *alternative)
                dumpAttributeVariant(v, indent + QLatin1Char('\t'));
        }
        break;
    default:
        qDebug("%sunknown variant type %d", indent.toLocal8Bit().constData(), var.userType());
    }
}

QDebug operator << (QDebug dbg, const QBluetoothServiceInfo &info)
{
    foreach (quint16 id, info.attributes()) {
        dumpAttributeVariant(info.attribute(id), QString::fromLatin1("(%1)\t").arg(id));
    }
    return dbg;
}

QBluetoothServiceInfo::Sequence QBluetoothServiceInfoPrivate::protocolDescriptor(QBluetoothUuid::ProtocolUuid protocol) const
{
    if (!attributes.contains(QBluetoothServiceInfo::ProtocolDescriptorList))
        return QBluetoothServiceInfo::Sequence();

    foreach (const QVariant &v, attributes.value(QBluetoothServiceInfo::ProtocolDescriptorList).value<QBluetoothServiceInfo::Sequence>()) {
        QBluetoothServiceInfo::Sequence parameters = v.value<QBluetoothServiceInfo::Sequence>();
        if (parameters.empty())
            continue;
        if (parameters.at(0).userType() == qMetaTypeId<QBluetoothUuid>()) {
            if (parameters.at(0).value<QBluetoothUuid>() == protocol)
                return parameters;
        }
    }

    return QBluetoothServiceInfo::Sequence();
}

int QBluetoothServiceInfoPrivate::serverChannel() const
{
    QBluetoothServiceInfo::Sequence parameters = protocolDescriptor(QBluetoothUuid::Rfcomm);

    if (parameters.isEmpty())
        return -1;
    else if (parameters.count() == 1)
        return 0;
    else
        return parameters.at(1).toUInt();
}

QT_END_NAMESPACE
