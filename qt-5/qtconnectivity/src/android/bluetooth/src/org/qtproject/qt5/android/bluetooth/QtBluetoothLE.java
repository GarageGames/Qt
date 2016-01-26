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

package org.qtproject.qt5.android.bluetooth;

import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothProfile;
import android.util.Log;

import java.util.ArrayList;
import java.util.Hashtable;
import java.util.LinkedList;
import java.util.List;
import java.util.UUID;

public class QtBluetoothLE {
    private static final String TAG = "QtBluetoothGatt";
    private final BluetoothAdapter mBluetoothAdapter;
    private boolean mLeScanRunning = false;

    private BluetoothGatt mBluetoothGatt = null;
    private String mRemoteGattAddress;
    private final UUID clientCharacteristicUuid = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");


    /* Pointer to the Qt object that "owns" the Java object */
    @SuppressWarnings({"CanBeFinal", "WeakerAccess"})
    long qtObject = 0;
    @SuppressWarnings("WeakerAccess")
    Activity qtactivity = null;

    @SuppressWarnings("WeakerAccess")
    public QtBluetoothLE() {
        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
    }

    public QtBluetoothLE(final String remoteAddress, Activity activity) {
        this();
        qtactivity = activity;
        mRemoteGattAddress = remoteAddress;
    }


    /*************************************************************/
    /* Device scan                                               */
    /*************************************************************/

    /*
        Returns true, if request was successfully completed
     */
    public boolean scanForLeDevice(final boolean isEnabled) {
        if (isEnabled == mLeScanRunning)
            return true;

        if (isEnabled) {
            mLeScanRunning = mBluetoothAdapter.startLeScan(leScanCallback);
        } else {
            mBluetoothAdapter.stopLeScan(leScanCallback);
            mLeScanRunning = false;
        }

        return (mLeScanRunning == isEnabled);
    }

    // Device scan callback
    private final BluetoothAdapter.LeScanCallback leScanCallback =
            new BluetoothAdapter.LeScanCallback() {

                @Override
                public void onLeScan(final BluetoothDevice device, int rssi, byte[] scanRecord) {
                    if (qtObject == 0)
                        return;

                    leScanResult(qtObject, device, rssi);
                }
            };

    public native void leScanResult(long qtObject, BluetoothDevice device, int rssi);

    /*************************************************************/
    /* Service Discovery                                         */
    /*************************************************************/

    private final BluetoothGattCallback gattCallback = new BluetoothGattCallback() {

        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            if (qtObject == 0)
                return;

            int qLowEnergyController_State = 0;
            //This must be in sync with QLowEnergyController::ControllerState
            switch (newState) {
                case BluetoothProfile.STATE_DISCONNECTED:
                    qLowEnergyController_State = 0;
                    // we disconnected -> get rid of data from previous run
                    resetData();
                    // reset mBluetoothGatt, reusing same object is not very reliable
                    // sometimes it reconnects and sometimes it does not.
                    if (mBluetoothGatt != null)
                        mBluetoothGatt.close();
                    mBluetoothGatt = null;
                    break;
                case BluetoothProfile.STATE_CONNECTED:
                    qLowEnergyController_State = 2;
            }

            //This must be in sync with QLowEnergyController::Error
            int errorCode;
            switch (status) {
                case BluetoothGatt.GATT_SUCCESS:
                    errorCode = 0; break; //QLowEnergyController::NoError
                default:
                    Log.w(TAG, "Unhandled error code on connectionStateChanged: " + status);
                    errorCode = status; break; //TODO deal with all errors
            }
            leConnectionStateChange(qtObject, errorCode, qLowEnergyController_State);
        }

        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            //This must be in sync with QLowEnergyController::Error
            int errorCode;
            StringBuilder builder = new StringBuilder();
            switch (status) {
                case BluetoothGatt.GATT_SUCCESS:
                    errorCode = 0; //QLowEnergyController::NoError
                    final List<BluetoothGattService> services = mBluetoothGatt.getServices();
                    for (BluetoothGattService service: services) {
                        builder.append(service.getUuid().toString()).append(" "); //space is separator
                    }
                    break;
                default:
                    Log.w(TAG, "Unhandled error code on onServicesDiscovered: " + status);
                    errorCode = status; break; //TODO deal with all errors
            }
            leServicesDiscovered(qtObject, errorCode, builder.toString());
        }

        public void onCharacteristicRead(android.bluetooth.BluetoothGatt gatt,
                                         android.bluetooth.BluetoothGattCharacteristic characteristic,
                                         int status)
        {
            //runningHandle is only used during serviceDetailsDiscovery
            //If it is -1 we got an update outside of the details discovery process
            final boolean isServiceDiscoveryRun = (runningHandle != -1);

            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.w(TAG, "onCharacteristicRead error: " + status);

                // read errors during serviceDiscovery are ignored
                if (isServiceDiscoveryRun)
                    return;
            }

            synchronized (this) {
                if (uuidToEntry.isEmpty()) // ignore data if internal setup is not ready;
                    return;
            }

            // once we have a service discovery run we report regular changes
            if (!isServiceDiscoveryRun) {

                int foundHandle = -1;
                synchronized (this) {
                    foundHandle = handleForCharacteristic(characteristic);
                }

                synchronized (readWriteQueue) {
                    ioJobPending = false;
                }

                if (foundHandle == -1) {
                    Log.w(TAG, "Out-of-detail-discovery: char update failed. " +
                               "Cannot find handle for characteristic");
                } else {
                    if (status == BluetoothGatt.GATT_SUCCESS) {
                        leCharacteristicRead(qtObject, characteristic.getService().getUuid().toString(),
                                foundHandle + 1, characteristic.getUuid().toString(),
                                characteristic.getProperties(), characteristic.getValue());
                    } else {
                        // This must be in sync with QLowEnergyService::CharacteristicReadError
                        final int characteristicReadError = 5;
                        leServiceError(qtObject, foundHandle + 1, characteristicReadError);
                    }
                }

                performNextIO();
                return;
            }

            GattEntry entry = entries.get(runningHandle);
            entry.valueKnown = true;
            entries.set(runningHandle, entry);

            // Qt manages handles starting at 1, in Java we use a system starting with 0
            //TODO avoid sending service uuid -> service handle should be sufficient
            leCharacteristicRead(qtObject, characteristic.getService().getUuid().toString(),
                    runningHandle + 1, characteristic.getUuid().toString(),
                    characteristic.getProperties(), characteristic.getValue());
            performServiceDetailDiscoveryForHandle(runningHandle + 1, false);
        }

        public void onCharacteristicWrite(android.bluetooth.BluetoothGatt gatt,
                                          android.bluetooth.BluetoothGattCharacteristic characteristic,
                                          int status)
        {
            if (status != BluetoothGatt.GATT_SUCCESS)
                Log.w(TAG, "onCharacteristicWrite: error " + status);

            int handle = handleForCharacteristic(characteristic);
            if (handle == -1) {
                Log.w(TAG,"onCharacteristicWrite: cannot find handle");
                return;
            }

            int errorCode;
            //This must be in sync with QLowEnergyService::ServiceError
            switch (status) {
                case BluetoothGatt.GATT_SUCCESS:
                    errorCode = 0; break; // NoError
                default:
                    errorCode = 2; break; // CharacteristicWriteError
            }

            synchronized (readWriteQueue) {
                ioJobPending = false;
            }
            leCharacteristicWritten(qtObject, handle+1, characteristic.getValue(), errorCode);
            performNextIO();
        }

        public void onCharacteristicChanged(android.bluetooth.BluetoothGatt gatt,
                                            android.bluetooth.BluetoothGattCharacteristic characteristic)
        {
            int handle = handleForCharacteristic(characteristic);
            if (handle == -1) {
                Log.w(TAG,"onCharacteristicChanged: cannot find handle");
                return;
            }

            leCharacteristicChanged(qtObject, handle+1, characteristic.getValue());
        }

        public void onDescriptorRead(android.bluetooth.BluetoothGatt gatt,
                                     android.bluetooth.BluetoothGattDescriptor descriptor,
                                     int status)
        {
            //runningHandle is only used during serviceDetailsDiscovery
            //If it is -1 we got an update outside of the details discovery process
            final boolean isServiceDiscoveryRun = (runningHandle != -1);

            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.w(TAG, "onDescriptorRead error: " + status);

                // read errors during serviceDiscovery are ignored
                if (isServiceDiscoveryRun)
                    return;
            }

            synchronized (this) {
                if (uuidToEntry.isEmpty()) // ignore data if internal setup is not ready;
                    return;
            }


            if (!isServiceDiscoveryRun) {

                int foundHandle = -1;
                synchronized (this) {
                    foundHandle = handleForDescriptor(descriptor);
                }

                synchronized (readWriteQueue) {
                    ioJobPending = false;
                }

                if (foundHandle == -1) {
                    Log.w(TAG, "Out-of-detail-discovery: char update failed. " +
                            "Cannot find handle for descriptor.");
                } else {
                    if (status == BluetoothGatt.GATT_SUCCESS) {
                        leDescriptorRead(qtObject, descriptor.getCharacteristic().getService().getUuid().toString(),
                                descriptor.getCharacteristic().getUuid().toString(), foundHandle + 1,
                                descriptor.getUuid().toString(), descriptor.getValue());
                    } else {
                        // This must be in sync with QLowEnergyService::DescriptorReadError
                        final int descriptorReadError = 6;
                        leServiceError(qtObject, foundHandle + 1, descriptorReadError);
                    }
                }

                performNextIO();
                return;
            }


            GattEntry entry = entries.get(runningHandle);
            entry.valueKnown = true;
            entries.set(runningHandle, entry);
            //TODO avoid sending service and characteristic uuid -> handles should be sufficient
            leDescriptorRead(qtObject, descriptor.getCharacteristic().getService().getUuid().toString(),
                    descriptor.getCharacteristic().getUuid().toString(), runningHandle+1,
                    descriptor.getUuid().toString(), descriptor.getValue());

            /* Some devices preset ClientCharacteristicConfiguration descriptors
             * to enable notifications out of the box. However the additional
             * BluetoothGatt.setCharacteristicNotification call prevents
             * automatic notifications from coming through. Hence we manually set them
             * up here.
             */

            if (descriptor.getUuid().compareTo(clientCharacteristicUuid) == 0) {
                final int value = descriptor.getValue()[0];
                // notification or indication bit set?
                if ((value & 0x03) > 0) {
                    Log.d(TAG, "Found descriptor with automatic notifications.");
                    mBluetoothGatt.setCharacteristicNotification(
                            descriptor.getCharacteristic(), true);
                }
            }

            performServiceDetailDiscoveryForHandle(runningHandle + 1, false);
        }

        public void onDescriptorWrite(android.bluetooth.BluetoothGatt gatt,
                                      android.bluetooth.BluetoothGattDescriptor descriptor,
                                      int status)
        {
            if (status != BluetoothGatt.GATT_SUCCESS)
                Log.w(TAG, "onDescriptorWrite: error " + status);

            int handle = handleForDescriptor(descriptor);

            int errorCode;
            //This must be in sync with QLowEnergyService::ServiceError
            switch (status) {
                case BluetoothGatt.GATT_SUCCESS:
                    errorCode = 0; break; // NoError
                default:
                    errorCode = 3; break; // DescriptorWriteError
            }

            synchronized (readWriteQueue) {
                ioJobPending = false;
            }

            leDescriptorWritten(qtObject, handle+1, descriptor.getValue(), errorCode);
            performNextIO();
        }
        //TODO Requires Android API 21 which is not available on CI yet.
//        public void onReliableWriteCompleted(android.bluetooth.BluetoothGatt gatt,
//                                             int status) {
//            System.out.println("onReliableWriteCompleted");
//        }
//
//        public void onReadRemoteRssi(android.bluetooth.BluetoothGatt gatt,
//                                     int rssi, int status) {
//            System.out.println("onReadRemoteRssi");
//        }

    };


    public boolean connect() {
        BluetoothDevice mRemoteGattDevice;

        try {
            mRemoteGattDevice = mBluetoothAdapter.getRemoteDevice(mRemoteGattAddress);
        } catch (IllegalArgumentException ex) {
            Log.w(TAG, "Remote address is not valid: " + mRemoteGattAddress);
            return false;
        }

        mBluetoothGatt = mRemoteGattDevice.connectGatt(qtactivity, false, gattCallback);
        return mBluetoothGatt != null;
    }

    public void disconnect() {
        if (mBluetoothGatt == null)
            return;

        mBluetoothGatt.disconnect();
    }

    public boolean discoverServices()
    {
        return mBluetoothGatt != null && mBluetoothGatt.discoverServices();
    }

    private enum GattEntryType
    {
        Service, Characteristic, CharacteristicValue, Descriptor
    }
    private class GattEntry
    {
        public GattEntryType type;
        public boolean valueKnown = false;
        public BluetoothGattService service = null;
        public BluetoothGattCharacteristic characteristic = null;
        public BluetoothGattDescriptor descriptor = null;
        public int endHandle;
    }

    private enum IoJobType
    {
        Read, Write
    }

    private class ReadWriteJob
    {
        public GattEntry entry;
        public byte[] newValue;
        public int requestedWriteType;
        public IoJobType jobType;
    }

    private final Hashtable<UUID, List<Integer>> uuidToEntry = new Hashtable<UUID, List<Integer>>(100);
    private final ArrayList<GattEntry> entries = new ArrayList<GattEntry>(100);
    private final LinkedList<Integer> servicesToBeDiscovered = new LinkedList<Integer>();


    private final LinkedList<ReadWriteJob> readWriteQueue = new LinkedList<ReadWriteJob>();
    private boolean ioJobPending;

    /*
        Internal helper function
        Returns the handle id for the given characteristic; otherwise returns -1.

        Note that this is the Java handle. The Qt handle is the Java handle +1.
     */
    private int handleForCharacteristic(BluetoothGattCharacteristic characteristic)
    {
        if (characteristic == null)
            return -1;

        List<Integer> handles = uuidToEntry.get(characteristic.getService().getUuid());
        if (handles == null || handles.isEmpty())
            return -1;

        //TODO for now we assume we always want the first service in case of uuid collision
        int serviceHandle = handles.get(0);

        try {
            GattEntry entry;
            for (int i = serviceHandle+1; i < entries.size(); i++) {
                entry = entries.get(i);
                if (entry == null)
                    continue;

                switch (entry.type) {
                    case Descriptor:
                    case CharacteristicValue:
                        continue;
                    case Service:
                        break;
                    case Characteristic:
                        if (entry.characteristic == characteristic)
                            return i;
                        break;
                }
            }
        } catch (IndexOutOfBoundsException ex) { /*nothing*/ }
        return -1;
    }

    /*
        Internal helper function
        Returns the handle id for the given descriptor; otherwise returns -1.

        Note that this is the Java handle. The Qt handle is the Java handle +1.
     */
    private int handleForDescriptor(BluetoothGattDescriptor descriptor)
    {
        if (descriptor == null)
            return -1;

        List<Integer> handles = uuidToEntry.get(descriptor.getCharacteristic().getService().getUuid());
        if (handles == null || handles.isEmpty())
            return -1;

        //TODO for now we assume we always want the first service in case of uuid collision
        int serviceHandle = handles.get(0);

        try {
            GattEntry entry;
            for (int i = serviceHandle+1; i < entries.size(); i++) {
                entry = entries.get(i);
                if (entry == null)
                    continue;

                switch (entry.type) {
                    case Characteristic:
                    case CharacteristicValue:
                        continue;
                    case Service:
                        break;
                    case Descriptor:
                        if (entry.descriptor == descriptor)
                            return i;
                        break;
                }
            }
        } catch (IndexOutOfBoundsException ignored) { }
        return -1;
    }

    private void populateHandles()
    {
        // We introduce the notion of artificial handles. While GATT handles
        // are not exposed on Android they help to quickly identify GATT attributes
        // on the C++ side. The Qt Api will not expose the handles
        GattEntry entry = null;
        List<BluetoothGattService> services = mBluetoothGatt.getServices();
        for (BluetoothGattService service: services) {
            GattEntry serviceEntry = new GattEntry();
            serviceEntry.type = GattEntryType.Service;
            serviceEntry.service = service;
            entries.add(entry);

            // remember handle for the service for later update
            int serviceHandle = entries.size() - 1;

            //some devices may have more than one service with the same uuid
            List<Integer> old = uuidToEntry.get(service.getUuid());
            if (old == null)
                old = new ArrayList<Integer>();
            old.add(entries.size()-1);
            uuidToEntry.put(service.getUuid(), old);

            // add all characteristics
            List<BluetoothGattCharacteristic> charList = service.getCharacteristics();
            for (BluetoothGattCharacteristic characteristic: charList) {
                entry = new GattEntry();
                entry.type = GattEntryType.Characteristic;
                entry.characteristic = characteristic;
                entries.add(entry);

                // this emulates GATT value attributes
                entry = new GattEntry();
                entry.type = GattEntryType.CharacteristicValue;
                entries.add(entry);

                // add all descriptors
                List<BluetoothGattDescriptor> descList = characteristic.getDescriptors();
                for (BluetoothGattDescriptor desc: descList) {
                    entry = new GattEntry();
                    entry.type = GattEntryType.Descriptor;
                    entry.descriptor = desc;
                    entries.add(entry);
                }
            }

            // update endHandle of current service
            serviceEntry.endHandle = entries.size() - 1;
            entries.set(serviceHandle, serviceEntry);
        }

        entries.trimToSize();
    }

    private int currentServiceInDiscovery = -1;
    private int runningHandle = -1;

    private void resetData()
    {
        synchronized (this) {
            runningHandle = -1;
            currentServiceInDiscovery = -1;
            uuidToEntry.clear();
            entries.clear();
            servicesToBeDiscovered.clear();
        }
        synchronized (readWriteQueue) {
            readWriteQueue.clear();
        }
    }

    public synchronized boolean discoverServiceDetails(String serviceUuid)
    {
        try {
            if (mBluetoothGatt == null)
                return false;

            if (entries.isEmpty())
                populateHandles();

            GattEntry entry;
            int serviceHandle;
            try {
                UUID service = UUID.fromString(serviceUuid);
                List<Integer> handles = uuidToEntry.get(service);
                if (handles == null || handles.isEmpty()) {
                    Log.w(TAG, "Unknown service uuid for current device: " + service.toString());
                    return false;
                }

                //TODO for now we assume we always want the first service in case of uuid collision
                serviceHandle = handles.get(0);
                entry = entries.get(serviceHandle);
                if (entry == null) {
                    Log.w(TAG, "Service with UUID " + service.toString() + " not found");
                    return false;
                }
            } catch (IllegalArgumentException ex) {
                //invalid UUID string passed
                Log.w(TAG, "Cannot parse given UUID");
                return false;
            }

            if (entry.type != GattEntryType.Service) {
                Log.w(TAG, "Given UUID is not a service UUID: " + serviceUuid);
                return false;
            }

            // current service already under investigation
            if (currentServiceInDiscovery == serviceHandle)
                return true;

            if (currentServiceInDiscovery != -1) {
                // we are currently discovering another service
                // we queue the new one up until we finish the previous one
                if (!entry.valueKnown) {
                    servicesToBeDiscovered.add(serviceHandle);
                    Log.w(TAG, "Service discovery already running on another service, " +
                               "queueing request for " + serviceUuid);
                } else {
                    Log.w(TAG, "Service already known");
                }
                return true;
            }

            if (!entry.valueKnown) {
                performServiceDetailDiscoveryForHandle(serviceHandle, true);
            } else {
                Log.w(TAG, "Service already discovered");
            }

        } catch (Exception ex) {
            ex.printStackTrace();
            return false;
        }

        return true;
    }

    /*
        Returns the uuids of the services included by the given service. Otherwise returns null.
        Directly called from Qt.
     */
    public String includedServices(String serviceUuid)
    {
        UUID uuid;
        try {
            uuid = UUID.fromString(serviceUuid);
        } catch (Exception ex) {
            ex.printStackTrace();
            return null;
        }

        //TODO Breaks in case of two services with same uuid
        BluetoothGattService service = mBluetoothGatt.getService(uuid);
        if (service == null)
            return null;

        final List<BluetoothGattService> includes = service.getIncludedServices();
        if (includes.isEmpty())
            return null;

        StringBuilder builder = new StringBuilder();
        for (BluetoothGattService includedService: includes) {
            builder.append(includedService.getUuid().toString()).append(" "); //space is separator
        }

        return builder.toString();
    }

    private void finishCurrentServiceDiscovery()
    {
        int currentEntry = currentServiceInDiscovery;
        GattEntry discoveredService = entries.get(currentServiceInDiscovery);
        discoveredService.valueKnown = true;
        entries.set(currentServiceInDiscovery, discoveredService);

        runningHandle = -1;
        currentServiceInDiscovery = -1;

        leServiceDetailDiscoveryFinished(qtObject, discoveredService.service.getUuid().toString(),
                currentEntry + 1, discoveredService.endHandle + 1);

        if (!servicesToBeDiscovered.isEmpty()) {
            try {
                int nextService = servicesToBeDiscovered.remove();
                performServiceDetailDiscoveryForHandle(nextService, true);
            } catch (IndexOutOfBoundsException ex) {
                Log.w(TAG, "Expected queued service but didn't find any");
            }
        }
    }

    private synchronized void performServiceDetailDiscoveryForHandle(int nextHandle, boolean searchStarted)
    {
        try {
            if (searchStarted) {
                currentServiceInDiscovery = nextHandle;
                runningHandle = ++nextHandle;
            } else {
                runningHandle = nextHandle;
            }

            GattEntry entry;
            try {
                entry = entries.get(nextHandle);
            } catch (IndexOutOfBoundsException ex) {
                //ex.printStackTrace();
                Log.w(TAG, "Last entry of last service read");
                finishCurrentServiceDiscovery();
                return;
            }

            boolean result;
            switch (entry.type) {
                case Characteristic:
                    result = mBluetoothGatt.readCharacteristic(entry.characteristic);
                    try {
                        if (!result) {
                            // add characteristic now since we won't get a read update later one
                            // this is possible when the characteristic is not readable
                            Log.d(TAG, "Non-readable characteristic " + entry.characteristic.getUuid() +
                                    " for service " + entry.characteristic.getService().getUuid());
                            leCharacteristicRead(qtObject, entry.characteristic.getService().getUuid().toString(),
                                    nextHandle + 1, entry.characteristic.getUuid().toString(),
                                    entry.characteristic.getProperties(), entry.characteristic.getValue());
                            performServiceDetailDiscoveryForHandle(runningHandle + 1, false);
                        }
                    } catch (Exception ex)
                    {
                        ex.printStackTrace();
                    }
                    break;
                case CharacteristicValue:
                    // ignore -> nothing to do for this artificial type
                    performServiceDetailDiscoveryForHandle(runningHandle + 1, false);
                    break;
                case Descriptor:
                    result = mBluetoothGatt.readDescriptor(entry.descriptor);
                    if (!result) {
                        // atm all descriptor types are readable
                        Log.d(TAG, "Non-readable descriptor " + entry.descriptor.getUuid() +
                                   " for service/char" + entry.descriptor.getCharacteristic().getService().getUuid() +
                                   "/" + entry.descriptor.getCharacteristic().getUuid());
                        leDescriptorRead(qtObject,
                                entry.descriptor.getCharacteristic().getService().getUuid().toString(),
                                entry.descriptor.getCharacteristic().getUuid().toString(),
                                nextHandle+1, entry.descriptor.getUuid().toString(),
                                entry.descriptor.getValue());
                        performServiceDetailDiscoveryForHandle(runningHandle + 1, false);
                    }
                    break;
                case Service:
                    finishCurrentServiceDiscovery();
                    break;
                default:
                    Log.w(TAG, "Invalid GATT attribute type");
                    break;
            }

        } catch(Exception ex) {
            ex.printStackTrace();
        }
    }

    /*************************************************************/
    /* Write Characteristics                                     */
    /*************************************************************/

    public boolean writeCharacteristic(int charHandle, byte[] newValue,
                                       int writeMode)
    {
        if (mBluetoothGatt == null)
            return false;

        GattEntry entry;
        try {
            entry = entries.get(charHandle-1); //Qt always uses handles+1
        } catch (IndexOutOfBoundsException ex) {
            ex.printStackTrace();
            return false;
        }

        ReadWriteJob newJob = new ReadWriteJob();
        newJob.newValue = newValue;
        newJob.entry = entry;
        newJob.jobType = IoJobType.Write;

        // writeMode must be in sync with QLowEnergyService::WriteMode
        // For now we ignore SignedWriteType as Qt doesn't support it yet.
        switch (writeMode) {
            case 1: //WriteWithoutResponse
                newJob.requestedWriteType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE;
                break;
            default:
                newJob.requestedWriteType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT;
                break;
        }

        boolean result;
        synchronized (readWriteQueue) {
            result = readWriteQueue.add(newJob);
        }

        if (!result) {
            Log.w(TAG, "Cannot add characteristic write request for " + charHandle + " to queue" );
            return false;
        }

        performNextIO();
        return true;
    }

    /*************************************************************/
    /* Write Descriptors                                         */
    /*************************************************************/

    public boolean writeDescriptor(int descHandle, byte[] newValue)
    {
        if (mBluetoothGatt == null)
            return false;

        GattEntry entry;
        try {
            entry = entries.get(descHandle-1); //Qt always uses handles+1
        } catch (IndexOutOfBoundsException ex) {
            ex.printStackTrace();
            return false;
        }

        ReadWriteJob newJob = new ReadWriteJob();
        newJob.newValue = newValue;
        newJob.entry = entry;
        newJob.requestedWriteType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT;
        newJob.jobType = IoJobType.Write;

        boolean result;
        synchronized (readWriteQueue) {
            result = readWriteQueue.add(newJob);
        }

        if (!result) {
            Log.w(TAG, "Cannot add descriptor write request for " + descHandle + " to queue" );
            return false;
        }

        performNextIO();
        return true;
    }

    /*************************************************************/
    /* Read Characteristics                                      */
    /*************************************************************/

    public boolean readCharacteristic(int charHandle)
    {
        if (mBluetoothGatt == null)
            return false;

        GattEntry entry;
        try {
            entry = entries.get(charHandle-1); //Qt always uses handles+1
        } catch (IndexOutOfBoundsException ex) {
            ex.printStackTrace();
            return false;
        }

        ReadWriteJob newJob = new ReadWriteJob();
        newJob.entry = entry;
        newJob.jobType = IoJobType.Read;

        boolean result;
        synchronized (readWriteQueue) {
            result = readWriteQueue.add(newJob);
        }

        if (!result) {
            Log.w(TAG, "Cannot add characteristic read request for " + charHandle + " to queue" );
            return false;
        }

        performNextIO();
        return true;
    }

    public boolean readDescriptor(int descHandle)
    {
        if (mBluetoothGatt == null)
            return false;

        GattEntry entry;
        try {
            entry = entries.get(descHandle-1); //Qt always uses handles+1
        } catch (IndexOutOfBoundsException ex) {
            ex.printStackTrace();
            return false;
        }

        ReadWriteJob newJob = new ReadWriteJob();
        newJob.entry = entry;
        newJob.jobType = IoJobType.Read;

        boolean result;
        synchronized (readWriteQueue) {
            result = readWriteQueue.add(newJob);
        }

        if (!result) {
            Log.w(TAG, "Cannot add descriptor read request for " + descHandle + " to queue" );
            return false;
        }

        performNextIO();
        return true;
    }

    /*
       The queuing is required because two writeCharacteristic/writeDescriptor calls
       cannot execute at the same time. The second write must happen after the
       previous write has finished with on(Characteristic|Descriptor)Write().
    */
    private void performNextIO()
    {
        if (mBluetoothGatt == null)
            return;

        boolean skip = false;
        final ReadWriteJob nextJob;
        synchronized (readWriteQueue) {
            if (readWriteQueue.isEmpty() || ioJobPending)
                return;

            nextJob = readWriteQueue.remove();

            Log.w(TAG, "Performing queued job " + nextJob.jobType);
            if (nextJob.jobType == IoJobType.Read)
                skip = executeReadJob(nextJob);
            else
                skip = executeWriteJob(nextJob);

            if (!skip)
                ioJobPending = true;
        }

        if (skip) {
            Log.w(TAG, "Skipping: " + nextJob.entry.type);

            /*
                BluetoothGatt.[read|write][Characteristic|Descriptor]() immediately
                return in cases where meta data doesn't match the intended action
                (e.g. trying to write to read-only char). When this happens
                we have to report an error back to Qt. This is not required during
                the initial service discovery though.
             */
            final boolean isServiceDiscoveryRun = (runningHandle != -1);
            if (!isServiceDiscoveryRun) {
                int handle = -1;
                if (nextJob.entry.type == GattEntryType.Characteristic)
                    handle = handleForCharacteristic(nextJob.entry.characteristic);
                else
                    handle = handleForDescriptor(nextJob.entry.descriptor);

                if (handle != -1) {
                    int errorCode = 0;

                    // The error codes below must be in sync with QLowEnergyService::ServiceError
                    if (nextJob.jobType == IoJobType.Read) {
                        errorCode = (nextJob.entry.type == GattEntryType.Characteristic) ?
                                    5 : 6; // CharacteristicReadError : DescriptorReadError
                    } else {
                        errorCode = (nextJob.entry.type == GattEntryType.Characteristic) ?
                                    2 : 3; // CharacteristicWriteError : DescriptorWriteError
                    }

                    leServiceError(qtObject, handle + 1, errorCode);
                }
            }

            performNextIO();
        }
    }

    // Runs inside the Mutex on readWriteQueue.
    // Returns true if nextJob should be skipped.
    private boolean executeWriteJob(ReadWriteJob nextJob)
    {
        boolean result;
        switch (nextJob.entry.type) {
            case Characteristic:
                if (nextJob.entry.characteristic.getWriteType() != nextJob.requestedWriteType) {
                    nextJob.entry.characteristic.setWriteType(nextJob.requestedWriteType);
                }
                result = nextJob.entry.characteristic.setValue(nextJob.newValue);
                if (!result || !mBluetoothGatt.writeCharacteristic(nextJob.entry.characteristic))
                    return true;
                break;
            case Descriptor:
                if (nextJob.entry.descriptor.getUuid().compareTo(clientCharacteristicUuid) == 0) {
                        /*
                            For some reason, Android splits characteristic notifications
                            into two operations. BluetoothGatt.enableCharacteristicNotification
                            ensures the local Bluetooth stack forwards the notifications. In addition,
                            BluetoothGattDescriptor.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
                            must be written to the peripheral.
                         */


                        /*  There is no documentation on indication behavior. The assumption is
                            that when indication or notification are requested we call
                            BluetoothGatt.setCharacteristicNotification. Furthermore it is assumed
                            indications are send via onCharacteristicChanged too and Android itself
                            will do the confirmation required for an indication as per
                            Bluetooth spec Vol 3, Part G, 4.11 . If neither of the two bits are set
                            we disable the signals.
                         */
                    boolean enableNotifications = false;
                    int value = (nextJob.newValue[0] & 0xff);
                    // first or second bit must be set
                    if (((value & 0x1) == 1) || (((value >> 1) & 0x1) == 1)) {
                        enableNotifications = true;
                    }

                    result = mBluetoothGatt.setCharacteristicNotification(
                            nextJob.entry.descriptor.getCharacteristic(), enableNotifications);
                    if (!result) {
                        Log.w(TAG, "Cannot set characteristic notification");
                        //we continue anyway to ensure that we write the requested value
                        //to the device
                    }

                    Log.d(TAG, "Enable notifications: " + enableNotifications);
                }

                result = nextJob.entry.descriptor.setValue(nextJob.newValue);
                if (!result || !mBluetoothGatt.writeDescriptor(nextJob.entry.descriptor))
                    return true;
                break;
            case Service:
            case CharacteristicValue:
                return true;
        }
        return false;
    }

    // Runs inside the Mutex on readWriteQueue.
    // Returns true if nextJob should be skipped.
    private boolean executeReadJob(ReadWriteJob nextJob)
    {
        boolean result;
        switch (nextJob.entry.type) {
            case Characteristic:
                result = mBluetoothGatt.readCharacteristic(nextJob.entry.characteristic);
                if (!result)
                    return true; // skip
                break;
            case Descriptor:
                result = mBluetoothGatt.readDescriptor(nextJob.entry.descriptor);
                if (!result)
                    return true; // skip
                break;
            case Service:
            case CharacteristicValue:
                return true;
        }
        return false;
    }

    public native void leConnectionStateChange(long qtObject, int wasErrorTransition, int newState);
    public native void leServicesDiscovered(long qtObject, int errorCode, String uuidList);
    public native void leServiceDetailDiscoveryFinished(long qtObject, final String serviceUuid,
                                                        int startHandle, int endHandle);
    public native void leCharacteristicRead(long qtObject, String serviceUuid,
                                            int charHandle, String charUuid,
                                            int properties, byte[] data);
    public native void leDescriptorRead(long qtObject, String serviceUuid, String charUuid,
                                        int descHandle, String descUuid, byte[] data);
    public native void leCharacteristicWritten(long qtObject, int charHandle, byte[] newData,
                                               int errorCode);
    public native void leDescriptorWritten(long qtObject, int charHandle, byte[] newData,
                                           int errorCode);
    public native void leCharacteristicChanged(long qtObject, int charHandle, byte[] newData);
    public native void leServiceError(long qtObject, int attributeHandle, int errorCode);
}

