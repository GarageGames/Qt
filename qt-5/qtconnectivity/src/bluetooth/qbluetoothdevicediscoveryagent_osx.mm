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

#include "qbluetoothdevicediscoverytimer_osx_p.h"
#include "qbluetoothdevicediscoveryagent.h"
#include "osx/osxbtledeviceinquiry_p.h"
#include "osx/osxbtdeviceinquiry_p.h"
#include "qbluetoothlocaldevice.h"
#include "osx/osxbtsdpinquiry_p.h"
#include "qbluetoothdeviceinfo.h"
#include "osx/osxbtutility_p.h"
#include "osx/uistrings_p.h"
#include "qbluetoothhostinfo.h"
#include "qbluetoothuuid.h"

#include <QtCore/qloggingcategory.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qglobal.h>
#include <QtCore/qstring.h>
#include <QtCore/qdebug.h>
#include <QtCore/qlist.h>

#include <Foundation/Foundation.h>
// Only after Foundation.h:
#include "osx/corebluetoothwrapper_p.h"

QT_BEGIN_NAMESPACE

using OSXBluetooth::ObjCScopedPointer;

class QBluetoothDeviceDiscoveryAgentPrivate : public OSXBluetooth::DeviceInquiryDelegate,
                                              public OSXBluetooth::LEDeviceInquiryDelegate
{
    friend class QBluetoothDeviceDiscoveryAgent;
    friend class OSXBluetooth::DDATimerHandler;
public:
    QBluetoothDeviceDiscoveryAgentPrivate(const QBluetoothAddress & address,
                                          QBluetoothDeviceDiscoveryAgent *q);
    virtual ~QBluetoothDeviceDiscoveryAgentPrivate(); // Just to make compiler happy.

    bool isValid() const;
    bool isActive() const;

    void start();
    void startLE();
    void stop();

private:
    enum AgentState {
        NonActive,
        ClassicScan,
        LEScan
    };

    // DeviceInquiryDelegate:
    void inquiryFinished(IOBluetoothDeviceInquiry *inq) Q_DECL_OVERRIDE;
    void error(IOBluetoothDeviceInquiry *inq, IOReturn error) Q_DECL_OVERRIDE;
    void deviceFound(IOBluetoothDeviceInquiry *inq, IOBluetoothDevice *device) Q_DECL_OVERRIDE;
    // LEDeviceInquiryDelegate:
    void LEdeviceInquiryError(QBluetoothDeviceDiscoveryAgent::Error error) Q_DECL_OVERRIDE;
    void LEnotSupported() Q_DECL_OVERRIDE;
    void LEdeviceFound(CBPeripheral *peripheral, const QBluetoothUuid &deviceUuid,
                       NSDictionary *advertisementData, NSNumber *RSSI) Q_DECL_OVERRIDE;
    void LEdeviceInquiryFinished() Q_DECL_OVERRIDE;

    // Check if it's a really new device/updated info and emit
    // q_ptr->deviceDiscovered.
    void deviceFound(const QBluetoothDeviceInfo &newDeviceInfo);

    void setError(IOReturn error, const QString &text = QString());
    void setError(QBluetoothDeviceDiscoveryAgent::Error, const QString &text = QString());

    void checkLETimeout();

    QBluetoothDeviceDiscoveryAgent *q_ptr;
    AgentState agentState;

    QBluetoothAddress adapterAddress;

    bool startPending;
    bool stopPending;

    QBluetoothDeviceDiscoveryAgent::Error lastError;
    QString errorString;

    QBluetoothDeviceDiscoveryAgent::InquiryType inquiryType;

    typedef ObjCScopedPointer<DeviceInquiryObjC> DeviceInquiry;
    DeviceInquiry inquiry;

    typedef ObjCScopedPointer<LEDeviceInquiryObjC> LEDeviceInquiry;
    LEDeviceInquiry inquiryLE;

    typedef ObjCScopedPointer<IOBluetoothHostController> HostController;
    HostController hostController;

    typedef QList<QBluetoothDeviceInfo> DevicesList;
    DevicesList discoveredDevices;

    OSXBluetooth::DDATimerHandler timer;
};

namespace OSXBluetooth {

DDATimerHandler::DDATimerHandler(QBluetoothDeviceDiscoveryAgentPrivate *d)
    : owner(d)
{
    Q_ASSERT_X(owner, Q_FUNC_INFO, "invalid pointer");

    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, this, &DDATimerHandler::onTimer);
}

void DDATimerHandler::start(int msec)
{
    Q_ASSERT_X(msec > 0, Q_FUNC_INFO, "invalid time interval");
    if (timer.isActive()) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "timer is active";
        return;
    }

    timer.start(msec);
}

void DDATimerHandler::stop()
{
    timer.stop();
}

void DDATimerHandler::onTimer()
{
    Q_ASSERT(owner);
    owner->checkLETimeout();
}

}

QBluetoothDeviceDiscoveryAgentPrivate::QBluetoothDeviceDiscoveryAgentPrivate(const QBluetoothAddress &adapter,
                                                                             QBluetoothDeviceDiscoveryAgent *q) :
    q_ptr(q),
    agentState(NonActive),
    adapterAddress(adapter),
    startPending(false),
    stopPending(false),
    lastError(QBluetoothDeviceDiscoveryAgent::NoError),
    inquiryType(QBluetoothDeviceDiscoveryAgent::GeneralUnlimitedInquiry),
    timer(this)
{
    Q_ASSERT_X(q != Q_NULLPTR, Q_FUNC_INFO, "invalid q_ptr (null)");

    HostController controller([[IOBluetoothHostController defaultController] retain]);
    if (!controller || [controller powerState] != kBluetoothHCIPowerStateON) {
        qCCritical(QT_BT_OSX) << Q_FUNC_INFO << "no default host "
                                 "controller or adapter is off";
        return;
    }

    DeviceInquiry newInquiry([[DeviceInquiryObjC alloc]initWithDelegate:this]);
    if (!newInquiry) { // Obj-C's way of "reporting errors":
        qCCritical(QT_BT_OSX) << Q_FUNC_INFO << "failed to "
                                 "initialize an inquiry";
        return;
    }

    // OSXBTLEDeviceInquiry can be constructed even if LE is not supported -
    // at this stage it's only a memory allocation of the object itself,
    // if it fails - we have some memory-related problem.
    LEDeviceInquiry newInquiryLE([[LEDeviceInquiryObjC alloc] initWithDelegate:this]);
    if (!newInquiryLE) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "failed to "
                                "initialize a LE inquiry";
        return;
    }

    hostController.reset(controller.take());
    inquiry.reset(newInquiry.take());
    inquiryLE.reset(newInquiryLE.take());
}

QBluetoothDeviceDiscoveryAgentPrivate::~QBluetoothDeviceDiscoveryAgentPrivate()
{
}

bool QBluetoothDeviceDiscoveryAgentPrivate::isValid() const
{
    // isValid() - Qt does not use exceptions, but the ctor
    // can fail to initialize some important data-members
    // (and the error is probably not even related to Bluetooth at all)
    // - say, allocation error - this is what meant here by valid/invalid.
    return hostController && [hostController powerState] == kBluetoothHCIPowerStateON
           && inquiry && inquiryLE;
}

bool QBluetoothDeviceDiscoveryAgentPrivate::isActive() const
{
    if (startPending)
        return true;

    if (stopPending)
        return false;

    return agentState != NonActive;
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

    agentState = ClassicScan;

    discoveredDevices.clear();
    setError(QBluetoothDeviceDiscoveryAgent::NoError);

    const IOReturn res = [inquiry start];
    if (res != kIOReturnSuccess) {
        setError(res, QCoreApplication::translate(DEV_DISCOVERY, DD_NOT_STARTED));
        agentState = NonActive;
        emit q_ptr->error(lastError);
    }
}

void QBluetoothDeviceDiscoveryAgentPrivate::startLE()
{
    Q_ASSERT_X(isValid(), Q_FUNC_INFO, "called on invalid device discovery agent");
    Q_ASSERT_X(lastError != QBluetoothDeviceDiscoveryAgent::InvalidBluetoothAdapterError,
               Q_FUNC_INFO, "called with an invalid Bluetooth adapter");

    agentState = LEScan;

    // CoreBluetooth does not have a timeout. We start a timer here
    // and check if scan really started and if yes if we have a timeout.
    timer.start([LEDeviceInquiryObjC inquiryLength]);

    if (![inquiryLE start]) {
        // We can be here only if we have some kind of resource allocation error, so we
        // do not emit finished, we emit error.
        timer.stop();
        setError(QBluetoothDeviceDiscoveryAgent::UnknownError,
                 QCoreApplication::translate(DEV_DISCOVERY, DD_NOT_STARTED_LE));
        agentState = NonActive;
        emit q_ptr->error(lastError);
    }
}

void QBluetoothDeviceDiscoveryAgentPrivate::stop()
{
    Q_ASSERT_X(isValid(), Q_FUNC_INFO, "called on invalid device discovery agent");
    Q_ASSERT_X(isActive(), Q_FUNC_INFO, "called whithout active inquiry");
    Q_ASSERT_X(lastError != QBluetoothDeviceDiscoveryAgent::InvalidBluetoothAdapterError,
               Q_FUNC_INFO, "called with invalid bluetooth adapter");

    const bool prevStart = startPending;
    startPending = false;
    stopPending = true;

    setError(QBluetoothDeviceDiscoveryAgent::NoError);

    if (agentState == ClassicScan) {
        const IOReturn res = [inquiry stop];
        if (res != kIOReturnSuccess) {
            qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "failed to stop";
            startPending = prevStart;
            stopPending = false;
            setError(res, QCoreApplication::translate(DEV_DISCOVERY, DD_NOT_STOPPED));
            emit q_ptr->error(lastError);
        }
    } else {
        // Can be asynchronous (depending on a status update of CBCentralManager).
        // The call itself is always 'success'.
        [inquiryLE stop];
    }
}

void QBluetoothDeviceDiscoveryAgentPrivate::inquiryFinished(IOBluetoothDeviceInquiry *inq)
{
    Q_UNUSED(inq)

    Q_ASSERT_X(isValid(), Q_FUNC_INFO, "invalid device discovery agent"); //We can never be here.

    // The subsequent start(LE) function (if any)
    // will (re)set the correct state.
    agentState = NonActive;

    if (stopPending && !startPending) {
        stopPending = false;
        emit q_ptr->canceled();
    } else if (startPending) {
        startPending = false;
        stopPending = false;
        start();
    } else {
        // We can be here _only_ if a classic scan
        // finished in a normal way (not cancelled).
        // startLE() will take care of old devices
        // not supporting Bluetooth 4.0.
        startLE();
    }
}

void QBluetoothDeviceDiscoveryAgentPrivate::error(IOBluetoothDeviceInquiry *inq, IOReturn error)
{
    Q_UNUSED(inq)

    Q_ASSERT_X(isValid(), Q_FUNC_INFO, "invalid device discovery agent");

    startPending = false;
    stopPending = false;

    setError(error);

    emit q_ptr->error(lastError);
}

void QBluetoothDeviceDiscoveryAgentPrivate::deviceFound(IOBluetoothDeviceInquiry *inq, IOBluetoothDevice *device)
{
    Q_UNUSED(inq)

    Q_ASSERT_X(isValid(), Q_FUNC_INFO, "invalid device discovery agent");
    Q_ASSERT_X(device, Q_FUNC_INFO, "invalid IOBluetoothDevice (nil)");
    Q_ASSERT_X(agentState == ClassicScan, Q_FUNC_INFO,
               "invalid agent state (expected classic scan)");

    QT_BT_MAC_AUTORELEASEPOOL;

    // Let's collect some info about this device:
    const QBluetoothAddress deviceAddress(OSXBluetooth::qt_address([device getAddress]));
    if (deviceAddress.isNull()) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "invalid Bluetooth address";
        return;
    }

    QString deviceName;
    if (device.name)
        deviceName = QString::fromNSString(device.name);

    const qint32 classOfDevice(device.classOfDevice);

    QBluetoothDeviceInfo deviceInfo(deviceAddress, deviceName, classOfDevice);
    deviceInfo.setCoreConfigurations(QBluetoothDeviceInfo::BaseRateCoreConfiguration);
    deviceInfo.setRssi(device.RSSI);

    const QList<QBluetoothUuid> uuids(OSXBluetooth::extract_services_uuids(device));
    deviceInfo.setServiceUuids(uuids, QBluetoothDeviceInfo::DataIncomplete);

    deviceFound(deviceInfo);
}

void QBluetoothDeviceDiscoveryAgentPrivate::setError(IOReturn error, const QString &text)
{
    if (error == kIOReturnSuccess)
        setError(QBluetoothDeviceDiscoveryAgent::NoError, text);
    else if (error == kIOReturnNoPower)
        setError(QBluetoothDeviceDiscoveryAgent::PoweredOffError, text);
    else
        setError(QBluetoothDeviceDiscoveryAgent::UnknownError, text);
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
        case QBluetoothDeviceDiscoveryAgent::UnknownError:
        default:
            errorString = QCoreApplication::translate(DEV_DISCOVERY, DD_UNKNOWN_ERROR);
        }
    }

    if (lastError != QBluetoothDeviceDiscoveryAgent::NoError)
        qCDebug(QT_BT_OSX) << "error set: "<<errorString;
}

void QBluetoothDeviceDiscoveryAgentPrivate::checkLETimeout()
{
    Q_ASSERT_X(agentState == LEScan, Q_FUNC_INFO, "invalid agent state");
    Q_ASSERT_X(inquiryLE, Q_FUNC_INFO, "LE device inquiry is nil");

    const int timeout = [LEDeviceInquiryObjC inquiryLength];
    Q_ASSERT(timeout > 0);
    const QTime scanStartTime([inquiryLE startTime]);
    if (scanStartTime.isValid()) {
        const int elapsed = scanStartTime.msecsTo(QTime::currentTime());
        Q_ASSERT(elapsed >= 0);
        if (elapsed >= timeout)
            [inquiryLE stop];
        else
            timer.start(timeout - elapsed);
    } else {
        // Scan not started yet. Wait 5 seconds more.
        timer.start(timeout / 2);
    }
}

void QBluetoothDeviceDiscoveryAgentPrivate::LEdeviceInquiryError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    // At the moment the only error reported can be 'powered off' error, it happens
    // after the LE scan started (so we have LE support and this is a real PoweredOffError).
    Q_ASSERT_X(error == QBluetoothDeviceDiscoveryAgent::PoweredOffError,
               Q_FUNC_INFO, "unexpected error code");

    timer.stop();

    agentState = NonActive;
    setError(error);
    emit q_ptr->error(lastError);
}

void QBluetoothDeviceDiscoveryAgentPrivate::LEnotSupported()
{
    // Not supported is not an error.
    qCDebug(QT_BT_OSX) << "no Bluetooth LE support";
    // After we call startLE and before receive NotSupported,
    // the user can call stop (setting a pending stop).
    // So the same rule apply:
    timer.stop();

    LEdeviceInquiryFinished();
}

void QBluetoothDeviceDiscoveryAgentPrivate::LEdeviceFound(CBPeripheral *peripheral, const QBluetoothUuid &deviceUuid,
                                                          NSDictionary *advertisementData,
                                                          NSNumber *RSSI)
{
    Q_ASSERT_X(peripheral, Q_FUNC_INFO, "invalid peripheral (nil)");
    Q_ASSERT_X(agentState == LEScan, Q_FUNC_INFO,
               "invalid agent state, expected LE scan");

    Q_UNUSED(advertisementData)

    QString name;
    if (peripheral.name)
        name = QString::fromNSString(peripheral.name);

    // TODO: fix 'classOfDevice' (0 for now).
    QBluetoothDeviceInfo newDeviceInfo(deviceUuid, name, 0);
    if (RSSI)
        newDeviceInfo.setRssi([RSSI shortValue]);
    // CoreBluetooth scans only for LE devices.
    newDeviceInfo.setCoreConfigurations(QBluetoothDeviceInfo::LowEnergyCoreConfiguration);

    deviceFound(newDeviceInfo);
}

void QBluetoothDeviceDiscoveryAgentPrivate::LEdeviceInquiryFinished()
{
    // The same logic as in inquiryFinished, but does not start LE scan.
    agentState = NonActive;

    timer.stop();

    if (stopPending && !startPending) {
        stopPending = false;
        emit q_ptr->canceled();
    } else if (startPending) {
        startPending = false;
        stopPending = false;
        start(); //Start from a classic scan again.
    } else {
        emit q_ptr->finished();
    }
}

void QBluetoothDeviceDiscoveryAgentPrivate::deviceFound(const QBluetoothDeviceInfo &newDeviceInfo)
{
    // Core Bluetooth does not allow us to access addresses, we have to use uuid instead.
    // This uuid has nothing to do with uuids in Bluetooth in general (it's generated by
    // Apple's framework using some algorithm), but it's a 128-bit uuid after all.
    const bool isLE = newDeviceInfo.coreConfigurations() == QBluetoothDeviceInfo::LowEnergyCoreConfiguration;

    for (int i = 0, e = discoveredDevices.size(); i < e; ++i) {
        if (isLE ? discoveredDevices[i].deviceUuid() == newDeviceInfo.deviceUuid():
                   discoveredDevices[i].address() == newDeviceInfo.address()) {
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
        const QList<QBluetoothHostInfo> localDevices = QBluetoothLocalDevice::allDevices();
        foreach (const QBluetoothHostInfo &hostInfo, localDevices) {
            if (hostInfo.address() == deviceAdapter)
                return;
        }
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
        } else {
            // We previously failed to initialize d_ptr correctly:
            // either some memory allocation problem or
            // no BT adapter found.
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
