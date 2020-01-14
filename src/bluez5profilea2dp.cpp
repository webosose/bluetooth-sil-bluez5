// Copyright (c) 2018-2020 LG Electronics, Inc.
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

#include "bluez5adapter.h"
#include "logging.h"
#include "bluez5profilea2dp.h"
#include "utils.h"

const std::string BLUETOOTH_PROFILE_A2DP_UUID = "0000110b-0000-1000-8000-00805f9b34fb";

Bluez5ProfileA2dp::Bluez5ProfileA2dp(Bluez5Adapter *adapter) :
	Bluez5ProfileBase(adapter, BLUETOOTH_PROFILE_A2DP_UUID),
	mObjectManager(0),
	mPropertiesProxy(0),
	mInterface(nullptr),
	mState(NOT_PLAYING),
	mConnected(false)
{
	g_bus_watch_name(G_BUS_TYPE_SYSTEM, "org.bluez", G_BUS_NAME_WATCHER_FLAGS_NONE,
					 handleBluezServiceStarted, handleBluezServiceStopped, this, NULL);
}


void Bluez5ProfileA2dp::delayReportChanged(const std::string &adapterAddress, const std::string &deviceAddress, guint16 delay)
{
	getA2dpObserver()->delayReportChanged(adapterAddress, deviceAddress, delay);
}

Bluez5ProfileA2dp::~Bluez5ProfileA2dp()
{
	if (mObjectManager)
		g_object_unref(mObjectManager);
}

void Bluez5ProfileA2dp::getProperties(const std::string &address, BluetoothPropertiesResultCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
}

void Bluez5ProfileA2dp::getProperty(const std::string &address, BluetoothProperty::Type type,
	                         BluetoothPropertyResultCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	BluetoothProperty prop(type);
	bool isConnected = false;
	Bluez5Device *device = mAdapter->findDevice(address);

	if (!device)
	{
		callback(BLUETOOTH_ERROR_PARAM_INVALID, prop);
		return;
	}

	if (mInterface)
	{
		const char* deviceObjectPath = bluez_media_transport1_get_device(mInterface);
		if (deviceObjectPath)
		{
			Bluez5Device* interfaceDevice = mAdapter->findDeviceByObjectPath(deviceObjectPath);
			if (interfaceDevice && interfaceDevice == device)
			{
				isConnected = true;
			}
		}
	}

	prop.setValue<bool>(isConnected);
	callback(BLUETOOTH_ERROR_NONE, prop);
}

BluetoothError Bluez5ProfileA2dp::startStreaming(const std::string &address)
{
	return BLUETOOTH_ERROR_NONE;
}

BluetoothError Bluez5ProfileA2dp::stopStreaming(const std::string &address)
{
	return BLUETOOTH_ERROR_NONE;
}

void Bluez5ProfileA2dp::connect(const std::string &address, BluetoothResultCallback callback)
{
	auto connectCallback = [this, address, callback](BluetoothError error) {
		if (error != BLUETOOTH_ERROR_NONE)
		{
			callback(error);
			return;
		}
		DEBUG("A2DP connected succesfully");
		callback(BLUETOOTH_ERROR_NONE);
	};

	if (!mConnected)
		Bluez5ProfileBase::connect(address, connectCallback);
	else
		callback(BLUETOOTH_ERROR_DEVICE_ALREADY_CONNECTED);
}

void Bluez5ProfileA2dp::disconnect(const std::string &address, BluetoothResultCallback callback)
{
	auto disConnectCallback = [this, address, callback](BluetoothError error) {
		if (error != BLUETOOTH_ERROR_NONE)
		{
			callback(error);
			return;
		}
		DEBUG("A2DP disconnected succesfully");
		callback(BLUETOOTH_ERROR_NONE);
	};
	Bluez5ProfileBase::disconnect(address, disConnectCallback);
}

BluetoothError Bluez5ProfileA2dp::setDelayReportingState(bool state)
{
	bool success = mAdapter->setAdapterDelayReport(state);
	if (!success)
		return BLUETOOTH_ERROR_FAIL;

	return BLUETOOTH_ERROR_NONE;
}

BluetoothError Bluez5ProfileA2dp::getDelayReportingState(bool &state)
{
	bool success = mAdapter->getAdapterDelayReport(state);
	if (!success)
		return BLUETOOTH_ERROR_FAIL;

	return BLUETOOTH_ERROR_NONE;
}

void Bluez5ProfileA2dp::updateConnectionStatus(const std::string &address, bool status)
{
	mConnected = status;
	BluetoothPropertiesList properties;
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::CONNECTED, status));

	getObserver()->propertiesChanged(convertAddressToLowerCase(mAdapter->getAddress()), convertAddressToLowerCase(address), properties);
}

void Bluez5ProfileA2dp::handleObjectAdded(GDBusObjectManager *objectManager, GDBusObject *object, void *userData)
{
	Bluez5ProfileA2dp *a2dp = static_cast<Bluez5ProfileA2dp*>(userData);

	std::string objectPath = g_dbus_object_get_object_path(object);

	auto mediaTransportInterface = g_dbus_object_get_interface(object, "org.bluez.MediaTransport1");
	if (mediaTransportInterface)
	{
		auto adapterPath = a2dp->mAdapter->getObjectPath();
		if (objectPath.compare(0, adapterPath.length(), adapterPath))
			return;

		GError *error = 0;
		a2dp->mInterface = bluez_media_transport1_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
													"org.bluez", objectPath.c_str(), NULL, &error);
		if (error)
		{
			DEBUG("Not able to get media transport interface");
			g_error_free(error);
			return;
		}

		a2dp->mPropertiesProxy = free_desktop_dbus_properties_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
																		   "org.bluez", objectPath.c_str(), NULL, &error);
		if (error)
		{
			DEBUG("Not able to get property interface");
			g_error_free(error);
			return;
		}

		g_signal_connect(G_OBJECT(a2dp->mPropertiesProxy), "properties-changed", G_CALLBACK(handlePropertiesChanged), a2dp);

		const char* deviceObjectPath = bluez_media_transport1_get_device(a2dp->mInterface);
		if (deviceObjectPath)
		{
			Bluez5Device* device = a2dp->mAdapter->findDeviceByObjectPath(deviceObjectPath);
			if (device)
			{
				a2dp->updateConnectionStatus(convertAddressToLowerCase(device->getAddress()), true);
				a2dp->mAdapter->updateProfileConnectionStatus(BLUETOOTH_PROFILE_ID_AVRCP, device->getAddress(), true);
				a2dp->mAdapter->handleDevicePropertiesChanged(device);
			}
		}

		g_object_unref(mediaTransportInterface);
	}
}

void Bluez5ProfileA2dp::handleObjectRemoved(GDBusObjectManager *objectManager, GDBusObject *object, void *userData)
{
	Bluez5ProfileA2dp *a2dp = static_cast<Bluez5ProfileA2dp*>(userData);

	std::string objectPath = g_dbus_object_get_object_path(object);

	auto mediaTransportInterface = g_dbus_object_get_interface(object, "org.bluez.MediaTransport1");
	if (mediaTransportInterface)
	{
		auto adapterPath = a2dp->mAdapter->getObjectPath();
		if (objectPath.compare(0, adapterPath.length(), adapterPath))
			return;

		std::size_t pos = objectPath.find("/fd");
		std::string devicePath = objectPath.substr (0, pos);

		Bluez5Device *device = a2dp->mAdapter->findDeviceByObjectPath(devicePath);
		if (!device)
		{
			ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "A2DP device null");
			return;
		}

		if (a2dp->mState == PLAYING)
		{
			a2dp->mState = NOT_PLAYING;
			a2dp->getA2dpObserver()->stateChanged(convertAddressToLowerCase(a2dp->mAdapter->getAddress()), convertAddressToLowerCase(device->getAddress()), a2dp->mState);
		}

		a2dp->updateConnectionStatus(convertAddressToLowerCase(device->getAddress()), false);
		a2dp->mAdapter->updateProfileConnectionStatus(BLUETOOTH_PROFILE_ID_AVRCP, device->getAddress(), false);
		a2dp->mAdapter->handleDevicePropertiesChanged(device);

		g_object_unref(a2dp->mInterface);
		g_object_unref(mediaTransportInterface);
		a2dp->mInterface = 0;
	}
}

void Bluez5ProfileA2dp::handlePropertiesChanged(BluezMediaTransport1 *transportInterface, gchar *interface,  GVariant *changedProperties,
												   GVariant *invalidatedProperties, gpointer userData)
{
	Bluez5ProfileA2dp *a2dp = static_cast<Bluez5ProfileA2dp*>(userData);
	DEBUG("properties changed for interface %s", interface);

	for (int n = 0; n < g_variant_n_children(changedProperties); n++)
	{
		GVariant *propertyVar = g_variant_get_child_value(changedProperties, n);
		GVariant *keyVar = g_variant_get_child_value(propertyVar, 0);
		GVariant *valueVar = g_variant_get_child_value(propertyVar, 1);

		std::string key = g_variant_get_string(keyVar, NULL);

		if (key == "State")
		{
			GVariant *tmpVar = g_variant_get_variant(valueVar);
			std::string state = g_variant_get_string(tmpVar, NULL);
			DEBUG("A2DP State %s", state.c_str());

			if (state == "active")
			{
				a2dp->mState = PLAYING;
			}
			else
			{
				a2dp->mState = NOT_PLAYING;
			}

			if (a2dp->mInterface)
			{
				const char* deviceObjectPath = bluez_media_transport1_get_device(a2dp->mInterface);
				if (deviceObjectPath)
				{
					Bluez5Device* device = a2dp->mAdapter->findDeviceByObjectPath(deviceObjectPath);
					if (device)
					{
						a2dp->getA2dpObserver()->stateChanged(convertAddressToLowerCase(a2dp->mAdapter->getAddress()), convertAddressToLowerCase(device->getAddress()), a2dp->mState);
					}
				}
			}
			g_variant_unref(tmpVar);
		}

		if (key == "Volume")
		{
			GVariant *tmpVar = g_variant_get_variant(valueVar);
			guint16 volume = g_variant_get_uint16(tmpVar);
			DEBUG("A2DP Volume %d", volume);

			if (a2dp->mInterface)
			{
				const char* deviceObjectPath = bluez_media_transport1_get_device(a2dp->mInterface);
				if (deviceObjectPath)
				{
					Bluez5Device* device = a2dp->mAdapter->findDeviceByObjectPath(deviceObjectPath);
					if (device)
					{
						a2dp->mAdapter->updateAvrcpVolume(device->getAddress(), volume);
					}
				}
			}
			g_variant_unref(tmpVar);
		}

		if (key == "Delay")
		{
			GVariant *tmpVar = g_variant_get_variant(valueVar);
			guint16 delay = g_variant_get_uint16(tmpVar);
			DEBUG("A2DP Volume %d", delay);

			if (a2dp->mInterface)
			{
				const char* deviceObjectPath = bluez_media_transport1_get_device(a2dp->mInterface);
				if (deviceObjectPath)
				{
					Bluez5Device* device = a2dp->mAdapter->findDeviceByObjectPath(deviceObjectPath);
					if (device)
					{
						a2dp->delayReportChanged(convertAddressToLowerCase(a2dp->mAdapter->getAddress()), convertAddressToLowerCase(device->getAddress()), delay);
					}
				}
			}
			g_variant_unref(tmpVar);
		}

		g_variant_unref(valueVar);
		g_variant_unref(keyVar);
		g_variant_unref(propertyVar);
	}
}

void Bluez5ProfileA2dp::handleBluezServiceStarted(GDBusConnection *conn, const gchar *name,
											  const gchar *nameOwner, gpointer user_data)
{
	Bluez5ProfileA2dp *a2dp = static_cast<Bluez5ProfileA2dp*>(user_data);

	GError *error = 0;
	a2dp->mObjectManager = g_dbus_object_manager_client_new_sync(conn, G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
										  "org.bluez", "/", NULL, NULL, NULL, NULL, &error);
	if (error)
	{
		ERROR(MSGID_OBJECT_MANAGER_CREATION_FAILED, 0, "Failed to create object manager: %s", error->message);
		g_error_free(error);
		return;
	}

	g_signal_connect(a2dp->mObjectManager, "object-added", G_CALLBACK(handleObjectAdded), a2dp);
	g_signal_connect(a2dp->mObjectManager, "object-removed", G_CALLBACK(handleObjectRemoved), a2dp);

	GList *objects = g_dbus_object_manager_get_objects(a2dp->mObjectManager);
	for (int n = 0; n < g_list_length(objects); n++)
	{
		auto object = static_cast<GDBusObject*>(g_list_nth(objects, n)->data);
		std::string objectPath = g_dbus_object_get_object_path(object);

		auto mediaTransportInterface = g_dbus_object_get_interface(object, "org.bluez.MediaTransport1");
		if (mediaTransportInterface)
		{
			auto adapterPath = a2dp->mAdapter->getObjectPath();
			if (objectPath.compare(0, adapterPath.length(), adapterPath))
				return;

			a2dp->mInterface = bluez_media_transport1_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
																							"org.bluez", objectPath.c_str(), NULL, &error);
			FreeDesktopDBusProperties *propertiesProxy = free_desktop_dbus_properties_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
																		   "org.bluez", objectPath.c_str(), NULL, &error);
			if (error)
			{
				ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "Not able to get property interface");
				g_error_free(error);
				return;
			}

			g_signal_connect(G_OBJECT(propertiesProxy), "properties-changed", G_CALLBACK(handlePropertiesChanged), a2dp);

			const char* deviceObjectPath = bluez_media_transport1_get_device(a2dp->mInterface);
			if (deviceObjectPath)
			{
				Bluez5Device* device = a2dp->mAdapter->findDeviceByObjectPath(deviceObjectPath);
				if (device)
				{
					a2dp->updateConnectionStatus(convertAddressToLowerCase(device->getAddress()), true);
					a2dp->mAdapter->updateProfileConnectionStatus(BLUETOOTH_PROFILE_ID_AVRCP, device->getAddress(), true);
					a2dp->mAdapter->handleDevicePropertiesChanged(device);
				}
			}

			g_object_unref(mediaTransportInterface);
		}
		g_object_unref(object);
	}
	g_list_free(objects);
}

void Bluez5ProfileA2dp::handleBluezServiceStopped(GDBusConnection *conn, const gchar *name,
											gpointer user_data)
{
}
