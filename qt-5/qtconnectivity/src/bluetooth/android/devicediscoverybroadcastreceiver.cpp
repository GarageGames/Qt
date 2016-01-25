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

#include "android/devicediscoverybroadcastreceiver_p.h"
#include <QtCore/QLoggingCategory>
#include <QtBluetooth/QBluetoothAddress>
#include <QtBluetooth/QBluetoothDeviceInfo>
#include "android/jni_android_p.h"
#include <QtCore/private/qjnihelpers_p.h>
#include <QtCore/QHash>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_ANDROID)

typedef QHash<jint, QBluetoothDeviceInfo::CoreConfigurations> JCachedBtTypes;
Q_GLOBAL_STATIC(JCachedBtTypes, cachedBtTypes)

// class name
static const char * const javaBluetoothDeviceClassName = "android/bluetooth/BluetoothDevice";
static const char * const javaDeviceTypeClassic = "DEVICE_TYPE_CLASSIC";
static const char * const javaDeviceTypeDual = "DEVICE_TYPE_DUAL";
static const char * const javaDeviceTypeLE = "DEVICE_TYPE_LE";
static const char * const javaDeviceTypeUnknown = "DEVICE_TYPE_UNKNOWN";

QBluetoothDeviceInfo::CoreConfigurations qtBtTypeForJavaBtType(jint javaType)
{
    const JCachedBtTypes::iterator it = cachedBtTypes()->find(javaType);
    if (it == cachedBtTypes()->end()) {
        QAndroidJniEnvironment env;

        if (javaType == QAndroidJniObject::getStaticField<jint>(
                            javaBluetoothDeviceClassName, javaDeviceTypeClassic)) {
            cachedBtTypes()->insert(javaType,
                                    QBluetoothDeviceInfo::BaseRateCoreConfiguration);
            return QBluetoothDeviceInfo::BaseRateCoreConfiguration;
        } else if (javaType == QAndroidJniObject::getStaticField<jint>(
                        javaBluetoothDeviceClassName, javaDeviceTypeLE)) {
            cachedBtTypes()->insert(javaType,
                                    QBluetoothDeviceInfo::LowEnergyCoreConfiguration);
            return QBluetoothDeviceInfo::LowEnergyCoreConfiguration;
        } else if (javaType == QAndroidJniObject::getStaticField<jint>(
                            javaBluetoothDeviceClassName, javaDeviceTypeDual)) {
            cachedBtTypes()->insert(javaType,
                                    QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration);
            return QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration;
        } else if (javaType == QAndroidJniObject::getStaticField<jint>(
                                javaBluetoothDeviceClassName, javaDeviceTypeUnknown)) {
            cachedBtTypes()->insert(javaType,
                                QBluetoothDeviceInfo::UnknownCoreConfiguration);
        } else {
            qCWarning(QT_BT_ANDROID) << "Unknown Bluetooth device type value";
        }

        return QBluetoothDeviceInfo::UnknownCoreConfiguration;
    } else {
        return it.value();
    }
}

DeviceDiscoveryBroadcastReceiver::DeviceDiscoveryBroadcastReceiver(QObject* parent): AndroidBroadcastReceiver(parent)
{
    addAction(valueForStaticField(JavaNames::BluetoothDevice, JavaNames::ActionFound));
    addAction(valueForStaticField(JavaNames::BluetoothAdapter, JavaNames::ActionDiscoveryStarted));
    addAction(valueForStaticField(JavaNames::BluetoothAdapter, JavaNames::ActionDiscoveryFinished));
}

// Runs in Java thread
void DeviceDiscoveryBroadcastReceiver::onReceive(JNIEnv *env, jobject context, jobject intent)
{
    Q_UNUSED(context);
    Q_UNUSED(env);

    QAndroidJniObject intentObject(intent);
    const QString action = intentObject.callObjectMethod("getAction", "()Ljava/lang/String;").toString();

    qCDebug(QT_BT_ANDROID) << "DeviceDiscoveryBroadcastReceiver::onReceive() - event:" << action;

    if (action == valueForStaticField(JavaNames::BluetoothAdapter,
                                      JavaNames::ActionDiscoveryFinished).toString()) {
        emit finished();
    } else if (action == valueForStaticField(JavaNames::BluetoothAdapter,
                                             JavaNames::ActionDiscoveryStarted).toString()) {

    } else if (action == valueForStaticField(JavaNames::BluetoothDevice,
                                             JavaNames::ActionFound).toString()) {
        //get BluetoothDevice
        QAndroidJniObject keyExtra = valueForStaticField(JavaNames::BluetoothDevice,
                                                         JavaNames::ExtraDevice);
        const QAndroidJniObject bluetoothDevice =
                intentObject.callObjectMethod("getParcelableExtra",
                                              "(Ljava/lang/String;)Landroid/os/Parcelable;",
                                              keyExtra.object<jstring>());

        if (!bluetoothDevice.isValid())
            return;

        keyExtra = valueForStaticField(JavaNames::BluetoothDevice,
                                       JavaNames::ExtraRssi);
        int rssi = intentObject.callMethod<jshort>("getShortExtra",
                                                "(Ljava/lang/String;S)S",
                                                keyExtra.object<jstring>(),
                                                0);

        const QBluetoothDeviceInfo info = retrieveDeviceInfo(env, bluetoothDevice, rssi);
        if (info.isValid())
            emit deviceDiscovered(info, false);
    }
}

// Runs in Java thread
void DeviceDiscoveryBroadcastReceiver::onReceiveLeScan(
        JNIEnv *env, jobject jBluetoothDevice, jint rssi)
{
    qCDebug(QT_BT_ANDROID) << "DeviceDiscoveryBroadcastReceiver::onReceiveLeScan()";
    const QAndroidJniObject bluetoothDevice(jBluetoothDevice);
    if (!bluetoothDevice.isValid())
        return;

    const QBluetoothDeviceInfo info = retrieveDeviceInfo(env, bluetoothDevice, rssi);
    if (info.isValid())
        emit deviceDiscovered(info, true);
}

QBluetoothDeviceInfo DeviceDiscoveryBroadcastReceiver::retrieveDeviceInfo(JNIEnv *env, const QAndroidJniObject &bluetoothDevice, int rssi)
{
    const QString deviceName = bluetoothDevice.callObjectMethod<jstring>("getName").toString();
    const QBluetoothAddress deviceAddress(bluetoothDevice.callObjectMethod<jstring>("getAddress").toString());

    const QAndroidJniObject bluetoothClass = bluetoothDevice.callObjectMethod("getBluetoothClass",
                                                                        "()Landroid/bluetooth/BluetoothClass;");
    if (!bluetoothClass.isValid())
        return QBluetoothDeviceInfo();
    int classType = bluetoothClass.callMethod<jint>("getDeviceClass");


    static QList<qint32> services;
    if (services.count() == 0)
        services << QBluetoothDeviceInfo::PositioningService
                 << QBluetoothDeviceInfo::NetworkingService
                 << QBluetoothDeviceInfo::RenderingService
                 << QBluetoothDeviceInfo::CapturingService
                 << QBluetoothDeviceInfo::ObjectTransferService
                 << QBluetoothDeviceInfo::AudioService
                 << QBluetoothDeviceInfo::TelephonyService
                 << QBluetoothDeviceInfo::InformationService;

    //Matching BluetoothClass.Service values
    qint32 result = 0;
    qint32 current = 0;
    for (int i = 0; i < services.count(); i++) {
        current = services.at(i);
        int id = (current << 16);
        if (bluetoothClass.callMethod<jboolean>("hasService", "(I)Z", id))
            result |= current;
    }

    result = result << 13;
    classType |= result;

    QBluetoothDeviceInfo info(deviceAddress, deviceName, classType);
    info.setRssi(rssi);

    if (QtAndroidPrivate::androidSdkVersion() >= 18) {
        jint javaBtType = bluetoothDevice.callMethod<jint>("getType");

        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        } else {
            info.setCoreConfigurations(qtBtTypeForJavaBtType(javaBtType));
        }
    }

    return info;
}

QT_END_NAMESPACE

