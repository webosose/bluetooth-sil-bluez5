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

#ifndef BLUEZ5SIL_H
#define BLUEZ5SIL_H

#include <list>

#include <glib.h>
#include <gio/gio.h>
#include <bluetooth-sil-api.h>

extern "C" {
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5Adapter;
class Bluez5Agent;
class Bluez5ObexAgent;

class Bluez5SIL : public BluetoothSIL
{
public:
	Bluez5SIL(BluetoothPairingIOCapability capability);
	~Bluez5SIL();

	Bluez5SIL(const Bluez5SIL&) = delete;
	Bluez5SIL& operator = (const Bluez5SIL&) = delete;

	BluetoothAdapter* getDefaultAdapter();
	std::vector<BluetoothAdapter*> getAdapters();

	Bluez5Adapter *getBluez5AdapterbyAddress(std::string address);
	Bluez5Adapter *getBluez5Adapter(std::string objectPath);
	Bluez5Adapter* getDefaultBluez5Adapter() { return mDefaultAdapter; }
	BluetoothPairingIOCapability getCapability() { return mCapability; }

	static void handleBluezServiceStarted(GDBusConnection *conn, const gchar *name,
										  const gchar *nameOwner, gpointer user_data);
	static void handleBluezServiceStopped(GDBusConnection *conn, const gchar *name,
										  gpointer user_data);

	static void handleObjectAdded(GDBusObjectManager *objectManager, GDBusObject *object, void *user_data);
	static void handleObjectRemoved(GDBusObjectManager *objectManager, GDBusObject *object, void *user_data);

	static GDBusObject* findInterface(GList* objects, const gchar *interface);

	void connectWithBluez();
	void checkDbusConnection();

private:
	void assignNewDefaultAdapter();
	void createAdapter(const std::string &objectPath);
	void removeAdapter(const std::string &objectPath);
	void createObexAgent();
	void deleteObexAgent();
	void createDevice(const std::string &objectPath);
	void removeDevice(const std::string &objectPath);
	void createAgentManager(const std::string &objectPath);
	void removeAgentManager(const std::string &objectPath);
	Bluez5Adapter* findAdapterForObjectPath(const std::string &objectPath);
	void createProfileManager(const std::string &objectPath);
	void removeProfileManager(const std::string &objectPath);
	void createMediaManager(const std::string &objectPath);
	void removeMediaManager(const std::string &objectPath);

private:
	guint nameWatch;
	GDBusObjectManager *mObjectManager;
	std::list<Bluez5Adapter*> mAdapters;
	Bluez5Adapter *mDefaultAdapter;
	BluezAgentManager1 *mAgentManager;
	BluezProfileManager1 *mProfileManager;
	Bluez5Agent *mAgent;
	Bluez5ObexAgent *mObexAgent;
	BluetoothPairingIOCapability mCapability;
};

#endif // BLUEZ5SIL_H
