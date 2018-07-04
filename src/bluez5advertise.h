// Copyright (c) 2018 LG Electronics, Inc.
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
	void registerAdvertisement(BluetoothResultCallback callback);
	void unRegisterAdvertisement();
	void advertiseServiceUuids(std::unordered_map<std::string, std::vector<uint8_t> > serviceList);
	void advertiseServiceData(const char* uuid, std::vector<uint8_t> serviceData);
	void advertiseManufacturerData(std::vector<uint8_t> data);
	void advertiseTxPower(bool value);
	void advertiseLocalName(const char *name);
	void advertiseAppearance(guint16 value);
	void advertiseIncludes(bool isTxPowerInclude, bool isLocalNameInclude, bool isAppearanceInclude);
	void advertiseDuration(uint16_t value);
	void advertiseTimeout(uint16_t value);
	void setAdRole(std::string role);
	void clearAllProperties();
	static void handleBusAcquired(GDBusConnection *connection, const gchar *name, gpointer user_data);
	static gboolean handleRelease(BluezLEAdvertisement1 *proxy, GDBusMethodInvocation *invocation, gpointer user_data);

private:
	void createInterface(GDBusConnection *connection);
	guint mBusId;
	bool mTxPower;
	BluezLEAdvertisement1 *mInterface;
	BluezLEAdvertisingManager1 *mAdvManager;
	std::string mPath;
	Bluez5SIL *mSIL;
};
#endif
