// Copyright (c) 2018-2019 LG Electronics, Inc.
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

#ifndef BLUEZ5ADVERTISE_H
#define BLUEZ5ADVERTISE_H

#include <string>
#include <glib.h>
#include <vector>
#include <unordered_map>

#include <bluetooth-sil-api.h>

extern "C" {
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5SIL;
class string;

class Bluez5Advertise
{
public:
	Bluez5Advertise(BluezLEAdvertisingManager1 *advManager, Bluez5SIL *sil);
	~Bluez5Advertise();

	Bluez5Advertise(const Bluez5Advertise&) = delete;
	Bluez5Advertise& operator = (const Bluez5Advertise&) = delete;

	void createAdvertisementId(AdvertiserIdStatusCallback callback);
	void registerAdvertisement(uint8_t advId, AdvertiserStatusCallback callback);
	gboolean unRegisterAdvertisement(uint8_t advId);
	void advertiseServiceUuids(uint8_t advId, std::unordered_map<std::string, std::vector<uint8_t> > serviceList);
	void advertiseServiceData(uint8_t advId, const char* uuid, std::vector<uint8_t> serviceData);
	void advertiseManufacturerData(uint8_t advId, std::vector<uint8_t> data);
	void advertiseTxPower(uint8_t advId, bool value);
	void advertiseLocalName(uint8_t advId, const char *name);
	void advertiseAppearance(uint8_t advId, guint16 value);
	void advertiseIncludes(uint8_t advId, bool isTxPowerInclude, bool isLocalNameInclude, bool isAppearanceInclude);
	void advertiseDuration(uint8_t advId, uint16_t value);
	void advertiseTimeout(uint8_t advId, uint16_t value);
	void advertiseDiscoverable(uint8_t advId, bool discoverable);
	void setAdRole(uint8_t advId, std::string role);
	void clearAllProperties(uint8_t advId);
	BluezLEAdvertisement1 *getInteface(uint8_t advId);
	std::string getPath(uint8_t advId);
	void removeAdvertise(uint8_t advId);
	static void handleBusAcquired(GDBusConnection *connection, const gchar *name, gpointer user_data);
	static gboolean handleRelease(BluezLEAdvertisement1 *proxy, GDBusMethodInvocation *invocation, gpointer user_data);

	class AdvertiseObject
	{
	public:
		AdvertiseObject(BluezLEAdvertisement1 *interface, std::string path):
		mInterface(interface),
		mPath(path)
		{
		}

		~AdvertiseObject()
		{
			if (mInterface)
				g_object_unref(mInterface);
		}

		AdvertiseObject(const AdvertiseObject&) = delete;
		AdvertiseObject& operator = (const AdvertiseObject&) = delete;

		BluezLEAdvertisement1 *mInterface;
		std::string mPath;
	};

private:
	void setConnection(GDBusConnection *connection) { mConn = connection; }
	std::string createInterface(uint8_t advId);
	uint8_t nextAdvId();
	guint mBusId;
	bool mTxPower;
	BluezLEAdvertisingManager1 *mAdvManager;
	Bluez5SIL *mSIL;
	GDBusConnection *mConn;
	std::unordered_map <uint8_t, std::unique_ptr <AdvertiseObject> > mAdvertiserMap;
};
#endif
