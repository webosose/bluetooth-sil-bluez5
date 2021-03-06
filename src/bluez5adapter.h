// Copyright (c) 2014-2021 LG Electronics, Inc.
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

#ifndef BLUEZ5ADAPTER_H
#define BLUEZ5ADAPTER_H

#include <unistd.h>
#include <glib.h>
#include <list>
#include <unordered_map>
#include <map>

#include <bluetooth-sil-api.h>
#include "bluez5device.h"
#include "bluez5advertise.h"

extern "C" {
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

enum FilterTypes{
	FILTER_NAME = 0x01,
	FILTER_ADDRESS = 0x02,
	FILTER_SERVICEUUID = 0x04,
	FILTER_SERVICEDATA = 0x08,
	FILTER_MANUFACTURERDATA = 0x10,
	FILTER_SERVICEUUIDMASK = 0x20,
	FILTER_NONE = 0x40
};

class Bluez5Agent;
class Bluez5ObexClient;
class Bluez5ObexAgent;
class Bluez5MprisPlayer;

class Bluez5Adapter : public BluetoothAdapter
{
public:
	Bluez5Adapter(const std::string &objectPath);
	~Bluez5Adapter();

	Bluez5Adapter(const Bluez5Adapter&) = delete;
	Bluez5Adapter& operator = (const Bluez5Adapter&) = delete;

	void getAdapterProperties(BluetoothPropertiesResultCallback callback);
	void getAdapterProperty(BluetoothProperty::Type type, BluetoothPropertyResultCallback callback);
	void setAdapterProperty(const BluetoothProperty& property, BluetoothResultCallback callback);
	void setAdapterProperties(const BluetoothPropertiesList& newProperties, BluetoothResultCallback callback);
	bool setAdapterDelayReport(bool delayReporting);
	bool getAdapterDelayReport(bool &delayReporting);
	void getDeviceProperties(const std::string& address, BluetoothPropertiesResultCallback callback);
	void setDeviceProperty(const std::string& address, const BluetoothProperty& property, BluetoothResultCallback callback);
	void setDeviceProperties(const std::string& address, const BluetoothPropertiesList& properties, BluetoothResultCallback callback);
	BluetoothError enable();
	BluetoothError forceRepower();
	BluetoothError disable();
	BluetoothError startDiscovery();
	void cancelDiscovery(BluetoothResultCallback callback);
	bool isServiceUuidValid(const BluetoothLeDiscoveryFilter &filter);
	bool isServiceDataValid(const BluetoothLeDiscoveryFilter &filter);
	bool isFilterValid(const BluetoothLeDiscoveryFilter &filter);
	int32_t addLeDiscoveryFilter(const BluetoothLeDiscoveryFilter &filter);
	BluetoothError removeLeDiscoveryFilter(uint32_t scanId);
	void removeFilterType(uint32_t scanId);
	BluetoothError prepareFilterForDiscovery();
	void matchLeDiscoveryFilterDevices(const BluetoothLeDiscoveryFilter &filter, uint32_t scanId);
	BluetoothError startLeDiscovery();
	BluetoothError cancelLeDiscovery();
	BluetoothProfile* getProfile(const std::string& name);

	void pair(const std::string &address, BluetoothResultCallback callback);
	BluetoothError supplyPairingConfirmation(const std::string &address, bool accept);
	BluetoothError supplyPairingSecret(const std::string &address, const std::string &pin);
	BluetoothError supplyPairingSecret(const std::string &address, BluetoothPasskey passkey);
	void unpair(const std::string &address, BluetoothResultCallback callback);
	void cancelPairing(const std::string &address, BluetoothResultCallback callback);

	void addDevice(const std::string &objectPath);
	void removeDevice(const std::string &objectPath);
	Bluez5Device* findDeviceByObjectPath(const std::string &objectPath);
	Bluez5Device* findDevice(const std::string &address);
	bool filterMatchCriteria(const BluetoothLeDiscoveryFilter &filter, Bluez5Device *device);
	bool stopLeDiscovery();
	bool resumeLeDiscovery();
	bool setBluezFilter(const BluetoothLeDiscoveryFilter &filter);
	bool clearPreviousFilter();
	bool bluezFilterUsageCriteria(unsigned char mFilterType);
	bool checkServiceUuid(const BluetoothLeDiscoveryFilter &filter, Bluez5Device *device);
	bool checkServiceData(const BluetoothLeDiscoveryFilter &filter, Bluez5Device *device);
	bool checkManufacturerData(const BluetoothLeDiscoveryFilter &filter, Bluez5Device *device);

	void assignAgent(Bluez5Agent *agent);
	Bluez5Agent *getAgent();

	void assignBleAdvertise(Bluez5Advertise *advertise);
	Bluez5Advertise *getAdvertise();

	BluezGattManager1 *getGattManager();

	void assignProfileManager(BluezProfileManager1* proxy);
	BluezProfileManager1 *getProfileManager();

	Bluez5MprisPlayer* getPlayer();

	bool isPairingFor(const std::string &address) const;
	bool isPairing() const;

	std::string getObjectPath() const;

	void handleDevicePropertiesChanged(Bluez5Device *device);

	static void handleAdapterPropertiesChanged(BluezAdapter1 *, gchar *interface,  GVariant *changedProperties,
											   GVariant *invalidatedProperties, gpointer userData);

	void reportPairingResult(bool success);

	BluetoothAdapterStatusObserver* getObserver() { return observer; }

	void setPairing(bool pairing) { mPairing = pairing; }

	Bluez5ObexClient* getObexClient() const { return mObexClient; }

	static gboolean handleDiscoveryTimeout(gpointer user_data);

	virtual void configureAdvertisement(bool connectable, bool includeTxPower, bool includeName,
	                                    BluetoothLowEnergyData manufacturerData, BluetoothLowEnergyServiceList services,
	                                    BluetoothResultCallback callback, uint8_t TxPower, BluetoothUuid solicitedService128);

	virtual void configureAdvertisement(bool connectable, bool includeTxPower, bool includeName, bool isScanResponse,
	                                    BluetoothLowEnergyData manufacturerData, BluetoothLowEnergyServiceList services,
	                                    ProprietaryDataList dataList,
	                                    BluetoothResultCallback callback, uint8_t TxPower, BluetoothUuid solicitedService128);

	virtual void startAdvertising(BluetoothResultCallback callback);
	virtual void startAdvertising(uint8_t advertiserId, const AdvertiseSettings &settings,
					const AdvertiseData &advertiseData, const AdvertiseData &scanResponse, AdvertiserStatusCallback callback);

	virtual void stopAdvertising(BluetoothResultCallback callback);

	virtual BluetoothError updateFirmware(const std::string &deviceName,
			const std::string &fwFileName,
			const std::string &miniDriverName,
			bool isShared);

	virtual BluetoothError resetModule(const std::string &deviceName, bool isShared);
	virtual void registerAdvertiser(AdvertiserIdStatusCallback callback);
	virtual void unregisterAdvertiser(uint8_t advertiserId, AdvertiserStatusCallback callback);
	virtual void disableAdvertiser(uint8_t advertiserId, AdvertiserStatusCallback callback);
	BluezAdapter1* getAdapterProxy() { return mAdapterProxy; }
	std::string getAddress() { return bluez_adapter1_get_address(mAdapterProxy);}
	void updateProfileConnectionStatus(const std::string PROFILE_ID, std::string address, bool isConnected, const std::string &uuid);
	void updateAvrcpVolume(std::string address, guint16 volume);
	void recievePassThroughCommand(std::string address, std::string key, std::string state);
	void mediaPlayStatusRequest(std::string address);
	void mediaMetaDataRequest(std::string address);
	void addMediaManager(std::string objectPath);
	BluezMedia1* getMediaManager() { return mMediaManager; }
	void removeMediaManager(const std::string &objectPath);
	void updateRemoteFeatures(uint8_t features, const std::string &role, const std::string &address);
	void updateSupportedNotificationEvents(uint16_t notificationEvents, const std::string& address);
	void notifyA2dpRoleChnange (const std::string &uuid );
	void notifyAvrcpRoleChange(const std::string &uuid);
	std::vector<std::string> getAdapterSupportedUuid () const{return mUuids;}

private:
	std::string propertyTypeToString(BluetoothProperty::Type type);
	GVariant* propertyValueToVariant(const BluetoothProperty& property);
	bool addPropertyFromVariant(BluetoothPropertiesList& properties, const std::string &key, GVariant *valueVar);
	bool setAdapterPropertySync(const BluetoothProperty& property);
	BluetoothProfile* createProfile(const std::string& profileId);

	void resetDiscoveryTimeout();
	void startDiscoveryTimeout();
	bool isDiscoveryTimeoutRunning();
	uint32_t nextScanId();

private:
	std::string mObjectPath;
	BluezAdapter1 *mAdapterProxy;
	BluezGattManager1 *mGattManagerProxy;
	FreeDesktopDBusProperties *mPropertiesProxy;
	bool mPowered;
	bool mDiscovering;
	bool mSILDiscovery;
	bool mUseBluezFilter;
	bool mLegacyScan;
	unsigned char mFilterType;
	std::unordered_map<std::string, Bluez5Device*> mDevices;
	std::unordered_map<uint32_t, BluetoothLeDiscoveryFilter> mLeScanFilters;
	std::unordered_map<uint32_t, unsigned char> mLeScanFilterTypes;
	std::unordered_map<uint32_t, std::unordered_map<std::string, Bluez5Device*>> mLeDevicesByScanId;
	uint32_t mDiscoveryTimeout;
	guint mDiscoveryTimeoutSource;
	Bluez5Agent *mAgent;
	Bluez5Advertise *mAdvertise;
	BluezProfileManager1 *mProfileManager;
	bool mPairing;
	Bluez5Device *mCurrentPairingDevice;
	BluetoothResultCallback mCurrentPairingCallback;
	std::map<std::string, BluetoothProfile*> mProfiles;
	Bluez5ObexClient *mObexClient;
	std::string mName;
	std::string mAlias;
	std::string mInterfaceName;
	BluetoothResultCallback mCancelDiscCallback;
	bool mAdvertising;
	std::vector <std::string> mUuids;
	Bluez5MprisPlayer *mPlayer;
	BluezMedia1 *mMediaManager;
};

#endif // BLUEZ5ADAPTER_H
