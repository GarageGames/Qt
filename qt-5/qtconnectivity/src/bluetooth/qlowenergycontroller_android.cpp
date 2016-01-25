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

#include "qlowenergycontroller_p.h"
#include <QtCore/QLoggingCategory>
#include <QtAndroidExtras/QAndroidJniEnvironment>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_ANDROID)

QLowEnergyControllerPrivate::QLowEnergyControllerPrivate()
    : QObject(),
      state(QLowEnergyController::UnconnectedState),
      error(QLowEnergyController::NoError),
      hub(0)
{
}

QLowEnergyControllerPrivate::~QLowEnergyControllerPrivate()
{
}

void QLowEnergyControllerPrivate::connectToDevice()
{
    // required to pass unit test on default backend
    if (remoteDevice.isNull()) {
        qWarning() << "Invalid/null remote device address";
        setError(QLowEnergyController::UnknownRemoteDeviceError);
        return;
    }

    setState(QLowEnergyController::ConnectingState);

    if (!hub) {
        hub = new LowEnergyNotificationHub(remoteDevice, this);
        connect(hub, &LowEnergyNotificationHub::connectionUpdated,
                this, &QLowEnergyControllerPrivate::connectionUpdated);
        connect(hub, &LowEnergyNotificationHub::servicesDiscovered,
                this, &QLowEnergyControllerPrivate::servicesDiscovered);
        connect(hub, &LowEnergyNotificationHub::serviceDetailsDiscoveryFinished,
                this, &QLowEnergyControllerPrivate::serviceDetailsDiscoveryFinished);
        connect(hub, &LowEnergyNotificationHub::characteristicRead,
                this, &QLowEnergyControllerPrivate::characteristicRead);
        connect(hub, &LowEnergyNotificationHub::descriptorRead,
                this, &QLowEnergyControllerPrivate::descriptorRead);
        connect(hub, &LowEnergyNotificationHub::characteristicWritten,
                this, &QLowEnergyControllerPrivate::characteristicWritten);
        connect(hub, &LowEnergyNotificationHub::descriptorWritten,
                this, &QLowEnergyControllerPrivate::descriptorWritten);
        connect(hub, &LowEnergyNotificationHub::characteristicChanged,
                this, &QLowEnergyControllerPrivate::characteristicChanged);
        connect(hub, &LowEnergyNotificationHub::serviceError,
                this, &QLowEnergyControllerPrivate::serviceError);
    }

    if (!hub->javaObject().isValid()) {
        qCWarning(QT_BT_ANDROID) << "Cannot initiate QtBluetoothLE";
        setError(QLowEnergyController::ConnectionError);
        setState(QLowEnergyController::UnconnectedState);
        return;
    }

    bool result = hub->javaObject().callMethod<jboolean>("connect");
    if (!result) {
        setError(QLowEnergyController::ConnectionError);
        setState(QLowEnergyController::UnconnectedState);
        return;
    }
}

void QLowEnergyControllerPrivate::disconnectFromDevice()
{
    /* Catch an Android timeout bug. If the device is connecting but cannot
     * physically connect it seems to ignore the disconnect call below.
     * At least BluetoothGattCallback.onConnectionStateChange never
     * arrives. The next BluetoothGatt.connect() works just fine though.
     * */

    QLowEnergyController::ControllerState oldState = state;
    setState(QLowEnergyController::ClosingState);

    if (hub)
        hub->javaObject().callMethod<void>("disconnect");

    if (oldState == QLowEnergyController::ConnectingState)
        setState(QLowEnergyController::UnconnectedState);
}

void QLowEnergyControllerPrivate::discoverServices()
{
    if (hub && hub->javaObject().callMethod<jboolean>("discoverServices")) {
        qCDebug(QT_BT_ANDROID) << "Service discovery initiated";
    } else {
        //revert to connected state
        setError(QLowEnergyController::NetworkError);
        setState(QLowEnergyController::ConnectedState);
    }
}

void QLowEnergyControllerPrivate::discoverServiceDetails(const QBluetoothUuid &service)
{
    if (!serviceList.contains(service)) {
        qCWarning(QT_BT_ANDROID) << "Discovery of unknown service" << service.toString()
                                 << "not possible";
        return;
    }

    if (!hub)
        return;

    //cut leading { and trailing } {xxx-xxx}
    QString tempUuid = service.toString();
    tempUuid.chop(1); //remove trailing '}'
    tempUuid.remove(0, 1); //remove first '{'

    QAndroidJniEnvironment env;
    QAndroidJniObject uuid = QAndroidJniObject::fromString(tempUuid);
    bool result = hub->javaObject().callMethod<jboolean>("discoverServiceDetails",
                                                         "(Ljava/lang/String;)Z",
                                                         uuid.object<jstring>());
    if (!result) {
        QSharedPointer<QLowEnergyServicePrivate> servicePrivate =
                serviceList.value(service);
        if (!servicePrivate.isNull()) {
            servicePrivate->setError(QLowEnergyService::UnknownError);
            servicePrivate->setState(QLowEnergyService::DiscoveryRequired);
        }
        qCWarning(QT_BT_ANDROID) << "Cannot discover details for" << service.toString();
        return;
    }

    qCDebug(QT_BT_ANDROID) << "Discovery of" << service << "started";
}

void QLowEnergyControllerPrivate::writeCharacteristic(
        const QSharedPointer<QLowEnergyServicePrivate> service,
        const QLowEnergyHandle charHandle,
        const QByteArray &newValue,
        bool writeWithResponse)
{
    //TODO don't ignore WriteWithResponse, right now we assume responses
    Q_ASSERT(!service.isNull());

    if (!service->characteristicList.contains(charHandle))
        return;

    QAndroidJniEnvironment env;
    jbyteArray payload;
    payload = env->NewByteArray(newValue.size());
    env->SetByteArrayRegion(payload, 0, newValue.size(),
                            (jbyte *)newValue.constData());

    bool result = false;
    if (hub) {
        qCDebug(QT_BT_ANDROID) << "Write characteristic with handle " << charHandle
                 << newValue.toHex() << "(service:" << service->uuid
                 << ", writeWithResponse:" << writeWithResponse << ")";
        result = hub->javaObject().callMethod<jboolean>("writeCharacteristic", "(I[BI)Z",
                      charHandle, payload,
                      writeWithResponse ?  QLowEnergyService::WriteWithResponse : QLowEnergyService::WriteWithoutResponse);
    }

    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        result = false;
    }

    env->DeleteLocalRef(payload);

    if (!result)
        service->setError(QLowEnergyService::CharacteristicWriteError);
}

void QLowEnergyControllerPrivate::writeDescriptor(
        const QSharedPointer<QLowEnergyServicePrivate> service,
        const QLowEnergyHandle /*charHandle*/,
        const QLowEnergyHandle descHandle,
        const QByteArray &newValue)
{
    Q_ASSERT(!service.isNull());

    QAndroidJniEnvironment env;
    jbyteArray payload;
    payload = env->NewByteArray(newValue.size());
    env->SetByteArrayRegion(payload, 0, newValue.size(),
                            (jbyte *)newValue.constData());

    bool result = false;
    if (hub) {
        qCDebug(QT_BT_ANDROID) << "Write descriptor with handle " << descHandle
                 << newValue.toHex() << "(service:" << service->uuid << ")";
        result = hub->javaObject().callMethod<jboolean>("writeDescriptor", "(I[B)Z",
                                                        descHandle, payload);
    }

    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        result = false;
    }

    env->DeleteLocalRef(payload);

    if (!result)
        service->setError(QLowEnergyService::DescriptorWriteError);
}

void QLowEnergyControllerPrivate::readCharacteristic(
        const QSharedPointer<QLowEnergyServicePrivate> service,
        const QLowEnergyHandle charHandle)
{
    Q_ASSERT(!service.isNull());

    if (!service->characteristicList.contains(charHandle))
        return;

    QAndroidJniEnvironment env;
    bool result = false;
    if (hub) {
        qCDebug(QT_BT_ANDROID) << "Read characteristic with handle"
                               <<  charHandle << service->uuid;
        result = hub->javaObject().callMethod<jboolean>("readCharacteristic",
                      "(I)Z", charHandle);
    }

    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        result = false;
    }

    if (!result)
        service->setError(QLowEnergyService::CharacteristicWriteError);
}

void QLowEnergyControllerPrivate::readDescriptor(
        const QSharedPointer<QLowEnergyServicePrivate> service,
        const QLowEnergyHandle /*charHandle*/,
        const QLowEnergyHandle descriptorHandle)
{
    Q_ASSERT(!service.isNull());

    QAndroidJniEnvironment env;
    bool result = false;
    if (hub) {
        qCDebug(QT_BT_ANDROID) << "Read descriptor with handle"
                               <<  descriptorHandle << service->uuid;
        result = hub->javaObject().callMethod<jboolean>("readDescriptor",
                      "(I)Z", descriptorHandle);
    }

    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        result = false;
    }

    if (!result)
        service->setError(QLowEnergyService::DescriptorWriteError);
}

void QLowEnergyControllerPrivate::connectionUpdated(
        QLowEnergyController::ControllerState newState,
        QLowEnergyController::Error errorCode)
{
    Q_Q(QLowEnergyController);

    const QLowEnergyController::ControllerState oldState = state;
    qCDebug(QT_BT_ANDROID) << "Connection updated:"
                           << "error:" << errorCode
                           << "oldState:" << oldState
                           << "newState:" << newState;

    if (errorCode != QLowEnergyController::NoError) {
        // ConnectionError if transition from Connecting to Connected
        if (oldState == QLowEnergyController::ConnectingState) {
            setError(QLowEnergyController::ConnectionError);
            /* There is a bug in Android, when connecting to an unconnectable
             * device. The connection times out and Android sends error code
             * 133 (doesn't exist) and STATE_CONNECTED. A subsequent disconnect()
             * call never sends a STATE_DISCONNECTED either.
             * As workaround we will trigger disconnect when we encounter
             * error during connect attempt. This leaves the controller
             * in a cleaner state.
             * */
            newState = QLowEnergyController::UnconnectedState;
        }
        else
            setError(errorCode);
    }

    setState(newState);
    if (newState == QLowEnergyController::UnconnectedState
            && !(oldState == QLowEnergyController::UnconnectedState
                || oldState == QLowEnergyController::ConnectingState)) {
        emit q->disconnected();
    } else if (newState == QLowEnergyController::ConnectedState
               && oldState != QLowEnergyController::ConnectedState ) {
        emit q->connected();
    }
}

void QLowEnergyControllerPrivate::servicesDiscovered(
        QLowEnergyController::Error errorCode, const QString &foundServices)
{
    Q_Q(QLowEnergyController);

    if (errorCode == QLowEnergyController::NoError) {
        //Android delivers all services in one go
        const QStringList list = foundServices.split(QStringLiteral(" "), QString::SkipEmptyParts);
        foreach (const QString &entry, list) {
            const QBluetoothUuid service(entry);
            if (service.isNull())
                return;

            QLowEnergyServicePrivate *priv = new QLowEnergyServicePrivate();
            priv->uuid = service;
            priv->setController(this);

            QSharedPointer<QLowEnergyServicePrivate> pointer(priv);
            serviceList.insert(service, pointer);

            emit q->serviceDiscovered(QBluetoothUuid(entry));
        }

        setState(QLowEnergyController::DiscoveredState);
        emit q->discoveryFinished();
    } else {
        setError(errorCode);
        setState(QLowEnergyController::ConnectedState);
    }
}

void QLowEnergyControllerPrivate::serviceDetailsDiscoveryFinished(
        const QString &serviceUuid, int startHandle, int endHandle)
{
    const QBluetoothUuid service(serviceUuid);
    if (!serviceList.contains(service)) {
        qCWarning(QT_BT_ANDROID) << "Discovery done of unknown service:"
                                 << service.toString();
        return;
    }

    //update service data
    QSharedPointer<QLowEnergyServicePrivate> pointer =
            serviceList.value(service);
    pointer->startHandle = startHandle;
    pointer->endHandle = endHandle;

    if (hub && hub->javaObject().isValid()) {
        QAndroidJniObject uuid = QAndroidJniObject::fromString(serviceUuid);
        QAndroidJniObject javaIncludes = hub->javaObject().callObjectMethod(
                                        "includedServices",
                                        "(Ljava/lang/String;)Ljava/lang/String;",
                                        uuid.object<jstring>());
        if (javaIncludes.isValid()) {
            const QStringList list = javaIncludes.toString()
                                                 .split(QStringLiteral(" "),
                                                        QString::SkipEmptyParts);
            foreach (const QString &entry, list) {
                const QBluetoothUuid service(entry);
                if (service.isNull())
                    return;

                pointer->includedServices.append(service);

                // update the type of the included service
                QSharedPointer<QLowEnergyServicePrivate> otherService =
                        serviceList.value(service);
                if (!otherService.isNull())
                    otherService->type |= QLowEnergyService::IncludedService;
            }
        }
    }

    qCDebug(QT_BT_ANDROID) << "Service" << serviceUuid << "discovered (start:"
              << startHandle << "end:" << endHandle << ")" << pointer.data();

    pointer->setState(QLowEnergyService::ServiceDiscovered);
}

void QLowEnergyControllerPrivate::characteristicRead(
        const QBluetoothUuid &serviceUuid, int handle,
        const QBluetoothUuid &charUuid, int properties, const QByteArray &data)
{
    if (!serviceList.contains(serviceUuid))
        return;

    QSharedPointer<QLowEnergyServicePrivate> service =
            serviceList.value(serviceUuid);
    QLowEnergyHandle charHandle = handle;

    QLowEnergyServicePrivate::CharData &charDetails =
            service->characteristicList[charHandle];

    //Android uses same property value as Qt which is the Bluetooth LE standard
    charDetails.properties = QLowEnergyCharacteristic::PropertyType(properties);
    charDetails.uuid = charUuid;
    charDetails.value = data;
    //value handle always one larger than characteristics value handle
    charDetails.valueHandle = charHandle + 1;

    if (service->state == QLowEnergyService::ServiceDiscovered) {
        QLowEnergyCharacteristic characteristic = characteristicForHandle(charHandle);
        if (!characteristic.isValid()) {
            qCWarning(QT_BT_ANDROID) << "characteristicRead: Cannot find characteristic";
            return;
        }
        emit service->characteristicRead(characteristic, data);
    }
}

void QLowEnergyControllerPrivate::descriptorRead(
        const QBluetoothUuid &serviceUuid, const QBluetoothUuid &charUuid,
        int descHandle, const QBluetoothUuid &descUuid, const QByteArray &data)
{
    if (!serviceList.contains(serviceUuid))
        return;

    QSharedPointer<QLowEnergyServicePrivate> service =
            serviceList.value(serviceUuid);

    bool entryUpdated = false;
    foreach (QLowEnergyHandle charHandle, service->characteristicList.keys()) {
        QLowEnergyServicePrivate::CharData &charDetails =
                service->characteristicList[charHandle];
        if (charDetails.uuid != charUuid)
            continue;

        // new entry created if it doesn't exist
        QLowEnergyServicePrivate::DescData &descDetails =
                charDetails.descriptorList[descHandle];
        descDetails.uuid = descUuid;
        descDetails.value = data;
        entryUpdated = true;
        break;
    }

    if (!entryUpdated) {
        qCWarning(QT_BT_ANDROID) << "Cannot find/update descriptor"
                                 << descUuid << charUuid << serviceUuid;
    } else if (service->state == QLowEnergyService::ServiceDiscovered){
        QLowEnergyDescriptor descriptor = descriptorForHandle(descHandle);
        if (!descriptor.isValid()) {
            qCWarning(QT_BT_ANDROID) << "descriptorRead: Cannot find descriptor";
            return;
        }
        emit service->descriptorRead(descriptor, data);
    }
}

void QLowEnergyControllerPrivate::characteristicWritten(
        int charHandle, const QByteArray &data, QLowEnergyService::ServiceError errorCode)
{
    QSharedPointer<QLowEnergyServicePrivate> service =
            serviceForHandle(charHandle);
    if (service.isNull())
        return;

    qCDebug(QT_BT_ANDROID) << "Characteristic write confirmation" << service->uuid
                           << charHandle << data.toHex() << errorCode;

    if (errorCode != QLowEnergyService::NoError) {
        service->setError(errorCode);
        return;
    }

    QLowEnergyCharacteristic characteristic = characteristicForHandle(charHandle);
    if (!characteristic.isValid()) {
        qCWarning(QT_BT_ANDROID) << "characteristicWritten: Cannot find characteristic";
        return;
    }

    // only update cache when property is readable. Otherwise it remains
    // empty.
    if (characteristic.properties() & QLowEnergyCharacteristic::Read)
        updateValueOfCharacteristic(charHandle, data, false);
    emit service->characteristicWritten(characteristic, data);
}

void QLowEnergyControllerPrivate::descriptorWritten(
        int descHandle, const QByteArray &data, QLowEnergyService::ServiceError errorCode)
{
    QSharedPointer<QLowEnergyServicePrivate> service =
            serviceForHandle(descHandle);
    if (service.isNull())
        return;

    qCDebug(QT_BT_ANDROID) << "Descriptor write confirmation" << service->uuid
                           << descHandle << data.toHex() << errorCode;

    if (errorCode != QLowEnergyService::NoError) {
        service->setError(errorCode);
        return;
    }

    QLowEnergyDescriptor descriptor = descriptorForHandle(descHandle);
    if (!descriptor.isValid()) {
        qCWarning(QT_BT_ANDROID) << "descriptorWritten: Cannot find descriptor";
        return;
    }

    updateValueOfDescriptor(descriptor.characteristicHandle(),
                            descHandle, data, false);
    emit service->descriptorWritten(descriptor, data);
}

void QLowEnergyControllerPrivate::characteristicChanged(
        int charHandle, const QByteArray &data)
{
    QSharedPointer<QLowEnergyServicePrivate> service =
            serviceForHandle(charHandle);
    if (service.isNull())
        return;

    qCDebug(QT_BT_ANDROID) << "Characteristic change notification" << service->uuid
                           << charHandle << data.toHex();

    QLowEnergyCharacteristic characteristic = characteristicForHandle(charHandle);
    if (!characteristic.isValid()) {
        qCWarning(QT_BT_ANDROID) << "characteristicChanged: Cannot find characteristic";
        return;
    }

    // only update cache when property is readable. Otherwise it remains
    // empty.
    if (characteristic.properties() & QLowEnergyCharacteristic::Read)
        updateValueOfCharacteristic(characteristic.attributeHandle(),
                                data, false);
    emit service->characteristicChanged(characteristic, data);
}

void QLowEnergyControllerPrivate::serviceError(
        int attributeHandle, QLowEnergyService::ServiceError errorCode)
{
    // ignore call if it isn't really an error
    if (errorCode == QLowEnergyService::NoError)
        return;

    QSharedPointer<QLowEnergyServicePrivate> service =
            serviceForHandle(attributeHandle);
    Q_ASSERT(!service.isNull());

    // ATM we don't really use attributeHandle but later on we might
    // want to associate the error code with a char or desc
    service->setError(errorCode);
}

QT_END_NAMESPACE
