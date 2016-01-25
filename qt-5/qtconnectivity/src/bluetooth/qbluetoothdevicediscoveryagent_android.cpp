/****************************************************************************
**
** Copyright (C) 2013 Lauri Laanmets (Proekspert AS) <lauri.laanmets@eesti.ee>
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

#include "qbluetoothdevicediscoveryagent_p.h"
#include <QtCore/QLoggingCategory>
#include <QtBluetooth/QBluetoothAddress>
#include <QtBluetooth/QBluetoothDeviceInfo>
#include <QtCore/private/qjnihelpers_p.h>
#include "android/devicediscoverybroadcastreceiver_p.h"
#include <QtAndroidExtras/QAndroidJniEnvironment>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_ANDROID)

enum {
    NoScanActive = 0,
    SDPScanActive = 1,
    BtleScanActive = 2
};

QBluetoothDeviceDiscoveryAgentPrivate::QBluetoothDeviceDiscoveryAgentPrivate(
    const QBluetoothAddress &deviceAdapter, QBluetoothDeviceDiscoveryAgent *parent) :
    inquiryType(QBluetoothDeviceDiscoveryAgent::GeneralUnlimitedInquiry),
    lastError(QBluetoothDeviceDiscoveryAgent::NoError),
    receiver(0),
    m_adapterAddress(deviceAdapter),
    m_active(NoScanActive),
    leScanTimeout(0),
    pendingCancel(false),
    pendingStart(false),
    q_ptr(parent)
{
    adapter = QAndroidJniObject::callStaticObjectMethod("android/bluetooth/BluetoothAdapter",
                                                        "getDefaultAdapter",
                                                        "()Landroid/bluetooth/BluetoothAdapter;");
}

QBluetoothDeviceDiscoveryAgentPrivate::~QBluetoothDeviceDiscoveryAgentPrivate()
{
    if (m_active != NoScanActive)
        stop();

    if (receiver) {
        receiver->unregisterReceiver();
        delete receiver;
    }
}

bool QBluetoothDeviceDiscoveryAgentPrivate::isActive() const
{
    if (pendingStart)
        return true;
    if (pendingCancel)
        return false;
    return m_active != NoScanActive;
}

void QBluetoothDeviceDiscoveryAgentPrivate::start()
{
    if (pendingCancel) {
        pendingStart = true;
        return;
    }

    Q_Q(QBluetoothDeviceDiscoveryAgent);

    if (!adapter.isValid()) {
        qCWarning(QT_BT_ANDROID) << "Device does not support Bluetooth";
        lastError = QBluetoothDeviceDiscoveryAgent::InputOutputError;
        errorString = QBluetoothDeviceDiscoveryAgent::tr("Device does not support Bluetooth");
        emit q->error(lastError);
        return;
    }

    if (!m_adapterAddress.isNull()
        && adapter.callObjectMethod<jstring>("getAddress").toString()
        != m_adapterAddress.toString()) {
        qCWarning(QT_BT_ANDROID) << "Incorrect local adapter passed.";
        lastError = QBluetoothDeviceDiscoveryAgent::InvalidBluetoothAdapterError;
        errorString = QBluetoothDeviceDiscoveryAgent::tr("Passed address is not a local device.");
        emit q->error(lastError);
        return;
    }

    const int state = adapter.callMethod<jint>("getState");
    if (state != 12) {  // BluetoothAdapter.STATE_ON
        lastError = QBluetoothDeviceDiscoveryAgent::PoweredOffError;
        errorString = QBluetoothDeviceDiscoveryAgent::tr("Device is powered off");
        emit q->error(lastError);
        return;
    }

    // install Java BroadcastReceiver
    if (!receiver) {
        // SDP based device discovery
        receiver = new DeviceDiscoveryBroadcastReceiver();
        qRegisterMetaType<QBluetoothDeviceInfo>();
        QObject::connect(receiver, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo,bool)),
                         this, SLOT(processDiscoveredDevices(QBluetoothDeviceInfo,bool)));
        QObject::connect(receiver, SIGNAL(finished()), this, SLOT(processSdpDiscoveryFinished()));
    }

    discoveredDevices.clear();

    const bool success = adapter.callMethod<jboolean>("startDiscovery");
    if (!success) {
        lastError = QBluetoothDeviceDiscoveryAgent::InputOutputError;
        errorString = QBluetoothDeviceDiscoveryAgent::tr("Discovery cannot be started");
        emit q->error(lastError);
        return;
    }

    m_active = SDPScanActive;

    qCDebug(QT_BT_ANDROID)
        << "QBluetoothDeviceDiscoveryAgentPrivate::start() - successfully executed.";
}

void QBluetoothDeviceDiscoveryAgentPrivate::stop()
{
    Q_Q(QBluetoothDeviceDiscoveryAgent);

    if (m_active == NoScanActive)
        return;

    if (m_active == SDPScanActive) {
        pendingCancel = true;
        pendingStart = false;
        bool success = adapter.callMethod<jboolean>("cancelDiscovery");
        if (!success) {
            lastError = QBluetoothDeviceDiscoveryAgent::InputOutputError;
            errorString = QBluetoothDeviceDiscoveryAgent::tr("Discovery cannot be stopped");
            emit q->error(lastError);
            return;
        }
    } else if (m_active == BtleScanActive) {
        stopLowEnergyScan();
    }
}

void QBluetoothDeviceDiscoveryAgentPrivate::processSdpDiscoveryFinished()
{
    // We need to guard because Android sends two DISCOVERY_FINISHED when cancelling
    // Also if we have two active agents both receive the same signal.
    // If this one is not active ignore the device information
    if (m_active != SDPScanActive)
        return;

    Q_Q(QBluetoothDeviceDiscoveryAgent);

    if (pendingCancel && !pendingStart) {
        m_active = NoScanActive;
        pendingCancel = false;
        emit q->canceled();
    } else if (pendingStart) {
        pendingStart = pendingCancel = false;
        start();
    } else {
        // check that it didn't finish due to turned off Bluetooth Device
        const int state = adapter.callMethod<jint>("getState");
        if (state != 12) {  // BluetoothAdapter.STATE_ON
            m_active = NoScanActive;
            lastError = QBluetoothDeviceDiscoveryAgent::PoweredOffError;
            errorString = QBluetoothDeviceDiscoveryAgent::tr("Device is powered off");
            emit q->error(lastError);
            return;
        }

        // start LE scan if supported
        if (QtAndroidPrivate::androidSdkVersion() < 18) {
            qCDebug(QT_BT_ANDROID) << "Skipping Bluetooth Low Energy device scan";
            m_active = NoScanActive;
            emit q->finished();
        } else {
            startLowEnergyScan();
        }
    }
}

void QBluetoothDeviceDiscoveryAgentPrivate::processDiscoveredDevices(
    const QBluetoothDeviceInfo &info, bool isLeResult)
{
    // If we have two active agents both receive the same signal.
    // If this one is not active ignore the device information
    if (m_active != SDPScanActive && !isLeResult)
        return;
    if (m_active != BtleScanActive && isLeResult)
        return;

    Q_Q(QBluetoothDeviceDiscoveryAgent);

    for (int i = 0; i < discoveredDevices.size(); i++) {
        if (discoveredDevices[i].address() == info.address()) {
            if (discoveredDevices[i] == info) {
                qCDebug(QT_BT_ANDROID) << "Duplicate: " << info.address()
                                       << "isLeScanResult:" << isLeResult;
                return;
            }

            // same device found -> avoid duplicates and update core configuration
            discoveredDevices[i].setCoreConfigurations(discoveredDevices[i].coreConfigurations() | info.coreConfigurations());

            emit q->deviceDiscovered(info);
            return;
        }
    }

    discoveredDevices.append(info);
    qCDebug(QT_BT_ANDROID) << "Device found: " << info.name() << info.address().toString()
                           << "isLeScanResult:" << isLeResult;
    emit q->deviceDiscovered(info);
}

void QBluetoothDeviceDiscoveryAgentPrivate::startLowEnergyScan()
{
    Q_Q(QBluetoothDeviceDiscoveryAgent);

    m_active = BtleScanActive;

    QAndroidJniEnvironment env;
    if (!leScanner.isValid()) {
        leScanner = QAndroidJniObject("org/qtproject/qt5/android/bluetooth/QtBluetoothLE");
        if (env->ExceptionCheck() || !leScanner.isValid()) {
            qCWarning(QT_BT_ANDROID) << "Cannot load BTLE device scan class";
            env->ExceptionDescribe();
            env->ExceptionClear();
            m_active = NoScanActive;
            emit q->finished();
        }

        leScanner.setField<jlong>("qtObject", reinterpret_cast<long>(receiver));
    }

    jboolean result = leScanner.callMethod<jboolean>("scanForLeDevice", "(Z)Z", true);
    if (!result) {
        qCWarning(QT_BT_ANDROID) << "Cannot start BTLE device scanner";
        m_active = NoScanActive;
        emit q->finished();
    }

    if (!leScanTimeout) {
        leScanTimeout = new QTimer(this);
        leScanTimeout->setSingleShot(true);
        leScanTimeout->setInterval(10000);
        connect(leScanTimeout, &QTimer::timeout,
                this, &QBluetoothDeviceDiscoveryAgentPrivate::stopLowEnergyScan);
    }

    leScanTimeout->start();
}

void QBluetoothDeviceDiscoveryAgentPrivate::stopLowEnergyScan()
{
    jboolean result = leScanner.callMethod<jboolean>("scanForLeDevice", "(Z)Z", false);
    if (!result)
        qCWarning(QT_BT_ANDROID) << "Cannot stop BTLE device scanner";

    m_active = NoScanActive;

    Q_Q(QBluetoothDeviceDiscoveryAgent);
    if (leScanTimeout->isActive()) {
        // still active if this function was called from stop()
        leScanTimeout->stop();
        emit q->canceled();
    } else {
        // timeout -> regular stop
        emit q->finished();
    }
}
QT_END_NAMESPACE
