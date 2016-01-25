/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2013 Javier S. Pedro <maemo@javispedro.com>
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

#include "osx/osxbtutility_p.h"
#include "osx/uistrings_p.h"

#include "qlowenergyserviceprivate_p.h"
#include "qlowenergycontroller_osx_p.h"
#include "qbluetoothlocaldevice.h"
#include "qbluetoothdeviceinfo.h"
#include "qlowenergycontroller.h"
#include "qbluetoothuuid.h"

#include <QtCore/qloggingcategory.h>
#include <QtCore/qsharedpointer.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qsysinfo.h>
#include <QtCore/qglobal.h>
#include <QtCore/qstring.h>
#include <QtCore/qlist.h>

#define OSX_D_PTR QLowEnergyControllerPrivateOSX *osx_d_ptr = static_cast<QLowEnergyControllerPrivateOSX *>(d_ptr)

QT_BEGIN_NAMESPACE

namespace {


class QLowEnergyControllerMetaTypes
{
public:
    QLowEnergyControllerMetaTypes()
    {
        qRegisterMetaType<QLowEnergyController::ControllerState>();
        qRegisterMetaType<QLowEnergyController::Error>();
    }
} qLowEnergyControllerMetaTypes;


typedef QSharedPointer<QLowEnergyServicePrivate> ServicePrivate;

// Convenience function, can return a smart pointer that 'isNull'.
ServicePrivate qt_createLEService(QLowEnergyControllerPrivateOSX *controller, CBService *cbService, bool included)
{
    Q_ASSERT_X(controller, Q_FUNC_INFO, "invalid controller (null)");
    Q_ASSERT_X(cbService, Q_FUNC_INFO, "invalid service (nil)");

    CBUUID *const cbUuid = cbService.UUID;
    if (!cbUuid) {
        qCDebug(QT_BT_OSX) << Q_FUNC_INFO << "invalid service, "
                              "UUID is nil";
        return ServicePrivate();
    }

    const QBluetoothUuid qtUuid(OSXBluetooth::qt_uuid(cbUuid));
    if (qtUuid.isNull()) // Conversion error is reported by qt_uuid.
        return ServicePrivate();

    ServicePrivate newService(new QLowEnergyServicePrivate);
    newService->uuid = qtUuid;
    newService->setController(controller);

    if (included)
        newService->type |= QLowEnergyService::IncludedService;

    // TODO: isPrimary is ... always 'NO' - to be investigated.
    /*
    #if QT_MAC_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_9, __IPHONE_6_0)
    using OSXBluetooth::qt_OS_limit;
    if (QSysInfo::MacintoshVersion >= qt_OS_limit(QSysInfo::MV_10_9, QSysInfo::MV_IOS_6_0)) {
        if (!cbService.isPrimary) {
            // Our guess included/not was probably wrong.
            newService->type &= ~QLowEnergyService::PrimaryService;
            newService->type |= QLowEnergyService::IncludedService;
        }
    }
    #endif
    */
    // No such property before 10_9/6_0.
    return newService;
}

typedef QList<QBluetoothUuid> UUIDList;

UUIDList qt_servicesUuids(NSArray *services)
{
    QT_BT_MAC_AUTORELEASEPOOL;

    if (!services || !services.count)
        return UUIDList();

    UUIDList uuids;

    for (CBService *s in services)
        uuids.append(OSXBluetooth::qt_uuid(s.UUID));

    return uuids;
}

}

QLowEnergyControllerPrivateOSX::QLowEnergyControllerPrivateOSX(QLowEnergyController *q)
    : q_ptr(q),
      isConnecting(false),
      lastError(QLowEnergyController::NoError),
      controllerState(QLowEnergyController::UnconnectedState),
      addressType(QLowEnergyController::PublicAddress)
{
    // This is the "wrong" constructor - no valid device UUID to connect later.
    Q_ASSERT_X(q, Q_FUNC_INFO, "invalid q_ptr (null)");
    // We still create a manager, to simplify error handling later.
    centralManager.reset([[ObjCCentralManager alloc] initWithDelegate:this]);
    if (!centralManager) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO
                             << "failed to initialize central manager";
    }
}

QLowEnergyControllerPrivateOSX::QLowEnergyControllerPrivateOSX(QLowEnergyController *q,
                                                               const QBluetoothDeviceInfo &deviceInfo)
    : q_ptr(q),
      deviceUuid(deviceInfo.deviceUuid()),
      deviceName(deviceInfo.name()),
      isConnecting(false),
      lastError(QLowEnergyController::NoError),
      controllerState(QLowEnergyController::UnconnectedState),
      addressType(QLowEnergyController::PublicAddress)
{
    Q_ASSERT_X(q, Q_FUNC_INFO, "invalid q_ptr (null)");
    centralManager.reset([[ObjCCentralManager alloc] initWithDelegate:this]);
    if (!centralManager) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO
                             << "failed to initialize central manager";
    }
}

QLowEnergyControllerPrivateOSX::~QLowEnergyControllerPrivateOSX()
{
}

bool QLowEnergyControllerPrivateOSX::isValid() const
{
    // isValid means only "was able to allocate all resources",
    // nothing more.
    return centralManager;
}

void QLowEnergyControllerPrivateOSX::LEnotSupported()
{
    // Report as an error. But this should not be possible
    // actually: before connecting to any device, we have
    // to discover it, if it was discovered ... LE _must_
    // be supported.
}

void QLowEnergyControllerPrivateOSX::connectSuccess()
{
    Q_ASSERT_X(controllerState == QLowEnergyController::ConnectingState,
               Q_FUNC_INFO, "invalid state");

    controllerState = QLowEnergyController::ConnectedState;

    if (!isConnecting) {
        emit q_ptr->stateChanged(QLowEnergyController::ConnectedState);
        emit q_ptr->connected();
    }
}

void QLowEnergyControllerPrivateOSX::serviceDiscoveryFinished(LEServices services)
{
    Q_ASSERT_X(controllerState == QLowEnergyController::DiscoveringState,
               Q_FUNC_INFO, "invalid state");

    using namespace OSXBluetooth;

    QT_BT_MAC_AUTORELEASEPOOL;

    // Now we have to traverse the discovered services tree.
    // Essentially it's an iterative version of more complicated code from the
    // OSXBTCentralManager's code.
    // All Obj-C entities either auto-release, or guarded by ObjCScopedReferences.
    if (services && [services count]) {
        QMap<QBluetoothUuid, CBService *> discoveredCBServices;
        //1. The first pass - none of this services is 'included' yet (we'll discover 'included'
        //   during the pass 2); we also ignore duplicates (== services with the same UUID)
        // - since we do not have a way to distinguish them later
        //   (our API is using uuids when creating QLowEnergyServices).
        for (CBService *cbService in services.data()) {
            const ServicePrivate newService(qt_createLEService(this, cbService, false));
            if (!newService.data())
                continue;
            if (discoveredServices.contains(newService->uuid)) {
                // It's a bit stupid we first created it ...
                qCDebug(QT_BT_OSX) << Q_FUNC_INFO << "discovered service with a duplicated UUID "
                                   << newService->uuid;
                continue;
            }
            discoveredServices.insert(newService->uuid, newService);
            discoveredCBServices.insert(newService->uuid, cbService);
        }

        ObjCStrongReference<NSMutableArray> toVisit([[NSMutableArray alloc] initWithArray:services], false);
        ObjCStrongReference<NSMutableArray> toVisitNext([[NSMutableArray alloc] init], false);
        ObjCStrongReference<NSMutableSet> visited([[NSMutableSet alloc] init], false);

        while (true) {
            for (NSUInteger i = 0, e = [toVisit count]; i < e; ++i) {
                CBService *const s = [toVisit objectAtIndex:i];
                if (![visited containsObject:s]) {
                    [visited addObject:s];
                    if (s.includedServices && s.includedServices.count)
                        [toVisitNext addObjectsFromArray:s.includedServices];
                }

                const QBluetoothUuid uuid(qt_uuid(s.UUID));
                if (discoveredServices.contains(uuid) && discoveredCBServices.value(uuid) == s) {
                    ServicePrivate qtService(discoveredServices.value(uuid));
                    // Add included UUIDs:
                    qtService->includedServices.append(qt_servicesUuids(s.includedServices));
                }// Else - we ignored this CBService object.
            }

            if (![toVisitNext count])
                break;

            for (NSUInteger i = 0, e = [toVisitNext count]; i < e; ++i) {
                CBService *const s = [toVisitNext objectAtIndex:i];
                const QBluetoothUuid uuid(qt_uuid(s.UUID));
                if (discoveredServices.contains(uuid)) {
                    if (discoveredCBServices.value(uuid) == s) {
                        ServicePrivate qtService(discoveredServices.value(uuid));
                        qtService->type |= QLowEnergyService::IncludedService;
                    } // Else this is the duplicate we ignored already.
                } else {
                    // Oh, we do not even have it yet???
                    ServicePrivate newService(qt_createLEService(this, s, true));
                    discoveredServices.insert(newService->uuid, newService);
                    discoveredCBServices.insert(newService->uuid, s);
                }
            }

            toVisit.resetWithoutRetain(toVisitNext.take());
            toVisitNext.resetWithoutRetain([[NSMutableArray alloc] init]);
        }
    } else {
        qCDebug(QT_BT_OSX) << Q_FUNC_INFO << "no services found";
    }

    foreach (const QBluetoothUuid &uuid, discoveredServices.keys()) {
        QMetaObject::invokeMethod(q_ptr, "serviceDiscovered", Qt::QueuedConnection,
                                 Q_ARG(QBluetoothUuid, uuid));
    }

    controllerState = QLowEnergyController::DiscoveredState;
    QMetaObject::invokeMethod(q_ptr, "stateChanged", Qt::QueuedConnection,
                              Q_ARG(QLowEnergyController::ControllerState, controllerState));
    QMetaObject::invokeMethod(q_ptr, "discoveryFinished", Qt::QueuedConnection);
}

void QLowEnergyControllerPrivateOSX::serviceDetailsDiscoveryFinished(LEService service)
{
    Q_ASSERT_X(!service.isNull(), Q_FUNC_INFO, "invalid service (null)");

    QT_BT_MAC_AUTORELEASEPOOL;

    if (!discoveredServices.contains(service->uuid)) {
        qCDebug(QT_BT_OSX) << Q_FUNC_INFO << "unknown service uuid: "
                           << service->uuid;
        return;
    }

    ServicePrivate qtService(discoveredServices.value(service->uuid));
    // Assert on handles?
    qtService->startHandle = service->startHandle;
    qtService->endHandle = service->endHandle;
    qtService->characteristicList = service->characteristicList;

    qtService->setState(QLowEnergyService::ServiceDiscovered);
}

void QLowEnergyControllerPrivateOSX::characteristicReadNotification(QLowEnergyHandle charHandle,
                                                                    const QByteArray &value)
{
    Q_ASSERT_X(charHandle, Q_FUNC_INFO, "invalid characteristic handle(0)");

    ServicePrivate service(serviceForHandle(charHandle));
    if (service.isNull())
        return;

    QLowEnergyCharacteristic characteristic(characteristicForHandle(charHandle));
    if (!characteristic.isValid()) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "unknown characteristic";
        return;
    }

    if (characteristic.properties() & QLowEnergyCharacteristic::Read)
        updateValueOfCharacteristic(charHandle, value, false);

    emit service->characteristicRead(characteristic, value);
}

void QLowEnergyControllerPrivateOSX::characteristicWriteNotification(QLowEnergyHandle charHandle,
                                                                     const QByteArray &value)
{
    Q_ASSERT_X(charHandle, Q_FUNC_INFO, "invalid characteristic handle(0)");

    ServicePrivate service(serviceForHandle(charHandle));
    if (service.isNull()) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "can not find service for characteristic handle "
                             << charHandle;
        return;
    }

    QLowEnergyCharacteristic characteristic(characteristicForHandle(charHandle));
    if (!characteristic.isValid()) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "unknown characteristic";
        return;
    }

    if (characteristic.properties() & QLowEnergyCharacteristic::Read)
        updateValueOfCharacteristic(charHandle, value, false);

    emit service->characteristicWritten(characteristic, value);
}

void QLowEnergyControllerPrivateOSX::characteristicUpdateNotification(QLowEnergyHandle charHandle,
                                                                     const QByteArray &value)
{
    // TODO: write/update notifications are quite similar (except asserts/warnings messages
    // and different signals emitted). Merge them into one function?
    Q_ASSERT_X(charHandle, Q_FUNC_INFO, "invalid characteristic handle(0)");

    ServicePrivate service(serviceForHandle(charHandle));
    if (service.isNull()) {
        // This can be an error (no characteristic found for this handle),
        // it can also be that we set notify value before the service
        // was reported (serviceDetailsDiscoveryFinished) - this happens,
        // if we read a descriptor (characteristic client configuration),
        // and it's (pre)set.
        return;
    }

    QLowEnergyCharacteristic characteristic(characteristicForHandle(charHandle));
    if (!characteristic.isValid()) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "unknown characteristic";
        return;
    }

    if (characteristic.properties() & QLowEnergyCharacteristic::Read)
        updateValueOfCharacteristic(charHandle, value, false);

    emit service->characteristicChanged(characteristic, value);
}

void QLowEnergyControllerPrivateOSX::descriptorReadNotification(QLowEnergyHandle dHandle, const QByteArray &value)
{
    Q_ASSERT_X(dHandle, Q_FUNC_INFO, "invalid descriptor handle (0)");

    const QLowEnergyDescriptor qtDescriptor(descriptorForHandle(dHandle));
    if (!qtDescriptor.isValid()) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "unknown descriptor " << dHandle;
        return;
    }

    ServicePrivate service(serviceForHandle(qtDescriptor.characteristicHandle()));
    updateValueOfDescriptor(qtDescriptor.characteristicHandle(), dHandle, value, false);
    emit service->descriptorRead(qtDescriptor, value);
}

void QLowEnergyControllerPrivateOSX::descriptorWriteNotification(QLowEnergyHandle dHandle, const QByteArray &value)
{
    Q_ASSERT_X(dHandle, Q_FUNC_INFO, "invalid descriptor handle (0)");

    const QLowEnergyDescriptor qtDescriptor(descriptorForHandle(dHandle));
    if (!qtDescriptor.isValid()) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "unknown descriptor " << dHandle;
        return;
    }

    ServicePrivate service(serviceForHandle(qtDescriptor.characteristicHandle()));
    // TODO: test if this data is what we expected.
    updateValueOfDescriptor(qtDescriptor.characteristicHandle(), dHandle, value, false);
    emit service->descriptorWritten(qtDescriptor, value);
}

void QLowEnergyControllerPrivateOSX::disconnected()
{
    controllerState = QLowEnergyController::UnconnectedState;

    if (!isConnecting) {
        emit q_ptr->stateChanged(QLowEnergyController::UnconnectedState);
        emit q_ptr->disconnected();
    }
}

void QLowEnergyControllerPrivateOSX::error(QLowEnergyController::Error errorCode)
{
    // Errors reported during connect and general errors.

    // We're still in connectToDevice,
    // some error was reported synchronously.
    // Return, the error will be correctly set later
    // by connectToDevice.
    if (isConnecting) {
        lastError = errorCode;
        return;
    }

    setErrorDescription(errorCode);
    emit q_ptr->error(lastError);

    if (controllerState == QLowEnergyController::ConnectingState) {
        controllerState = QLowEnergyController::UnconnectedState;
        emit q_ptr->stateChanged(controllerState);
    } else if (controllerState == QLowEnergyController::DiscoveringState) {
        controllerState = QLowEnergyController::ConnectedState;
        emit q_ptr->stateChanged(controllerState);
    } // In any other case we stay in Discovered, it's
      // a service/characteristic - related error.
}

void QLowEnergyControllerPrivateOSX::error(const QBluetoothUuid &serviceUuid,
                                           QLowEnergyController::Error errorCode)
{
    // Errors reported while discovering service details etc.
    Q_UNUSED(errorCode) // TODO: setError?

    // We failed to discover any characteristics/descriptors.
    if (discoveredServices.contains(serviceUuid)) {
        ServicePrivate qtService(discoveredServices.value(serviceUuid));
        qtService->setState(QLowEnergyService::InvalidService);
    } else {
        qCDebug(QT_BT_OSX) << Q_FUNC_INFO << "error reported for unknown service "
                           << serviceUuid;
    }
}

void QLowEnergyControllerPrivateOSX::error(const QBluetoothUuid &serviceUuid,
                                           QLowEnergyService::ServiceError errorCode)
{
    if (!discoveredServices.contains(serviceUuid)) {
        qCDebug(QT_BT_OSX) << Q_FUNC_INFO << "unknown service uuid: "
                           << serviceUuid;
        return;
    }

    ServicePrivate service(discoveredServices.value(serviceUuid));
    service->setError(errorCode);
}

void QLowEnergyControllerPrivateOSX::connectToDevice()
{
    Q_ASSERT_X(isValid(), Q_FUNC_INFO, "invalid private controller");
    Q_ASSERT_X(controllerState == QLowEnergyController::UnconnectedState,
               Q_FUNC_INFO, "invalid state");
    Q_ASSERT_X(!deviceUuid.isNull(), Q_FUNC_INFO,
               "invalid private controller (no device uuid)");
    Q_ASSERT_X(!isConnecting, Q_FUNC_INFO,
               "recursive connectToDevice call");

    setErrorDescription(QLowEnergyController::NoError);

    isConnecting = true;// Do not emit signals if some callback is executed synchronously.
    controllerState = QLowEnergyController::ConnectingState;
    const QLowEnergyController::Error status = [centralManager connectToDevice:deviceUuid];
    isConnecting = false;

    if (status == QLowEnergyController::NoError && lastError == QLowEnergyController::NoError) {
        emit q_ptr->stateChanged(controllerState);
        if (controllerState == QLowEnergyController::ConnectedState) {
            // If a peripheral is connected already from the Core Bluetooth's
            // POV:
            emit q_ptr->connected();
        } else if (controllerState == QLowEnergyController::UnconnectedState) {
            // Ooops, tried to connect, got peripheral disconnect instead -
            // this happens with Core Bluetooth.
            emit q_ptr->disconnected();
        }
    } else if (status != QLowEnergyController::NoError) {
        error(status);
    } else {
        // Re-set the error/description and emit.
        error(lastError);
    }
}

void QLowEnergyControllerPrivateOSX::discoverServices()
{
    Q_ASSERT_X(isValid(), Q_FUNC_INFO, "invalid private controller");
    Q_ASSERT_X(controllerState != QLowEnergyController::UnconnectedState,
               Q_FUNC_INFO, "not connected to peripheral");

    controllerState = QLowEnergyController::DiscoveringState;
    emit q_ptr->stateChanged(QLowEnergyController::DiscoveringState);
    [centralManager discoverServices];
}

void QLowEnergyControllerPrivateOSX::discoverServiceDetails(const QBluetoothUuid &serviceUuid)
{
    Q_ASSERT_X(isValid(), Q_FUNC_INFO, "invalid private controller");

    if (controllerState != QLowEnergyController::DiscoveredState) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO
                             << "can not discover service details in the current state, "
                             << "QLowEnergyController::DiscoveredState is expected";
        return;
    }

    if (!discoveredServices.contains(serviceUuid)) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "unknown service: " << serviceUuid;
        return;
    }

    ServicePrivate qtService(discoveredServices.value(serviceUuid));
    if ([centralManager discoverServiceDetails:serviceUuid]) {
        qtService->setState(QLowEnergyService::DiscoveringServices);
    } else {
        // The error is returned by CentralManager - no
        // service with a given UUID found on a peripheral.
        qtService->setState(QLowEnergyService::InvalidService);
    }
}

void QLowEnergyControllerPrivateOSX::setNotifyValue(QSharedPointer<QLowEnergyServicePrivate> service,
                                                    QLowEnergyHandle charHandle,
                                                    const QByteArray &newValue)
{
    Q_ASSERT_X(!service.isNull(), Q_FUNC_INFO, "invalid service (null)");
    Q_ASSERT_X(isValid(), Q_FUNC_INFO, "invalid controller");

    if (newValue.size() > 2) {
        // Qt's API requires an error on such write.
        // With Core Bluetooth we do not write any descriptor,
        // but instead call a special method. So it's better to
        // intercept wrong data size here:
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "client characteristic configuration descriptor "
                                "is 2 bytes, but value size is: " << newValue.size();
        service->setError(QLowEnergyService::DescriptorWriteError);
        return;
    }

    if (!discoveredServices.contains(service->uuid)) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "no service with uuid: "
                             << service->uuid << " found";
        return;
    }

    if (!service->characteristicList.contains(charHandle)) {
        qCDebug(QT_BT_OSX) << Q_FUNC_INFO << "no characteristic with handle: "
                           << charHandle << " found";
        return;
    }

    if (![centralManager setNotifyValue:newValue forCharacteristic:charHandle])
        service->setError(QLowEnergyService::DescriptorWriteError);
}

void QLowEnergyControllerPrivateOSX::readCharacteristic(QSharedPointer<QLowEnergyServicePrivate> service,
                                                        QLowEnergyHandle charHandle)
{
    Q_ASSERT_X(!service.isNull(), Q_FUNC_INFO, "invalid service (null)");
    Q_ASSERT_X(isValid(), Q_FUNC_INFO, "invalid controller");

    if (!discoveredServices.contains(service->uuid)) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "no service with uuid:"
                             << service->uuid << "found";
        return;
    }

    if (!service->characteristicList.contains(charHandle)) {
        qCDebug(QT_BT_OSX) << Q_FUNC_INFO << "no characteristic with handle:"
                           << charHandle << "found";
        return;
    }

    if (![centralManager readCharacteristic:charHandle])
        service->setError(QLowEnergyService::CharacteristicReadError);
}

void QLowEnergyControllerPrivateOSX::writeCharacteristic(QSharedPointer<QLowEnergyServicePrivate> service,
                                                         QLowEnergyHandle charHandle, const QByteArray &newValue,
                                                         bool writeWithResponse)
{
    Q_ASSERT_X(!service.isNull(), Q_FUNC_INFO, "invalid service (null)");
    Q_ASSERT_X(isValid(), Q_FUNC_INFO, "invalid controller");

    // We can work only with services, found on a given peripheral
    // (== created by the given LE controller),
    // otherwise we can not write anything at all.
    if (!discoveredServices.contains(service->uuid)) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "no service with uuid: "
                             << service->uuid << " found";
        return;
    }

    if (!service->characteristicList.contains(charHandle)) {
        qCDebug(QT_BT_OSX) << Q_FUNC_INFO << "no characteristic with handle: "
                           << charHandle << " found";
        return;
    }

    const bool result = [centralManager write:newValue
                                        charHandle:charHandle
                                        withResponse:writeWithResponse];
    if (!result)
        service->setError(QLowEnergyService::CharacteristicWriteError);
}

quint16 QLowEnergyControllerPrivateOSX::updateValueOfCharacteristic(QLowEnergyHandle charHandle,
                                                                    const QByteArray &value,
                                                                    bool appendValue)
{
    QSharedPointer<QLowEnergyServicePrivate> service = serviceForHandle(charHandle);
    if (!service.isNull() && service->characteristicList.contains(charHandle)) {
        if (appendValue)
            service->characteristicList[charHandle].value += value;
        else
            service->characteristicList[charHandle].value = value;

        return service->characteristicList[charHandle].value.size();
    }

    return 0;
}

void QLowEnergyControllerPrivateOSX::readDescriptor(QSharedPointer<QLowEnergyServicePrivate> service,
                                                    QLowEnergyHandle descriptorHandle)
{
    Q_ASSERT_X(!service.isNull(), Q_FUNC_INFO, "invalid service (null)");
    Q_ASSERT_X(isValid(), Q_FUNC_INFO, "invalid controller");

    if (!discoveredServices.contains(service->uuid)) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "no service with uuid:"
                             << service->uuid << "found";
        return;
    }

    if (![centralManager readDescriptor:descriptorHandle])
        service->setError(QLowEnergyService::DescriptorReadError);
}

void QLowEnergyControllerPrivateOSX::writeDescriptor(QSharedPointer<QLowEnergyServicePrivate> service,
                                                     QLowEnergyHandle descriptorHandle,
                                                     const QByteArray &newValue)
{
    Q_ASSERT_X(!service.isNull(), Q_FUNC_INFO, "invalid service (null)");
    Q_ASSERT_X(isValid(), Q_FUNC_INFO, "invalid controller");

    // We can work only with services found on a given peripheral
    // (== created by the given LE controller),
    // otherwise we can not write anything at all.
    if (!discoveredServices.contains(service->uuid)) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "no service with uuid: "
                             << service->uuid << " found";
        return;
    }

    if (![centralManager write:newValue descHandle:descriptorHandle])
        service->setError(QLowEnergyService::DescriptorWriteError);
}

quint16 QLowEnergyControllerPrivateOSX::updateValueOfDescriptor(QLowEnergyHandle charHandle, QLowEnergyHandle descHandle,
                                                                const QByteArray &value, bool appendValue)
{
    ServicePrivate service(serviceForHandle(charHandle));
    if (service.isNull() || !service->characteristicList.contains(charHandle))
        return 0;

    if (!service->characteristicList[charHandle].descriptorList.contains(descHandle))
        return 0;

    if (appendValue)
        service->characteristicList[charHandle].descriptorList[descHandle].value += value;
    else
        service->characteristicList[charHandle].descriptorList[descHandle].value = value;

    return service->characteristicList[charHandle].descriptorList[descHandle].value.size();
}

QSharedPointer<QLowEnergyServicePrivate> QLowEnergyControllerPrivateOSX::serviceForHandle(QLowEnergyHandle handle)
{
    foreach (QSharedPointer<QLowEnergyServicePrivate> service, discoveredServices.values()) {
        if (service->startHandle <= handle && handle <= service->endHandle)
            return service;
    }

    return QSharedPointer<QLowEnergyServicePrivate>();
}

QLowEnergyCharacteristic QLowEnergyControllerPrivateOSX::characteristicForHandle(QLowEnergyHandle charHandle)
{
    QSharedPointer<QLowEnergyServicePrivate> service(serviceForHandle(charHandle));
    if (service.isNull())
        return QLowEnergyCharacteristic();

    if (service->characteristicList.isEmpty())
        return QLowEnergyCharacteristic();

    // Check whether it is the handle of a characteristic header
    if (service->characteristicList.contains(charHandle))
        return QLowEnergyCharacteristic(service, charHandle);

    // Check whether it is the handle of the characteristic value or its descriptors
    QList<QLowEnergyHandle> charHandles(service->characteristicList.keys());
    std::sort(charHandles.begin(), charHandles.end());

    for (int i = charHandles.size() - 1; i >= 0; --i) {
        if (charHandles.at(i) > charHandle)
            continue;

        return QLowEnergyCharacteristic(service, charHandles.at(i));
    }

    return QLowEnergyCharacteristic();
}

QLowEnergyDescriptor QLowEnergyControllerPrivateOSX::descriptorForHandle(QLowEnergyHandle descriptorHandle)
{
    const QLowEnergyCharacteristic ch(characteristicForHandle(descriptorHandle));
    if (!ch.isValid())
        return QLowEnergyDescriptor();

    const QLowEnergyServicePrivate::CharData charData = ch.d_ptr->characteristicList[ch.attributeHandle()];

    if (charData.descriptorList.contains(descriptorHandle))
        return QLowEnergyDescriptor(ch.d_ptr, ch.attributeHandle(), descriptorHandle);

    return QLowEnergyDescriptor();
}

void QLowEnergyControllerPrivateOSX::setErrorDescription(QLowEnergyController::Error errorCode)
{
    // This function does not emit!

    lastError = errorCode;

    switch (lastError) {
    case QLowEnergyController::NoError:
        errorString.clear();
        break;
    case QLowEnergyController::UnknownRemoteDeviceError:
        errorString = QCoreApplication::translate(LE_CONTROLLER, LEC_RDEV_NO_FOUND);
        break;
    case QLowEnergyController::InvalidBluetoothAdapterError:
        errorString = QCoreApplication::translate(LE_CONTROLLER, LEC_NO_LOCAL_DEV);
        break;
    case QLowEnergyController::NetworkError:
        errorString = QCoreApplication::translate(LE_CONTROLLER, LEC_IO_ERROR);
        break;
    case QLowEnergyController::UnknownError:
    default:
        errorString = QCoreApplication::translate(LE_CONTROLLER, LEC_UNKNOWN_ERROR);
        break;
    }
}

void QLowEnergyControllerPrivateOSX::invalidateServices()
{
    foreach (const QSharedPointer<QLowEnergyServicePrivate> service, discoveredServices.values()) {
        service->setController(Q_NULLPTR);
        service->setState(QLowEnergyService::InvalidService);
    }

    discoveredServices.clear();
}

QLowEnergyController::QLowEnergyController(const QBluetoothAddress &remoteAddress,
                                           QObject *parent)
    : QObject(parent),
      d_ptr(new QLowEnergyControllerPrivateOSX(this))
{
    OSX_D_PTR;

    osx_d_ptr->remoteAddress = remoteAddress;
    osx_d_ptr->localAddress = QBluetoothLocalDevice().address();

    qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "construction with remote address "
                            "is not supported!";
}

QLowEnergyController::QLowEnergyController(const QBluetoothDeviceInfo &remoteDevice,
                                           QObject *parent)
    : QObject(parent),
      d_ptr(new QLowEnergyControllerPrivateOSX(this, remoteDevice))
{
    OSX_D_PTR;

    osx_d_ptr->localAddress = QBluetoothLocalDevice().address();
    // That's the only "real" ctor - with Core Bluetooth we need a _valid_ deviceUuid
    // from 'remoteDevice'.
}

QLowEnergyController::QLowEnergyController(const QBluetoothAddress &remoteAddress,
                                           const QBluetoothAddress &localAddress,
                                           QObject *parent)
    : QObject(parent),
      d_ptr(new QLowEnergyControllerPrivateOSX(this))
{
    OSX_D_PTR;

    osx_d_ptr->remoteAddress = remoteAddress;
    osx_d_ptr->localAddress = localAddress;

    qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "construction with remote/local "
                            "addresses is not supported!";
}

QLowEnergyController::~QLowEnergyController()
{
    // Deleting a peripheral will also disconnect.
    delete d_ptr;
}

QBluetoothAddress QLowEnergyController::localAddress() const
{
    OSX_D_PTR;

    return osx_d_ptr->localAddress;
}

QBluetoothAddress QLowEnergyController::remoteAddress() const
{
    OSX_D_PTR;

    return osx_d_ptr->remoteAddress;
}

QString QLowEnergyController::remoteName() const
{
   OSX_D_PTR;

   return osx_d_ptr->deviceName;
}

QLowEnergyController::ControllerState QLowEnergyController::state() const
{
    OSX_D_PTR;

    return osx_d_ptr->controllerState;
}

QLowEnergyController::RemoteAddressType QLowEnergyController::remoteAddressType() const
{
    OSX_D_PTR;

    return osx_d_ptr->addressType;
}

void QLowEnergyController::setRemoteAddressType(RemoteAddressType type)
{
    Q_UNUSED(type)

    OSX_D_PTR;

    osx_d_ptr->addressType = type;
}

void QLowEnergyController::connectToDevice()
{
    OSX_D_PTR;

    // A memory allocation problem.
    if (!osx_d_ptr->isValid())
        return osx_d_ptr->error(UnknownError);

    // No QBluetoothDeviceInfo provided during construction.
    if (osx_d_ptr->deviceUuid.isNull())
        return osx_d_ptr->error(UnknownRemoteDeviceError);

    if (osx_d_ptr->controllerState != UnconnectedState)
        return;

    osx_d_ptr->connectToDevice();
}

void QLowEnergyController::disconnectFromDevice()
{
    if (state() == UnconnectedState || state() == ClosingState)
        return;

    OSX_D_PTR;

    if (osx_d_ptr->isValid()) {
        const ControllerState oldState = osx_d_ptr->controllerState;

        osx_d_ptr->controllerState = ClosingState;
        emit stateChanged(ClosingState);
        osx_d_ptr->invalidateServices();
        [osx_d_ptr->centralManager disconnectFromDevice];

        if (oldState == ConnectingState) {
            // With a pending connect attempt there is no
            // guarantee we'll ever have didDisconnect callback,
            // set the state here and now to make sure we still
            // can connect.
            osx_d_ptr->controllerState = UnconnectedState;
            emit stateChanged(UnconnectedState);
        }
    }
}

void QLowEnergyController::discoverServices()
{
    if (state() != ConnectedState)
        return;

    OSX_D_PTR;

    osx_d_ptr->discoverServices();
}

QList<QBluetoothUuid> QLowEnergyController::services() const
{
    OSX_D_PTR;

    return osx_d_ptr->discoveredServices.keys();
}

QLowEnergyService *QLowEnergyController::createServiceObject(const QBluetoothUuid &serviceUuid,
                                                             QObject *parent)
{
    OSX_D_PTR;

    if (!osx_d_ptr->discoveredServices.contains(serviceUuid))
        return Q_NULLPTR;

    return new QLowEnergyService(osx_d_ptr->discoveredServices.value(serviceUuid), parent);
}

QLowEnergyController::Error QLowEnergyController::error() const
{
    OSX_D_PTR;

    return osx_d_ptr->lastError;
}

QString QLowEnergyController::errorString() const
{
    OSX_D_PTR;

    return osx_d_ptr->errorString;
}

QT_END_NAMESPACE
