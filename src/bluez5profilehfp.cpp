// Copyright (c) 2020 LG Electronics, Inc.
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

#include "bluez5profilehfp.h"
#include "bluez5adapter.h"
#include "logging.h"
#include "utils.h"
#include <glib-2.0/glib/gvariant.h>

const std::string BLUETOOTH_PROFILE_REMOTE_HFP_HF_UUID = "0000111e-0000-1000-8000-00805f9b34fb";
const std::string BLUETOOTH_PROFILE_REMOTE_HFP_AG_UUID = "0000111f-0000-1000-8000-00805f9b34fb";

Bluez5ProfileHfp::Bluez5ProfileHfp(Bluez5Adapter *adapter) :
	Bluez5ProfileBase(adapter, BLUETOOTH_PROFILE_REMOTE_HFP_AG_UUID),
	mConnected(false)
{
	g_bus_watch_name(G_BUS_TYPE_SYSTEM, "org.ofono", G_BUS_NAME_WATCHER_FLAGS_NONE,
					 handleOfonoServiceStarted, handleOfonoServiceStopped, this, NULL);
}

Bluez5ProfileHfp::~Bluez5ProfileHfp()
{

}

void Bluez5ProfileHfp::updateConnectionStatus(const std::string &address, bool status)
{
	if (mConnected != status)
	{
		mConnected = status;
		DEBUG("adapter address %s device address %s connection status updated to %d", mAdapter->getAddress().c_str(), address.c_str(), status);
		BluetoothPropertiesList properties;
		properties.push_back(BluetoothProperty(BluetoothProperty::Type::CONNECTED, status));

		getObserver()->propertiesChanged(convertAddressToLowerCase(mAdapter->getAddress()), convertAddressToLowerCase(address), properties);
	}
}

void Bluez5ProfileHfp::getProperty(const std::string &address, BluetoothProperty::Type type,
	                         BluetoothPropertyResultCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	BluetoothProperty prop(type);
	Bluez5Device *device = mAdapter->findDevice(address);

	if (!device)
	{
		callback(BLUETOOTH_ERROR_PARAM_INVALID, prop);
		return;
	}

	prop.setValue<bool>(mConnected);
	callback(BLUETOOTH_ERROR_NONE, prop);
}

void Bluez5ProfileHfp::connect(const std::string &address, BluetoothResultCallback callback)
{
	auto connectCallback = [this, address, callback](BluetoothError error) {
		if (error != BLUETOOTH_ERROR_NONE)
		{
			callback(error);
			return;
		}
		DEBUG("HFP connected succesfully");
		callback(BLUETOOTH_ERROR_NONE);
	};

	if (!mConnected)
		Bluez5ProfileBase::connect(address, connectCallback);
	else
		callback(BLUETOOTH_ERROR_DEVICE_ALREADY_CONNECTED);
}

void Bluez5ProfileHfp::disconnect(const std::string &address, BluetoothResultCallback callback)
{
	auto disConnectCallback = [this, address, callback](BluetoothError error) {
		if (error != BLUETOOTH_ERROR_NONE)
		{
			callback(error);
			return;
		}
		DEBUG("HFP disconnected succesfully");
		callback(BLUETOOTH_ERROR_NONE);
	};

	Bluez5ProfileBase::disconnect(address, disConnectCallback);
}

void Bluez5ProfileHfp::handleOfonoServiceStarted(GDBusConnection *conn, const gchar *name,
											  const gchar *nameOwner, gpointer userData)
{
	DEBUG("connected to org.ofono service");
	Bluez5ProfileHfp *hfp = static_cast<Bluez5ProfileHfp*>(userData);

	GError *error = 0;

	hfp->mOfonoManager = ofono_manager_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE, "org.ofono", "/", NULL, &error);

	GVariant *modems;
	ofono_manager_call_get_modems_sync(hfp->mOfonoManager, &modems, NULL, &error);
	if (error)
	{
		DEBUG("Not able to get modems");
		g_error_free(error);
	}

	hfp->paraseModems(modems);
	g_signal_connect(G_OBJECT(hfp->mOfonoManager), "modem-added", G_CALLBACK(handleModemAdded), hfp);
	g_signal_connect(G_OBJECT(hfp->mOfonoManager), "modem-removed", G_CALLBACK(handleModemRemoved), hfp);
}

void Bluez5ProfileHfp::handleOfonoServiceStopped(GDBusConnection *conn, const gchar *name,
											gpointer userData)
{
	DEBUG("disconnected from org.ofono service");
}

void Bluez5ProfileHfp::paraseModems(GVariant *modems)
{
	g_autoptr(GVariantIter) iter1 = NULL;
	g_variant_get (modems, "a(oa{sv})", &iter1);

	const gchar *string;
	g_autoptr(GVariantIter) iter2 = NULL;
	gboolean hfpInterfaceFound = false;
	GError *error = 0;

	while (g_variant_iter_loop (iter1, "(&oa{sv})", &string, &iter2))
	{
		const gchar *key;
		g_autoptr(GVariant) value = NULL;

		if (!isAdapterMatching(string))
			continue;

		auto ofonoModemProxy = ofono_modem_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, "org.ofono", string, NULL, &error);
		g_signal_connect(G_OBJECT(ofonoModemProxy), "property-changed", G_CALLBACK(handleModemPropertyChanged), this);

		while (g_variant_iter_loop (iter2, "{&sv}", &key, &value))
		{
			std::string keyStr = key;
			if(keyStr == "Serial")
			{
				setConnectedDeviceAddress(g_variant_get_string(value, NULL));
			}
			else if (keyStr == "Interfaces")
			{
				DEBUG("Interface property changed for device %s", getConnectedDeviceAddress().c_str());
				g_autoptr(GVariantIter) iter2;
				gchar *str;

				g_variant_get(value, "as", &iter2);
				while (g_variant_iter_loop (iter2, "s", &str))
				{
					if (std::string(str) ==  "org.ofono.Handsfree")
					hfpInterfaceFound = true;
				}
			}
		}
	}

	if (hfpInterfaceFound)
	{
		updateConnectionStatus(getConnectedDeviceAddress(), true);
	}
	else
	{
		updateConnectionStatus(getConnectedDeviceAddress(), false);
	}
}

void Bluez5ProfileHfp::getModemProperties(OfonoModem *modemProxy)
{
	GError *error = 0;
	GVariant *out;
	ofono_modem_call_get_properties_sync(modemProxy, &out, NULL, &error);
	if (error)
	{
		ERROR(MSGID_OBJECT_MANAGER_CREATION_FAILED, 0, "Failed to call: %s", error->message);
		g_error_free(error);
		//When device is removed modemProxy also removed so update status
		updateConnectionStatus(getConnectedDeviceAddress(), false);
		return;
	}

	g_autoptr(GVariantIter) iter = NULL;
	g_variant_get(out, "a{sv}", &iter);
	gchar *name;
	GVariant *valueVar;
	std::string key;
	gboolean hfpInterfaceFound = false;

	while (g_variant_iter_loop (iter, "{sv}", &name, &valueVar))
	{
		key = name;
		if(key == "Serial")
		{
			setConnectedDeviceAddress(g_variant_get_string(valueVar, NULL));
		}
		else if (key == "Interfaces")
		{
			DEBUG("Interface property changed for device %s", getConnectedDeviceAddress().c_str());
			g_autoptr(GVariantIter) iter2;
			gchar *str;

			g_variant_get(valueVar, "as", &iter2);
			while (g_variant_iter_loop (iter2, "s", &str))
			{
				if (std::string(str) ==  "org.ofono.Handsfree")
					hfpInterfaceFound = true;
			}
		}
	}

	if (hfpInterfaceFound)
	{
		updateConnectionStatus(getConnectedDeviceAddress(), true);
	}
	else
	{
		updateConnectionStatus(getConnectedDeviceAddress(), false);
	}
}

bool Bluez5ProfileHfp::isAdapterMatching(std::string objectPath)
{
	std::string::size_type idx = objectPath.find("/hfp");

	if (idx != std::string::npos)
		objectPath.erase(idx, std::string("/hfp").length());
	else
		return false;

	auto adapterPath = mAdapter->getObjectPath();
	if (objectPath.compare(0, adapterPath.length(), adapterPath))
	{
		DEBUG("Ignoring Handsfree interface for %s", adapterPath.c_str());
		return false;
	}
	else return true;
}

void Bluez5ProfileHfp::handleModemAdded(OfonoManager *object, const gchar *path, GVariant *properties, void *userData)
{
	Bluez5ProfileHfp *hfp = static_cast<Bluez5ProfileHfp*>(userData);
	if (!hfp)
		return;

	if (!hfp->isAdapterMatching(path))
		return;
	
	GError *error = 0;
	GVariant *modems;
	ofono_manager_call_get_modems_sync(hfp->mOfonoManager, &modems, NULL, &error);
	if (error)
	{
		DEBUG("Not able to get modems");
		g_error_free(error);
	}

	hfp->paraseModems(modems);
}
  
void Bluez5ProfileHfp::handleModemRemoved(OfonoManager *object, const gchar *path, void *userData)
{
	Bluez5ProfileHfp *hfp = static_cast<Bluez5ProfileHfp*>(userData);
	if (!hfp)
		return;

	DEBUG("Bluez5ProfileHfp  handleObjectRemoved in ofono %s", path);
}

void Bluez5ProfileHfp::handleModemPropertyChanged(OfonoModem *proxy, char *name, GVariant *v, void *userData)
{
	Bluez5ProfileHfp *hfp = static_cast<Bluez5ProfileHfp*>(userData);
	if (!hfp)
		return;

	std::string key = name;

	if (key == "Interfaces")
		hfp->getModemProperties(proxy);
}
