// Copyright (c) 2018-2021 LG Electronics, Inc.
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

const std::string BLUETOOTH_PROFILE_A2DP_SOURCE_UUID = "0000110a-0000-1000-8000-00805f9b34fb";
const std::string BLUETOOTH_PROFILE_A2DP_SINK_UUID = "0000110b-0000-1000-8000-00805f9b34fb";

Bluez5ProfileA2dp::Bluez5ProfileA2dp(Bluez5Adapter *adapter) :
	Bluez5ProfileBase(adapter, BLUETOOTH_PROFILE_A2DP_SINK_UUID),
	mObjectManager(0),
	mPropertiesProxy(0),
	mInterface(nullptr),
	mState(NOT_PLAYING),
	mConnected(false)
{
	mWatcherId = g_bus_watch_name(G_BUS_TYPE_SYSTEM, "org.bluez", G_BUS_NAME_WATCHER_FLAGS_NONE,
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
	if (mPropertiesProxy)
	{
		g_object_unref(mPropertiesProxy);
		mPropertiesProxy = 0;
	}
	/* Stops watching a name */
	if (mWatcherId)
		g_bus_unwatch_name(mWatcherId);
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
	{
		updateA2dpUuid(address, connectCallback);
		Bluez5ProfileBase::connect(address, connectCallback);
	}
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
	updateA2dpUuid(address, disConnectCallback);
	Bluez5ProfileBase::disconnect(address, disConnectCallback);
}

void Bluez5ProfileA2dp::updateA2dpUuid(const std::string &address, BluetoothResultCallback callback)
{
	Bluez5Device *device = mAdapter->findDevice(address);
	if (!device)
	{
		callback(BLUETOOTH_ERROR_PARAM_INVALID);
		return;
	}

#ifndef WEBOS_AUTO
	for(auto tempUuid : device->getUuids())
	{
		if ((tempUuid == BLUETOOTH_PROFILE_A2DP_SOURCE_UUID) ||
			(tempUuid == BLUETOOTH_PROFILE_A2DP_SINK_UUID))
		{
			mUuid = tempUuid;
			break;
		}
	}
#else
	for(auto tempUuid : mAdapter->getAdapterSupportedUuid())
	{
		if ((tempUuid == BLUETOOTH_PROFILE_A2DP_SOURCE_UUID) ||
			(tempUuid == BLUETOOTH_PROFILE_A2DP_SINK_UUID))
		{
			setA2dpUuid(tempUuid);
			break;
		}
	}
#endif
}

void Bluez5ProfileA2dp::setA2dpUuid(const std::string &uuid)
{
	if (uuid == BLUETOOTH_PROFILE_A2DP_SOURCE_UUID)
	{
		mUuid = BLUETOOTH_PROFILE_A2DP_SINK_UUID;
	}
	else if (uuid == BLUETOOTH_PROFILE_A2DP_SINK_UUID)
	{
		mUuid = BLUETOOTH_PROFILE_A2DP_SOURCE_UUID;
	}
}


void Bluez5ProfileA2dp::enable(const std::string &uuid, BluetoothResultCallback callback)
{
#ifdef WEBOS_AUTO
	GError *error = 0;
	std::string role;

	if (uuid == BLUETOOTH_PROFILE_A2DP_SOURCE_UUID)
	{
		role = "source";
		mUuid = BLUETOOTH_PROFILE_A2DP_SINK_UUID;
	}
	else if (uuid == BLUETOOTH_PROFILE_A2DP_SINK_UUID)
	{
		role = "sink";
		mUuid = BLUETOOTH_PROFILE_A2DP_SOURCE_UUID;
	}

	BluezMedia1 *mMediaProxy = mAdapter->getMediaManager();
	if (mMediaProxy)
	{
		bool ret = bluez_media1_call_select_role_sync(mMediaProxy, role.c_str(), NULL, &error);
		if (!ret)
		{
			if (strstr(error->message, "org.bluez.Error.AlreadyExists") == NULL)
			{
				ERROR("A2DP_ENABLE_ROLE", 0, "Role enable %s failed error %s", role.c_str(), error->message);
				if(callback != NULL)
				{
					callback(BLUETOOTH_ERROR_FAIL);
				}
			}
			g_error_free(error);
		}
	}
#endif
	mAdapter->notifyA2dpRoleChnange(uuid);
	if(callback != NULL)
	{
		callback(BLUETOOTH_ERROR_NONE);
	}

}

void Bluez5ProfileA2dp::disable(const std::string &uuid, BluetoothResultCallback callback)
{
#ifdef WEBOS_AUTO
	GError *error = 0;
	std::string enableRole;

	if (uuid == BLUETOOTH_PROFILE_A2DP_SOURCE_UUID)
	{
		enableRole = "sink";
	}
	else if (uuid == BLUETOOTH_PROFILE_A2DP_SINK_UUID)
	{
		enableRole = "source";
	}

	BluezMedia1 *mMediaProxy = mAdapter->getMediaManager();
	if (mMediaProxy)
	{
		bool ret = bluez_media1_call_select_role_sync(mMediaProxy, enableRole.c_str(), NULL, &error);
		if (!ret)
		{
			if (strstr(error->message, "org.bluez.Error.AlreadyExists") == NULL)
			{
				ERROR("A2DP_ENABLE_ROLE", 0, "Role enable %s failed error %s", enableRole.c_str(), error->message);
				callback(BLUETOOTH_ERROR_FAIL);
			}
			g_error_free(error);
		}
	}

	mAdapter->notifyA2dpRoleChnange(uuid);
	callback(BLUETOOTH_ERROR_NONE);
#else
	callback(BLUETOOTH_ERROR_UNSUPPORTED);
#endif
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

void Bluez5ProfileA2dp::updateConnectionStatus(const std::string &address,
											   bool status, const std::string &uuid)
{
	DEBUG("Bluez5ProfileA2dp::updateConnectionStatus: %s = %d", uuid.c_str(), status);
	mConnected = status;
	BluetoothPropertiesList properties;
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::CONNECTED, status));

	if (mState == PLAYING && !status)
	{
		DEBUG("Sending notplaying");
		mState = NOT_PLAYING;
		getA2dpObserver()->stateChanged(convertAddressToLowerCase(
											mAdapter->getAddress()),
										convertAddressToLowerCase(address), mState);
	}

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

		updateTransportProperties(a2dp);
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

		a2dp->mTransportUuid = "";
		g_object_unref(a2dp->mInterface);
		g_object_unref(mediaTransportInterface);
		a2dp->mInterface = 0;
		if (a2dp->mPropertiesProxy)
		{
			g_object_unref(a2dp->mPropertiesProxy);
			a2dp->mPropertiesProxy = 0;
		}
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

		if (key == "UUID")
		{
			GVariant *tmpVar = g_variant_get_variant(valueVar);
			std::string state = g_variant_get_string(tmpVar, NULL);
			DEBUG("A2DP Connected UUID %s", state.c_str());

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
			a2dp->mPropertiesProxy = free_desktop_dbus_properties_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
																		   "org.bluez", objectPath.c_str(), NULL, &error);
			if (error)
			{
				ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "Not able to get property interface");
				g_error_free(error);
				return;
			}

			g_signal_connect(G_OBJECT(a2dp->mPropertiesProxy), "properties-changed", G_CALLBACK(handlePropertiesChanged), a2dp);
			updateTransportProperties(a2dp);
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
void Bluez5ProfileA2dp::updateTransportProperties(Bluez5ProfileA2dp *pA2dp)
{
	DEBUG("A2DP updateTransportProperties");
	GVariant *propsVar;
	GError *error = 0;

	if (!pA2dp->mPropertiesProxy)
		return;

	free_desktop_dbus_properties_call_get_all_sync(pA2dp->mPropertiesProxy, "org.bluez.MediaTransport1", &propsVar, NULL, &error);
	if (!error && propsVar != NULL)
	{
		for (int n = 0; n < g_variant_n_children(propsVar); n++)
		{
			GVariant *propertyVar = g_variant_get_child_value(propsVar, n);
			GVariant *keyVar = g_variant_get_child_value(propertyVar, 0);
			GVariant *valueVar = g_variant_get_child_value(propertyVar, 1);

			std::string key = g_variant_get_string(keyVar, NULL);
			if (key == "UUID")
			{
				GVariant *tmpVar = g_variant_get_variant(valueVar);
				pA2dp->mTransportUuid = g_variant_get_string(tmpVar, NULL);
				DEBUG("A2DP transport Connected UUID %s", pA2dp->mTransportUuid.c_str());
				g_variant_unref(tmpVar);
			}
			g_variant_unref(propertyVar);
			g_variant_unref(keyVar);
			g_variant_unref(valueVar);
		}
	}
	else
	{
		DEBUG("Not able to read MediaTransport1 property interface");
		g_error_free(error);
		return;
	}

}


