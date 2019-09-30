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

#ifndef BLUEZ5PROFILEA2DP_H
#define BLUEZ5PROFILEA2DP_H

#include <string>

#include <bluetooth-sil-api.h>
#include "bluez5profilebase.h"

class Bluez5Adapter;
class Bluez5Device;

class Bluez5ProfileA2dp : public Bluez5ProfileBase,
                         public BluetoothA2dpProfile
{
public:
	Bluez5ProfileA2dp(Bluez5Adapter *adapter);
	~Bluez5ProfileA2dp();
	void getProperties(const std::string &address, BluetoothPropertiesResultCallback  callback);
	void getProperty(const std::string &address, BluetoothProperty::Type type,
	                         BluetoothPropertyResultCallback callback);

	BluetoothError startStreaming(const std::string &address);
	BluetoothError stopStreaming(const std::string &address);
	void connect(const std::string &address, BluetoothResultCallback callback);
	void disconnect(const std::string &address, BluetoothResultCallback callback);
	void updateConnectionStatus(const std::string &address, bool status);

	static void handleObjectAdded(GDBusObjectManager *objectManager, GDBusObject *object, void *user_data);
	static void handleObjectRemoved(GDBusObjectManager *objectManager, GDBusObject *object, void *user_data);
	static void handleBluezServiceStarted(GDBusConnection *conn, const gchar *name,
											const gchar *nameOwner, gpointer user_data);
	static void handleBluezServiceStopped(GDBusConnection *conn, const gchar *name,
											gpointer user_data);
	static void handlePropertiesChanged(BluezMediaTransport1 *, gchar *interface,  GVariant *changedProperties,
										GVariant *invalidatedProperties, gpointer userData);
	BluezMediaTransport1* getMediaTransport() { return mInterface; }

private:
	GDBusObjectManager *mObjectManager;
	BluetoothA2dpProfileState mState;
	FreeDesktopDBusProperties *mPropertiesProxy;
	BluezMediaTransport1 *mInterface;
};

#endif
