// Copyright (c) 2014-2020 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef BLUEZ5DEVICE_H
#define BLUEZ5DEVICE_H

#include <string>
#include <bluetooth-sil-api.h>

extern "C" {
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5Adapter;

class Bluez5Device
{
public:
	Bluez5Device(Bluez5Adapter *adapter, const std::string &objectPath);
	~Bluez5Device();

	Bluez5Device(const Bluez5Device&) = delete;
	Bluez5Device& operator = (const Bluez5Device&) = delete;

	void pair(BluetoothResultCallback callback);
	void unpair(BluetoothResultCallback callback);
	void cancelPairing(BluetoothResultCallback callback);

	void connect(const std::string& uuid, BluetoothResultCallback callback);
	void disconnect(const std::string &uuid, BluetoothResultCallback callback);
	void connect(BluetoothResultCallback callback);
	void disconnect(BluetoothResultCallback callback);
	void connectGatt(BluetoothResultCallback callback);

	std::string getObjectPath() const;

	std::string getName() const;
	std::string getAddress() const;
	uint32_t getClassOfDevice() const;
	BluetoothDeviceType getType() const;
	std::vector<std::string> getUuids() const;
	std::vector<std::string> getMapInstancesName() const;
	std::map<std::string, std::vector<std::string>> getSupportedMessageTypes() const;
	bool getConnected() const;
	Bluez5Adapter* getAdapter() const;
	std::vector<uint8_t> getScanRecord() const;
	std::string getServiceDataUuid() const;
	std::vector<uint8_t> getManufactureData() const;

	BluetoothPropertiesList buildPropertiesList() const;

	static void handlePropertiesChanged(BluezDevice1 *, gchar *interface,  GVariant *changedProperties,
									GVariant *invalidatedProperties, gpointer userData);

	void setPaired (bool paired) { mPaired = paired; }
	bool setDevicePropertySync(const BluetoothProperty& property);
	void setDevicePropertyAsync(const BluetoothProperty& property, BluetoothResultCallback callback);

	static void handleMediaPlayRequest(BluezDevice1 *, gpointer user_data);
	static void handleMediaMetaRequest(BluezDevice1 *, gpointer user_data);

	uint8_t getRemoteTargetFeatures() { return bluez_device1_get_avrcp_tgfeatures(mDeviceProxy); }
	uint8_t getRemoteControllerFeatures() { return bluez_device1_get_avrcp_ctfeatures(mDeviceProxy); }
	void updateConnectedUuid(const std::string& uuid , bool status);

private:
	bool parsePropertyFromVariant(const std::string &key, GVariant *valueVar);
	GVariant* devPropertyValueToVariant(const BluetoothProperty& property);
	std::string devPropertyTypeToString(BluetoothProperty::Type type);
	void updateConnectedRole();
	void updateProfileConnectionStatus(std::vector <std::string> prevConnectedUuis);
	std::vector<std::string> convertToSupportedtypes(std::uint8_t data);
	bool isLittleEndian();
private:
	Bluez5Adapter *mAdapter;
	std::string mName;
	std::string mAlias;
	std::string mAddress;
	std::string mObjectPath;
	uint32_t mClassOfDevice;
	BluetoothDeviceType mType;
	std::vector<std::string> mUuids;
	std::vector<std::string> mMapInstancesName;
	std::map <std::string, std::vector<std::string>> mMapSupportedMessageTypes;
	std::vector <std::string> mConnectedUuids;
	std::vector <std::uint8_t> mManufacturerData;
	struct ServiceData
	{
		std::string mServiceDataUuid;
		std::vector <std::uint8_t> mScanRecord;
	} ;

	ServiceData mServiceData;
	bool mPaired;
	BluezDevice1 *mDeviceProxy;
	FreeDesktopDBusProperties *mPropertiesProxy;
	bool mConnected;
	bool mTrusted;
	bool mBlocked;
	int mTxPower;
	int mRSSI;
	uint32_t mConnectedRole;
};

#endif // BLUEZ5DEVICE_H
