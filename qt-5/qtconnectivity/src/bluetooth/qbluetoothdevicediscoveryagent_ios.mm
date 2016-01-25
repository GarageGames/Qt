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

#include "qbluetoothdevicediscoveryagent.h"
#include "osx/osxbtledeviceinquiry_p.h"
#include "qbluetoothlocaldevice.h"
#include "qbluetoothdeviceinfo.h"
#include "osx/osxbtutility_p.h"
#include "osx/uistrings_p.h"
#include "qbluetoothuuid.h"

#include <QtCore/qloggingcategory.h>
#include <QtCore/qglobal.h>
#include <QtCore/qstring.h>
#include <QtCore/qdebug.h>
#include <QtCore/qlist.h>

#include <CoreBluetooth/CoreBluetooth.h>

QT_BEGIN_NAMESPACE

using OSXBluetooth::ObjCScopedPointer;

class QBluetoothDeviceDiscoveryAgentPrivate : public OSXBluetooth::LEDeviceInquiryDelegate
{
    friend class QBluetoothDeviceDiscoveryAgent;
public:
    QBluetoothDeviceDiscoveryAgentPrivate(const QBluetoothAddress &address,
                                          QBluetoothDeviceDiscoveryAgent *q);
    virtual ~QBluetoothDeviceDiscoveryAgentPrivate();

    bool isValid() const;
    bool isActive() const;

    void start();
    void stop();

private:
    // LEDeviceInquiryDelegate:
    void LEdeviceInquiryError(QBluetoothDeviceDiscoveryAgent::Error error) Q_DECL_OVERRIDE;
    void LEnotSupported() Q_DECL_OVERRIDE;
    void LEdeviceFound(CBPeripheral *peripheral, const QBluetoothUuid &deviceUuid,
                       NSDictionary *advertisementData, NSNumber *RSSI) Q_DECL_OVERRIDE;
    void LEdeviceInquiryFinished() Q_DECL_OVERRIDE;

    void setError(QBluetoothDeviceDiscoveryAgent::Error, const QString &text = QString());

    QBluetoothDeviceDiscoveryAgent *q_ptr;

    QBluetoothDeviceDiscoveryAgent::Error lastError;
    QString errorString;

    QBluetoothDeviceDiscoveryAgent::InquiryType inquiryType;

    typedef ObjCScopedPointer<LEDeviceInquiryObjC> LEDeviceInquiry;
    LEDeviceInquiry inquiryLE;

    typedef QList<QBluetoothDeviceInfo> DevicesList;
    DevicesList discoveredDevices;

    bool startPending;
    bool stopPending;
};

QBluetoothDeviceDiscoveryAgentPrivate::QBluetoothDeviceDiscoveryAgentPrivate(const QBluetoothAddress &adapter,
                                                                             QBluetoothDeviceDiscoveryAgent *q) :
    q_ptr(q),
    lastError(QBluetoothDeviceDiscoveryAgent::NoError),
    inquiryType(QBluetoothDeviceDiscoveryAgent::GeneralUnlimitedInquiry),
    startPending(false),
    stopPending(false)
{
    Q_UNUSED(adapter);

    Q_ASSERT_X(q != Q_NULLPTR, Q_FUNC_INFO, "invalid q_ptr (null)");

    // OSXBTLEDeviceInquiry can be constructed even if LE is not supported -
    // at this stage it's only a memory allocation of the object itself,
    // if it fails - we have some memory-related problems.
    LEDeviceInquiry newInquiryLE([[LEDeviceInquiryObjC alloc] initWithDelegate:this]);
    if (!newInquiryLE) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "failed to initialize a device inquiry object";
        return;
    }

    inquiryLE.reset(newInquiryLE.take());
}

QBluetoothDeviceDiscoveryAgentPrivate::~QBluetoothDeviceDiscoveryAgentPrivate()
{
}

bool QBluetoothDeviceDiscoveryAgentPrivate::isValid() const
{
    // isValid() - Qt does not use exceptions, but the ctor
    // can fail to initialize some important data-members
    // - this is what meant here by valid/invalid.
    return inquiryLE;
}

bool QBluetoothDeviceDiscoveryAgentPrivate::isActive() const
{
    if (startPending)
        return true;
    if (stopPending)
        return false;

    return [inquiryLE isActive];
}

void QBluetoothDeviceDiscoveryAgentPrivate::start()
{
    Q_ASSERT_X(isValid(), Q_FUNC_INFO, "called on invalid device discovery agent");
    Q_ASSERT_X(!isActive(), Q_FUNC_INFO, "called on active device discovery agent");
    Q_ASSERT_X(lastError != QBluetoothDeviceDiscoveryAgent::InvalidBluetoothAdapterError,
               Q_FUNC_INFO, "called with an invalid Bluetooth adapter");

    if (stopPending) {
        startPending = true;
        return;
    }

    discoveredDevices.clear();
    setError(QBluetoothDeviceDiscoveryAgent::NoError);

    if (![inquiryLE start]) {
        // We can be here only if we have some kind of
        // resource allocation error.
        setError(QBluetoothDeviceDiscoveryAgent::UnknownError,
                 QCoreApplication::translate(DEV_DISCOVERY, DD_NOT_STARTED));
        emit q_ptr->error(lastError);
    }
}

void QBluetoothDeviceDiscoveryAgentPrivate::stop()
{
    Q_ASSERT_X(isValid(), Q_FUNC_INFO, "called on invalid device discovery agent");
    Q_ASSERT_X(isActive(), Q_FUNC_INFO, "called whithout active inquiry");
    Q_ASSERT_X(lastError != QBluetoothDeviceDiscoveryAgent::InvalidBluetoothAdapterError,
               Q_FUNC_INFO, "called with invalid bluetooth adapter");

    startPending = false;
    stopPending = true;

    setError(QBluetoothDeviceDiscoveryAgent::NoError);
    // Can be asynchronous (depending on a status update of CBCentralManager).
    // The call itself is always 'success'.
    [inquiryLE stop];
}

void QBluetoothDeviceDiscoveryAgentPrivate::LEdeviceInquiryError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    // At the moment the only error reported by osxbtledeviceinquiry
    // can be 'powered off' error, it happens
    // after the LE scan started (so we have LE support and this is
    // a real PoweredOffError).
    Q_ASSERT_X(error == QBluetoothDeviceDiscoveryAgent::PoweredOffError,
               Q_FUNC_INFO, "unexpected error");

    startPending = false;
    stopPending = false;
    setError(error);
    emit q_ptr->error(lastError);
}

void QBluetoothDeviceDiscoveryAgentPrivate::LEnotSupported()
{
    startPending = false;
    stopPending = false;
    setError(QBluetoothDeviceDiscoveryAgent::UnsupportedPlatformError);
    emit q_ptr->error(lastError);
}

void QBluetoothDeviceDiscoveryAgentPrivate::LEdeviceFound(CBPeripheral *peripheral, const QBluetoothUuid &deviceUuid,
                                                          NSDictionary *advertisementData,
                                                          NSNumber *RSSI)
{
    Q_ASSERT_X(peripheral, Q_FUNC_INFO, "invalid peripheral (nil)");

    QT_BT_MAC_AUTORELEASEPOOL;

    QString name;
    if (peripheral.name && peripheral.name.length) {
        name = QString::fromNSString(peripheral.name);
    } else {
        NSString *const localName = [advertisementData objectForKey:CBAdvertisementDataLocalNameKey];
        if (localName && [localName length])
            name = QString::fromNSString(localName);
    }

    // TODO: fix 'classOfDevice' (0 for now).
    QBluetoothDeviceInfo newDeviceInfo(deviceUuid, name, 0);
    if (RSSI)
        newDeviceInfo.setRssi([RSSI shortValue]);
    // CoreBluetooth scans only for LE devices.
    newDeviceInfo.setCoreConfigurations(QBluetoothDeviceInfo::LowEnergyCoreConfiguration);

    // Update, append or discard.
    for (int i = 0, e = discoveredDevices.size(); i < e; ++i) {
        if (discoveredDevices[i].deviceUuid() == newDeviceInfo.deviceUuid()) {
            if (discoveredDevices[i] == newDeviceInfo)
                return;

            discoveredDevices.replace(i, newDeviceInfo);
            emit q_ptr->deviceDiscovered(newDeviceInfo);
            return;
        }
    }

    discoveredDevices.append(newDeviceInfo);
    emit q_ptr->deviceDiscovered(newDeviceInfo);
}

void QBluetoothDeviceDiscoveryAgentPrivate::LEdeviceInquiryFinished()
{
    Q_ASSERT_X(isValid(), Q_FUNC_INFO, "invalid device discovery agent");

    if (stopPending && !startPending) {
        stopPending = false;
        emit q_ptr->canceled();
    } else if (startPending) {
        startPending = false;
        stopPending = false;
        start();
    } else {
        emit q_ptr->finished();
    }
}

void QBluetoothDeviceDiscoveryAgentPrivate::setError(QBluetoothDeviceDiscoveryAgent::Error error,
                                                     const QString &text)
{
    lastError = error;

    if (text.length() > 0) {
        errorString = text;
    } else {
        switch (lastError) {
        case QBluetoothDeviceDiscoveryAgent::NoError:
            errorString = QString();
            break;
        case QBluetoothDeviceDiscoveryAgent::PoweredOffError:
            errorString = QCoreApplication::translate(DEV_DISCOVERY, DD_POWERED_OFF);
            break;
        case QBluetoothDeviceDiscoveryAgent::InvalidBluetoothAdapterError:
            errorString = QCoreApplication::translate(DEV_DISCOVERY, DD_INVALID_ADAPTER);
            break;
        case QBluetoothDeviceDiscoveryAgent::InputOutputError:
            errorString = QCoreApplication::translate(DEV_DISCOVERY, DD_IO);
            break;
        case QBluetoothDeviceDiscoveryAgent::UnsupportedPlatformError:
            errorString = QCoreApplication::translate(DEV_DISCOVERY, DD_NOTSUPPORTED);
            break;
        case QBluetoothDeviceDiscoveryAgent::UnknownError:
        default:
            errorString = QCoreApplication::translate(DEV_DISCOVERY, DD_UNKNOWN_ERROR);
        }
    }
}

QBluetoothDeviceDiscoveryAgent::QBluetoothDeviceDiscoveryAgent(QObject *parent) :
    QObject(parent),
    d_ptr(new QBluetoothDeviceDiscoveryAgentPrivate(QBluetoothAddress(), this))
{
}

QBluetoothDeviceDiscoveryAgent::QBluetoothDeviceDiscoveryAgent(
    const QBluetoothAddress &deviceAdapter, QObject *parent) :
    QObject(parent),
    d_ptr(new QBluetoothDeviceDiscoveryAgentPrivate(deviceAdapter, this))
{
    if (!deviceAdapter.isNull()) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "local device address is "
                                "not available, provided address is ignored";
        d_ptr->setError(InvalidBluetoothAdapterError);
    }
}

QBluetoothDeviceDiscoveryAgent::~QBluetoothDeviceDiscoveryAgent()
{
    delete d_ptr;
}

QBluetoothDeviceDiscoveryAgent::InquiryType QBluetoothDeviceDiscoveryAgent::inquiryType() const
{
    return d_ptr->inquiryType;
}

void QBluetoothDeviceDiscoveryAgent::setInquiryType(QBluetoothDeviceDiscoveryAgent::InquiryType type)
{
    d_ptr->inquiryType = type;
}

QList<QBluetoothDeviceInfo> QBluetoothDeviceDiscoveryAgent::discoveredDevices() const
{
    return d_ptr->discoveredDevices;
}

void QBluetoothDeviceDiscoveryAgent::start()
{
    if (d_ptr->lastError != InvalidBluetoothAdapterError) {
        if (d_ptr->isValid()) {
            if (!isActive())
                d_ptr->start();
            else
                qCDebug(QT_BT_OSX) << Q_FUNC_INFO << "already started";
        } else {
            // We previously failed to initialize
            // private object correctly.
            d_ptr->setError(InvalidBluetoothAdapterError);
            emit error(InvalidBluetoothAdapterError);
        }
    }
}

void QBluetoothDeviceDiscoveryAgent::stop()
{
    if (d_ptr->isValid()) {
        if (isActive() && d_ptr->lastError != InvalidBluetoothAdapterError)
            d_ptr->stop();
        else
            qCDebug(QT_BT_OSX) << Q_FUNC_INFO << "failed to stop";
    }
}

bool QBluetoothDeviceDiscoveryAgent::isActive() const
{
    if (d_ptr->isValid())
        return d_ptr->isActive();

    return false;
}

QBluetoothDeviceDiscoveryAgent::Error QBluetoothDeviceDiscoveryAgent::error() const
{
    return d_ptr->lastError;
}

QString QBluetoothDeviceDiscoveryAgent::errorString() const
{
    return d_ptr->errorString;
}

QT_END_NAMESPACE
