// Copyright (c) 2014-2019 LG Electronics, Inc.
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

#include <string.h>

#include "bluez5sil.h"
#include "bluez5adapter.h"
#include "bluez5device.h"
#include "bluez5agent.h"
#include "bluez5advertise.h"
#include "logging.h"
#include "asyncutils.h"
#include "dbusutils.h"
#include "bluez5mprisplayer.h"

PmLogContext bluez5LogContext;

static const char* const logContextName = "bluetooth-sil-bluez5";

Bluez5SIL::Bluez5SIL(BluetoothPairingIOCapability capability) :
	nameWatch(0),
	mObjectManager(0),
	mDefaultAdapter(0),
	mAgentManager(0),
	mBleAdvManager(0),
	mProfileManager(0),
	mAgent(0),
	mBleAdvertise(0),
	mCapability(capability),
	mGattManager(0)
{
}

Bluez5SIL::~Bluez5SIL()
{
	if (mAgent)
		delete mAgent;

	if (mBleAdvertise)
		delete mBleAdvertise;
}

void Bluez5SIL::handleBluezServiceStarted(GDBusConnection *conn, const gchar *name,
											  const gchar *nameOwner, gpointer user_data)
{
	Bluez5SIL *sil = static_cast<Bluez5SIL*>(user_data);

	DEBUG("bluez is now available");
	if(!sil)
		return;

	GError *error = 0;
	sil->mObjectManager = g_dbus_object_manager_client_new_sync(conn, G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
										  "org.bluez", "/", NULL, NULL, NULL, NULL, &error);
	if (error)
	{
		ERROR(MSGID_OBJECT_MANAGER_CREATION_FAILED, 0, "Failed to create object manager: %s", error->message);
		g_error_free(error);
		return;
	}

	g_signal_connect(sil->mObjectManager, "object-added", G_CALLBACK(handleObjectAdded), sil);
	g_signal_connect(sil->mObjectManager, "object-removed", G_CALLBACK(handleObjectRemoved), sil);


	GList *objects = g_dbus_object_manager_get_objects(sil->mObjectManager);

	/*Objects may come in any order, first device object then adapter so
	 better to traverse all objects to adapter then other interfaces*/

	for (int n = 0; n < g_list_length(objects); n++)
	{
		auto object = static_cast<GDBusObject*>(g_list_nth(objects, n)->data);
		auto objectPath = g_dbus_object_get_object_path(object);

		auto adapterInterface = g_dbus_object_get_interface(object, "org.bluez.Adapter1");
		if (adapterInterface)
		{
			sil->createAdapter(std::string(objectPath));
		}
	}

	if (sil->observer && !sil->mAdapters.empty())
		sil->observer->adaptersChanged();

	GDBusObject* agentManagerObject = findInterface(objects, "org.bluez.AgentManager1");
	if (agentManagerObject)
	{
		sil->createAgentManager(std::string(g_dbus_object_get_object_path(agentManagerObject)));
	}

	GDBusObject* advertiseManager = findInterface(objects, "org.bluez.LEAdvertisingManager1");
	if (advertiseManager)
	{
		sil->createBleAdvManager(std::string(g_dbus_object_get_object_path(advertiseManager)));
	}

	GDBusObject* gattManager = findInterface(objects, "org.bluez.GattManager1");
	if (gattManager)
	{
		sil->createGattManager(std::string(g_dbus_object_get_object_path(gattManager)));
	}

	GDBusObject* profileManager = findInterface(objects, "org.bluez.ProfileManager1");
	if (profileManager)
	{
		sil->createProfileManager(std::string(g_dbus_object_get_object_path(profileManager)));
	}


	for (int n = 0; n < g_list_length(objects); n++)
	{
		auto object = static_cast<GDBusObject*>(g_list_nth(objects, n)->data);
		auto objectPath = g_dbus_object_get_object_path(object);

		auto deviceInterface = g_dbus_object_get_interface(object, "org.bluez.Device1");
		if (deviceInterface)
		{
			sil->createDevice(std::string(objectPath));
			g_object_unref(deviceInterface);
		}
	}

	for (int n = 0; n < g_list_length(objects); n++)
	{
		auto object = static_cast<GDBusObject*>(g_list_nth(objects, n)->data);
		auto objectPath = g_dbus_object_get_object_path(object);

		auto mediaManager = g_dbus_object_get_interface(object, "org.bluez.Media1");
		if (mediaManager)
		{
			sil->createMediaManager(std::string(objectPath));
		}
	}

	for (int n = 0; n < g_list_length(objects); n++)
	{
		auto object = static_cast<GDBusObject*>(g_list_nth(objects, n)->data);
		g_object_unref(object);
	}
	g_list_free(objects);
}

void Bluez5SIL::handleBluezServiceStopped(GDBusConnection *conn, const gchar *name,
											gpointer user_data)
{
	Bluez5SIL *sil = static_cast<Bluez5SIL*>(user_data);

	DEBUG("bluez disappeared. Stopping until it comes back.");

	if (!sil)
		return;

	sil->mDefaultAdapter = 0;

	// Drop all adapters as they are invalid now. We will recreate them once
	// bluez comes back.
	for (auto iter = sil->mAdapters.begin(); iter != sil->mAdapters.end(); ++iter)
	{
		Bluez5Adapter *adapter = *iter;
		delete adapter;
	}
	sil->mAdapters.clear();

	if (sil->observer)
		sil->observer->adaptersChanged();

	if (sil->mObjectManager)
	{
		g_object_unref(sil->mObjectManager);
		sil->mObjectManager = 0;
	}
}

void Bluez5SIL::handleObjectAdded(GDBusObjectManager *objectManager, GDBusObject *object, void *user_data)
{
	Bluez5SIL *sil = static_cast<Bluez5SIL*>(user_data);

	auto objectPath = g_dbus_object_get_object_path(object);

	auto adapterInterface = g_dbus_object_get_interface(object, "org.bluez.Adapter1");
	if (adapterInterface)
	{
		sil->createAdapter(std::string(objectPath));
		g_object_unref(adapterInterface);
	}

	auto deviceInterface = g_dbus_object_get_interface(object, "org.bluez.Device1");
	if (deviceInterface)
	{
		sil->createDevice(std::string(objectPath));
		g_object_unref(deviceInterface);
	}

	auto mediaManagerInterface = g_dbus_object_get_interface(object, "org.bluez.Media1");
	if (mediaManagerInterface)
	{
		sil->createMediaManager(std::string(objectPath));
		g_object_unref(mediaManagerInterface);
	}
}

void Bluez5SIL::handleObjectRemoved(GDBusObjectManager *objectManager, GDBusObject *object, void *user_data)
{
	Bluez5SIL *sil = static_cast<Bluez5SIL*>(user_data);

	auto objectPath = g_dbus_object_get_object_path(object);

	auto adapterInterface = g_dbus_object_get_interface(object, "org.bluez.Adapter1");
	if (adapterInterface)
	{
		sil->removeAdapter(std::string(objectPath));
		g_object_unref(adapterInterface);
	}

	auto deviceInterface = g_dbus_object_get_interface(object, "org.bluez.Device1");
	if (deviceInterface)
	{
		sil->removeDevice(std::string(objectPath));
		g_object_unref(deviceInterface);
	}

	auto agentManagerInterface = g_dbus_object_get_interface(object, "org.bluez.AgentManager1");
	if (agentManagerInterface)
	{
		sil->removeAgentManager(std::string(objectPath));
		g_object_unref(agentManagerInterface);
	}

	auto mediaManagerInterface = g_dbus_object_get_interface(object, "org.bluez.Media1");
	if (mediaManagerInterface)
	{
		sil->removeMediaManager(std::string(objectPath));
		g_object_unref(mediaManagerInterface);
	}
}

GDBusObject* Bluez5SIL::findInterface(GList* objects, const gchar *interfaceName)
{
	for (int n = 0; n < g_list_length(objects); n++)
	{
		GDBusObject* object = static_cast<GDBusObject*>(g_list_nth(objects, n)->data);
		const gchar * objectPath = g_dbus_object_get_object_path(object);
		auto interface = g_dbus_object_get_interface(object, interfaceName);
		if (interface)
		{
			g_object_unref(interface);
			return object;
		}
	}
	return nullptr;
}

void Bluez5SIL::assignNewDefaultAdapter()
{
	for (auto it = mAdapters.begin(); it != mAdapters.end(); it++)
	{
		Bluez5Adapter *adapter = *it;
		std::size_t found = adapter->getObjectPath().find("hci0");
		if (found != std::string::npos)
			mDefaultAdapter = adapter;
	}
}

void Bluez5SIL::createAdapter(const std::string &objectPath)
{
	DEBUG("New adapter on path %s", objectPath.c_str());

	Bluez5Adapter *adapter = new Bluez5Adapter(std::string(objectPath));
	mAdapters.push_back(adapter);

	assignNewDefaultAdapter();

	if (mAgent)
		adapter->assignAgent(mAgent);

	if (observer)
		observer->adaptersChanged();

	if (mBleAdvertise)
		adapter->assignBleAdvertise(mBleAdvertise);

	if (mGattManager)
		adapter->assignGattManager(mGattManager);

	if (mProfileManager)
		adapter->assignProfileManager(mProfileManager);
}

void Bluez5SIL::removeAdapter(const std::string &objectPath)
{
	DEBUG("Remove adapter on path %s", objectPath.c_str());

	for (auto adapter : mAdapters)
	{
		if (adapter->getObjectPath() == objectPath)
		{
			if (mDefaultAdapter == adapter)
				mDefaultAdapter = 0;

			mAdapters.remove(adapter);
			delete adapter;

			break;
		}
	}

	assignNewDefaultAdapter();

	if (observer)
		observer->adaptersChanged();
}

Bluez5Adapter* Bluez5SIL::findAdapterForObjectPath(const std::string &objectPath)
{
	for (auto adapter : mAdapters)
	{
		auto adapterPath = adapter->getObjectPath();
		if (!objectPath.compare(0, adapterPath.length(), adapterPath))
			return adapter;
	}

	return 0;
}

void Bluez5SIL::createDevice(const std::string &objectPath)
{
	DEBUG("New device on path %s", objectPath.c_str());

	auto adapter = findAdapterForObjectPath(objectPath);
	if (adapter)
		adapter->addDevice(objectPath);
}

void Bluez5SIL::removeDevice(const std::string &objectPath)
{
	DEBUG("Remove device on path %s", objectPath.c_str());

	auto adapter = findAdapterForObjectPath(objectPath);
	if (adapter)
		adapter->removeDevice(objectPath);
}

void Bluez5SIL::createAgentManager(const std::string &objectPath)
{
	if (mAgentManager)
	{
		WARNING(MSGID_MULTIPLE_AGENT_MGR, 0, "Tried to create another agent manager instance");
		return;
	}

	GError *error = 0;
	mAgentManager = bluez_agent_manager1_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
                                                                "org.bluez", objectPath.c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_FAILED_TO_CREATE_AGENT_MGR_PROXY, 0, "Failed to create dbus proxy for agent manager on path %s: %s",
			  objectPath.c_str(), error->message);
		g_error_free(error);
		return;
	}

	mAgent = new Bluez5Agent(mAgentManager, this);

	for (auto adapter : mAdapters)
		adapter->assignAgent(mAgent);
}

void Bluez5SIL::removeAgentManager(const std::string &objectPath)
{
	if (!mAgentManager)
		return;

	for (auto adapter : mAdapters)
		adapter->assignAgent(0);

	delete mAgent;
	mAgent = 0;

	g_object_unref(mAgentManager);
	mAgentManager = 0;
}

void Bluez5SIL::createBleAdvManager(const std::string &objectPath)
{
	if (mBleAdvManager)
	{
		WARNING(MSGID_MULTIPLE_AGENT_MGR, 0, "Tried to create another BleAdvertise manager instance");
		return;
	}

	GError *error = 0;
	mBleAdvManager = bluez_leadvertising_manager1_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
                                                                "org.bluez", objectPath.c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_FAILED_TO_CREATE_AGENT_MGR_PROXY, 0, "Failed to create dbus proxy for agent manager on path %s: %s",
			  objectPath.c_str(), error->message);
		g_error_free(error);
		return;
	}

	mBleAdvertise = new Bluez5Advertise(mBleAdvManager, this);

	for (auto adapter : mAdapters)
		adapter->assignBleAdvertise(mBleAdvertise);

}

void Bluez5SIL::removeBleAdvManager(const std::string &objectPath)
{
	if (!mBleAdvManager)
		return;

	g_object_unref(mBleAdvManager);
	mBleAdvManager = 0;

	for (auto adapter : mAdapters)
		adapter->assignBleAdvertise(0);

	delete mBleAdvertise;
	mBleAdvertise = 0;
}

void Bluez5SIL::createProfileManager(const std::string &objectPath)
{
	if (mProfileManager)
	{
		WARNING(MSGID_MULTIPLE_AGENT_MGR, 0, "Tried to create another profile manager instance");
		return;
	}

	GError *error = 0;
	mProfileManager = bluez_profile_manager1_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
                                                                "org.bluez", objectPath.c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_FAILED_TO_CREATE_AGENT_MGR_PROXY, 0, "Failed to create dbus proxy for profile manager on path %s: %s",
			  objectPath.c_str(), error->message);
		g_error_free(error);
		return;
	}

	for (auto adapter : mAdapters)
		adapter->assignProfileManager(mProfileManager);
}

void Bluez5SIL::removeProfileManager(const std::string &objectPath)
{
	if (!mProfileManager)
		return;

	g_object_unref(mProfileManager);
	mProfileManager = 0;
}

void Bluez5SIL::createGattManager(const std::string &objectPath)
{
	if (mGattManager)
	{
		WARNING(MSGID_MULTIPLE_AGENT_MGR, 0, "Tried to create another BleAdvertise manager instance");
		return;
	}

	GError *error = 0;
	mGattManager = bluez_gatt_manager1_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
                                                                "org.bluez", objectPath.c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_FAILED_TO_CREATE_AGENT_MGR_PROXY, 0, "Failed to create dbus proxy for agent manager on path %s: %s",
			  objectPath.c_str(), error->message);
		g_error_free(error);
		return;
	}

	for (auto adapter : mAdapters)
		adapter->assignGattManager(mGattManager);
}

void Bluez5SIL::createMediaManager(const std::string &objectPath)
{
	auto adapter = findAdapterForObjectPath(objectPath);
	if (adapter)
		adapter->addMediaManager(objectPath);
}

void Bluez5SIL::removeMediaManager(const std::string &objectPath)
{
	auto adapter = findAdapterForObjectPath(objectPath);
	if (adapter)
		adapter->removeMediaManager(objectPath);
}

void Bluez5SIL::connectWithBluez()
{
	if (nameWatch)
	{
		WARNING(MSGID_ALREADY_CONNECTED, 0, "Tried to reconnect with bluez when already connected");
		return;
	}

	DEBUG("Waiting for bluez to be available on the bus");

	nameWatch = g_bus_watch_name(G_BUS_TYPE_SYSTEM, "org.bluez", G_BUS_NAME_WATCHER_FLAGS_NONE,
					 handleBluezServiceStarted, handleBluezServiceStopped, this, NULL);
}

BluetoothAdapter* Bluez5SIL::getDefaultAdapter()
{
	return mDefaultAdapter;
}

std::vector<BluetoothAdapter*> Bluez5SIL::getAdapters()
{
	// As of right now the SIL API requires us to return a vector here which
	// may change in the future but until then we have to do the conversion
	// here.
	std::vector<BluetoothAdapter*> adapters(mAdapters.begin(), mAdapters.end());
	return adapters;
}

Bluez5Adapter * Bluez5SIL::getBluez5Adapter(std::string objectPath)
{
	for (auto adapter: mAdapters)
	{
		std::size_t found = objectPath.find(adapter->getObjectPath());
		if (found != std::string::npos)
			return adapter;
	}

	return nullptr;
}

void Bluez5SIL::checkDbusConnection()
{
	DBusUtils::waitForBus(G_BUS_TYPE_SYSTEM, [this](bool available) {
		if (!available)
			return;

		DEBUG("DBus system bus is available now");

		connectWithBluez();
	});
}

BluetoothSIL* createBluetoothSIL(const int version, BluetoothPairingIOCapability capability)
{
	if (BLUETOOTH_SIL_API_VERSION != version)
		return NULL;

	g_type_init();

	PmLogErr error = PmLogGetContext(logContextName, &bluez5LogContext);
	if (error != kPmLogErr_None)
	{
		fprintf(stderr, "Failed to setup up log context %s\n", logContextName);
	}

	auto sil = new Bluez5SIL(capability);

	// We first have to check for a connection to the dbus daemon as the bluetooth
	// service itself does not have a startup dependency on it so we have to simulate
	// this by trying to connect to it and do that again if it fails after a timeout.
	// Only when we can establish a connection we can proceed with correctly
	// connecting to bluez.
	sil->checkDbusConnection();

	return sil;
}

// vim: noai:ts=4:sw=4:ss=4:expandtab
