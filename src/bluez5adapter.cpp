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

#include "asyncutils.h"
#include "logging.h"
#include "bluez5adapter.h"
#include "bluez5device.h"
#include "bluez5agent.h"
#include "bluez5obexclient.h"
#include "bluez5obexagent.h"
#include "bluez5profileftp.h"
#include "utils.h"
#include "bluez5profilegatt.h"
#include "bluez5profilespp.h"
#include "bluez5profileopp.h"
#include "bluez5profilea2dp.h"
#include "bluez5profileavrcp.h"
#include "bluez5mprisplayer.h"

const std::string BASEUUID = "-0000-1000-8000-00805f9b34fb";
const std::string BLUETOOTH_PROFILE_AVRCP_TARGET_UUID = "0000110c-0000-1000-8000-00805f9b34fb";
const std::string BLUETOOTH_PROFILE_AVRCP_REMOTE_UUID = "0000110e-0000-1000-8000-00805f9b34fb";

Bluez5Adapter::Bluez5Adapter(const std::string &objectPath) :
	mObjectPath(objectPath),
	mAdapterProxy(0),
	mGattManagerProxy(0),
	mPropertiesProxy(0),
	mPowered(false),
	mDiscovering(false),
	mDiscoveryTimeout(0),
	mDiscoveryTimeoutSource(0),
	mAgent(0),
	mAdvertise(0),
	mProfileManager(0),
	mPlayer(nullptr),
	mPairing(false),
	mCurrentPairingDevice(0),
	mCurrentPairingCallback(0),
	mObexClient(0),
	mObexAgent(0),
	mCancelDiscCallback(0),
	mAdvertising(false)
{
	GError *error = 0;

	mAdapterProxy = bluez_adapter1_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
													"org.bluez", objectPath.c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_FAILED_TO_CREATE_ADAPTER_PROXY, 0, "Failed to create dbus proxy for adapter on path %s: %s",
			  objectPath.c_str(), error->message);
		g_error_free(error);
		return;
	}

	mPropertiesProxy = free_desktop_dbus_properties_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
													"org.bluez", objectPath.c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_FAILED_TO_CREATE_ADAPTER_PROXY, 0, "Failed to create dbus proxy for adapter on path %s: %s",
			  objectPath.c_str(), error->message);
		g_error_free(error);
		return;
	}

	DEBUG("Successfully created proxy for adapter on path %s", objectPath.c_str());

	g_signal_connect(G_OBJECT(mPropertiesProxy), "properties-changed", G_CALLBACK(handleAdapterPropertiesChanged), this);

	mObexClient = new Bluez5ObexClient;
	mObexAgent = new Bluez5ObexAgent(this);
}

Bluez5Adapter::~Bluez5Adapter()
{
	if (mAdapterProxy)
		g_object_unref(mAdapterProxy);

	if (mPropertiesProxy)
		g_object_unref(mPropertiesProxy);

	if (mObexClient)
		delete mObexClient;
}

bool Bluez5Adapter::isDiscoveryTimeoutRunning()
{
	return (mDiscoveryTimeoutSource != 0);
}

uint32_t Bluez5Adapter::nextScanId()
{
	static uint32_t nextScanId = 1;
	return nextScanId++;
}

void Bluez5Adapter::handleAdapterPropertiesChanged(BluezAdapter1 *, gchar *interface,  GVariant *changedProperties,
												   GVariant *invalidatedProperties, gpointer userData)
{
	auto adapter = static_cast<Bluez5Adapter*>(userData);
	BluetoothPropertiesList properties;
	bool changed = false;

	for (int n = 0; n < g_variant_n_children(changedProperties); n++)
	{
		GVariant *propertyVar = g_variant_get_child_value(changedProperties, n);
		GVariant *keyVar = g_variant_get_child_value(propertyVar, 0);
		GVariant *valueVar = g_variant_get_child_value(propertyVar, 1);

		std::string key = g_variant_get_string(keyVar, NULL);

		changed |= adapter->addPropertyFromVariant(properties ,key, g_variant_get_variant(valueVar));

		g_variant_unref(valueVar);
		g_variant_unref(keyVar);
		g_variant_unref(propertyVar);
	}

	// If state has changed and we're not discovering or powered any more
	// we have to make sure to reset the discovery timeout.
	if (changed && (!adapter->mPowered || !adapter->mDiscovering) && adapter->isDiscoveryTimeoutRunning())
		adapter->resetDiscoveryTimeout();

	if (changed && adapter->observer)
		adapter->observer->adapterPropertiesChanged(properties);
}

bool Bluez5Adapter::addPropertyFromVariant(BluetoothPropertiesList& properties, const std::string &key, GVariant *valueVar)
{
	bool changed = false;

	if (key == "Name")
	{
		// prefer Alias over Name. So if there is a mAlias name, always consider that and do not update Name
		if (mAlias.empty())
		{
			mName = g_variant_get_string(valueVar, NULL);
			DEBUG ("%s: Since alias is empty, get name property as %s", __func__, mName.c_str());
			properties.push_back(BluetoothProperty(BluetoothProperty::Type::NAME, mName));
			changed = true;
		}
	}
	else if (key == "Alias")
	{
		mAlias = g_variant_get_string(valueVar, NULL);
		DEBUG ("%s: Got alias property as %s", __func__, mAlias.c_str());
		properties.push_back(BluetoothProperty(BluetoothProperty::Type::NAME, mAlias));
		changed = true;
	}
	else if (key == "Address")
	{
		std::string address = g_variant_get_string(valueVar, NULL);
		properties.push_back(BluetoothProperty(BluetoothProperty::Type::BDADDR, address));
		changed = true;
	}
	else if (key == "Class")
	{
		uint32_t classOfDevice = g_variant_get_uint32(valueVar);
		properties.push_back(BluetoothProperty(BluetoothProperty::Type::CLASS_OF_DEVICE, classOfDevice));
		changed = true;
	}
	else if (key == "DeviceType")
	{
		BluetoothDeviceType typeOfDevice = (BluetoothDeviceType)g_variant_get_uint32(valueVar);
		properties.push_back(BluetoothProperty(BluetoothProperty::Type::TYPE_OF_DEVICE, typeOfDevice));
		changed = true;
	}
	else if (key == "Discoverable")
	{
		bool discoverable = g_variant_get_boolean(valueVar);
		properties.push_back(BluetoothProperty(BluetoothProperty::Type::DISCOVERABLE, discoverable));
		changed = true;
	}
	else if (key == "DiscoverableTimeout")
	{
		uint32_t discoverableTimeout = g_variant_get_uint32(valueVar);
		properties.push_back(BluetoothProperty(BluetoothProperty::Type::DISCOVERABLE_TIMEOUT, discoverableTimeout));
		changed = true;
	}
	else if (key == "Pairable")
	{
		bool pairable = g_variant_get_boolean(valueVar);
		properties.push_back(BluetoothProperty(BluetoothProperty::Type::PAIRABLE, pairable));
		changed = true;
	}
	else if (key == "PairableTimeout")
	{
		uint32_t pairableTimeout = g_variant_get_uint32(valueVar);
		properties.push_back(BluetoothProperty(BluetoothProperty::Type::PAIRABLE_TIMEOUT, pairableTimeout));
		changed = true;
	}
	else if (key == "UUIDs")
	{
		std::vector<std::string> uuids;

		for (int m = 0; m < g_variant_n_children(valueVar); m++)
		{
			GVariant *uuidVar = g_variant_get_child_value(valueVar, m);

			std::string uuid = g_variant_get_string(uuidVar, NULL);
			uuids.push_back(uuid);

			g_variant_unref(uuidVar);
		}

		properties.push_back(BluetoothProperty(BluetoothProperty::Type::UUIDS, uuids));
		changed = true;
	}
	else if (key == "Powered")
	{
		bool powered = g_variant_get_boolean(valueVar);
		if (powered != mPowered)
		{
			mPowered = powered;
			if (observer)
				observer->adapterStateChanged(mPowered);
		}
	}
	else if (key == "Discovering")
	{
		bool discovering = g_variant_get_boolean(valueVar);
		if (discovering != mDiscovering)
		{
			mDiscovering = discovering;
			getObserver()->discoveryStateChanged(mDiscovering);
			if (mCancelDiscCallback)
				mCancelDiscCallback(BLUETOOTH_ERROR_NONE);
		}
	}

	return changed;
}

void Bluez5Adapter::getAdapterProperties(BluetoothPropertiesResultCallback callback)
{
	auto propertiesGetCallback = [this, callback](GAsyncResult *result) {
		GVariant *propsVar;
		GError *error = 0;
		gboolean ret;

		ret = free_desktop_dbus_properties_call_get_all_finish(mPropertiesProxy, &propsVar, result, &error);
		if (error)
		{
			g_error_free(error);
			callback(BLUETOOTH_ERROR_FAIL, BluetoothPropertiesList());
			return;
		}

		BluetoothPropertiesList properties;

		for (int n = 0; n < g_variant_n_children(propsVar); n++)
		{
			GVariant *propertyVar = g_variant_get_child_value(propsVar, n);
			GVariant *keyVar = g_variant_get_child_value(propertyVar, 0);
			GVariant *valueVar = g_variant_get_child_value(propertyVar, 1);

			std::string key = g_variant_get_string(keyVar, NULL);

			addPropertyFromVariant(properties, key, g_variant_get_variant(valueVar));

			g_variant_unref(valueVar);
			g_variant_unref(keyVar);
			g_variant_unref(propertyVar);
		}

		properties.push_back(BluetoothProperty(BluetoothProperty::Type::DISCOVERY_TIMEOUT, mDiscoveryTimeout));
		properties.push_back(BluetoothProperty(BluetoothProperty::Type::STACK_NAME, std::string("bluez5")));
		properties.push_back(BluetoothProperty(BluetoothProperty::Type::UUIDS, mUuids));

		callback(BLUETOOTH_ERROR_NONE, properties);
	};

	free_desktop_dbus_properties_call_get_all(mPropertiesProxy, "org.bluez.Adapter1", NULL,
						glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(propertiesGetCallback));
}

std::string Bluez5Adapter::propertyTypeToString(BluetoothProperty::Type type)
{
	std::string propertyName;

	switch (type)
	{
	case BluetoothProperty::Type::NAME:
		propertyName = "Name";
		break;
	case BluetoothProperty::Type::ALIAS:
		propertyName = "Alias";
		break;
	case BluetoothProperty::Type::BDADDR:
		propertyName = "Address";
		break;
	case BluetoothProperty::Type::CLASS_OF_DEVICE:
		propertyName = "Class";
		break;
	case BluetoothProperty::Type::TYPE_OF_DEVICE:
		propertyName = "DeviceType";
		break;
	case BluetoothProperty::Type::DISCOVERABLE:
		propertyName = "Discoverable";
		break;
	case BluetoothProperty::Type::DISCOVERABLE_TIMEOUT:
		propertyName = "DiscoverableTimeout";
		break;
	case BluetoothProperty::Type::PAIRABLE:
		propertyName = "Pairable";
		break;
	case BluetoothProperty::Type::PAIRABLE_TIMEOUT:
		propertyName = "PairableTimeout";
		break;
	case BluetoothProperty::Type::DISCOVERY_TIMEOUT:
		propertyName = "DiscoveryTimeout";
		break;
	default:
		break;
	}

	return propertyName;
}

void Bluez5Adapter::getAdapterProperty(BluetoothProperty::Type type, BluetoothPropertyResultCallback callback)
{
	std::string propertyName = propertyTypeToString(type);

	if (propertyName.length() == 0)
	{
		callback(BLUETOOTH_ERROR_PARAM_INVALID, BluetoothProperty());
		return;
	}

	if (type == BluetoothProperty::Type::DISCOVERY_TIMEOUT)
	{
		callback(BLUETOOTH_ERROR_NONE, BluetoothProperty(type, mDiscoveryTimeout));
		return;
	}
	else if (type == BluetoothProperty::Type::STACK_NAME)
	{
		callback(BLUETOOTH_ERROR_NONE, BluetoothProperty(type, std::string("bluez5")));
		return;
	}

	auto propertyGetCallback = [this, callback, type](GAsyncResult *result) {
		GVariant *propVar, *realPropVar;
		GError *error = 0;
		gboolean ret;

		ret = free_desktop_dbus_properties_call_get_finish(mPropertiesProxy, &propVar, result, &error);
		if (error)
		{
			g_error_free(error);
			callback(BLUETOOTH_ERROR_FAIL, BluetoothProperty());
			return;
		}

		realPropVar = g_variant_get_variant(propVar);

		BluetoothProperty property(type);
		std::string strValue;
		uint32_t uint32Value;
		bool boolValue;

		switch (type)
		{
		case BluetoothProperty::Type::NAME:
			if (mAlias.empty())
			{
				mName = g_variant_get_string(realPropVar, NULL);
				DEBUG ("%s: Alias is empty, so get property returns name as %s", __func__, mName.c_str());
				property.setValue(mName);
			}
			else
			{
				DEBUG ("%s: Alias name available, so get property returns name as %s", __func__, mAlias.c_str());
				property.setValue(mAlias);
			}
			break;

		case BluetoothProperty::Type::ALIAS:
			mAlias = g_variant_get_string(realPropVar, NULL);
			DEBUG ("%s: Got alias property as %s", __func__, mAlias.c_str());
			property.setValue(mAlias);
			break;

		case BluetoothProperty::Type::BDADDR:
			strValue = g_variant_get_string(realPropVar, NULL);
			property.setValue(strValue);
			break;
		case BluetoothProperty::Type::CLASS_OF_DEVICE:
		case BluetoothProperty::Type::DISCOVERABLE_TIMEOUT:
		case BluetoothProperty::Type::PAIRABLE_TIMEOUT:
		case BluetoothProperty::Type::TYPE_OF_DEVICE:
			uint32Value = g_variant_get_uint32(realPropVar);
			property.setValue(uint32Value);
			break;
		case BluetoothProperty::Type::DISCOVERABLE:
		case BluetoothProperty::Type::PAIRABLE:
			boolValue = g_variant_get_boolean(realPropVar);
			property.setValue(boolValue);
			break;
		default:
			callback(BLUETOOTH_ERROR_FAIL, BluetoothProperty());
			return;
		}

		g_variant_unref(realPropVar);
		g_variant_unref(propVar);

		callback(BLUETOOTH_ERROR_NONE, property);
	};

	free_desktop_dbus_properties_call_get(mPropertiesProxy, "org.bluez.Adapter1", propertyName.c_str(), NULL,
						glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(propertyGetCallback));
}

GVariant* Bluez5Adapter::propertyValueToVariant(const BluetoothProperty& property)
{
	GVariant *valueVar = 0;

	switch (property.getType())
	{
	case BluetoothProperty::Type::NAME:
	case BluetoothProperty::Type::ALIAS:
	case BluetoothProperty::Type::BDADDR:
		valueVar = g_variant_new_string(property.getValue<std::string>().c_str());
		break;
	case BluetoothProperty::Type::CLASS_OF_DEVICE:
	case BluetoothProperty::Type::DISCOVERABLE_TIMEOUT:
	case BluetoothProperty::Type::PAIRABLE_TIMEOUT:
	case BluetoothProperty::Type::TYPE_OF_DEVICE:
		valueVar = g_variant_new_uint32(property.getValue<uint32_t>());
		break;
	case BluetoothProperty::Type::DISCOVERABLE:
	case BluetoothProperty::Type::PAIRABLE:
		valueVar = g_variant_new_boolean(property.getValue<bool>());
		break;
	default:
		break;
	}

	return valueVar;
}

bool Bluez5Adapter::setAdapterPropertySync(const BluetoothProperty& property)
{
	std::string propertyName = propertyTypeToString(property.getType());

	DEBUG ("%s: property name is %s", __func__, propertyName.c_str());

	if (property.getType() == BluetoothProperty::Type::DISCOVERY_TIMEOUT)
	{
		mDiscoveryTimeout = property.getValue<uint32_t>();

		if (observer)
		{
			BluetoothPropertiesList properties;
			properties.push_back(property);
			observer->adapterPropertiesChanged(properties);
		}

		return true;
	}

	GVariant *valueVar = propertyValueToVariant(property);
	if (!valueVar)
		return false;

	GError *error = 0;
	free_desktop_dbus_properties_call_set_sync(mPropertiesProxy, "org.bluez.Adapter1", propertyName.c_str(),
						g_variant_new_variant(valueVar), NULL, &error);
	if (error)
	{
		DEBUG ("%s: error is %s", __func__, error->message);
		g_error_free(error);
		return false;
	}

	return true;
}

void Bluez5Adapter::setAdapterProperty(const BluetoothProperty& property, BluetoothResultCallback callback)
{
	if (!setAdapterPropertySync(property))
	{
		callback(BLUETOOTH_ERROR_FAIL);
		return;
	}

	callback(BLUETOOTH_ERROR_NONE);
}

void Bluez5Adapter::setAdapterProperties(const BluetoothPropertiesList& properties, BluetoothResultCallback callback)
{
	for (auto property : properties)
	{
		if (!setAdapterPropertySync(property))
		{
			callback(BLUETOOTH_ERROR_FAIL);
			return;
		}
	}

	callback(BLUETOOTH_ERROR_NONE);
}

void Bluez5Adapter::getDeviceProperties(const std::string& address, BluetoothPropertiesResultCallback callback)
{
	Bluez5Device *device  = findDevice(address);
	if (!device)
	{
		callback(BLUETOOTH_ERROR_FAIL, BluetoothPropertiesList());
		return;
	}

	callback(BLUETOOTH_ERROR_NONE, device->buildPropertiesList());
}

void Bluez5Adapter::setDeviceProperty(const std::string& address, const BluetoothProperty& property, BluetoothResultCallback callback)
{
	Bluez5Device *device  = findDevice(address);
	if (!device)
	{
		callback(BLUETOOTH_ERROR_FAIL);
		return;
	}

	device->setDevicePropertyAsync(property, callback);
}

void Bluez5Adapter::setDeviceProperties(const std::string& address, const BluetoothPropertiesList& properties, BluetoothResultCallback callback)
{
	Bluez5Device *device  = findDevice(address);
	if (!device)
	{
		callback(BLUETOOTH_ERROR_FAIL);
		return;
	}

	for (auto property : properties)
	{
		if (!device->setDevicePropertySync(property))
		{
			callback(BLUETOOTH_ERROR_FAIL);
			return;
		}
	}
	callback(BLUETOOTH_ERROR_NONE);
}

BluetoothError Bluez5Adapter::enable()
{
	if (mPowered)
		return BLUETOOTH_ERROR_NONE;

	GVariant *valueVar = g_variant_new_boolean(TRUE);
	GError *error = 0;

	free_desktop_dbus_properties_call_set_sync(mPropertiesProxy, "org.bluez.Adapter1",
						"Powered", g_variant_new_variant(valueVar),
						NULL, &error);
	if (error)
	{
		g_error_free(error);
		return BLUETOOTH_ERROR_FAIL;
	}

	return BLUETOOTH_ERROR_NONE;
}

BluetoothError Bluez5Adapter::disable()
{
	if (!mPowered)
		return BLUETOOTH_ERROR_NONE;

	GVariant *valueVar = g_variant_new_boolean(FALSE);
	GError *error = 0;

	free_desktop_dbus_properties_call_set_sync(mPropertiesProxy, "org.bluez.Adapter1",
						"Powered", g_variant_new_variant(valueVar),
						NULL, &error);
	if (error)
	{
		g_error_free(error);
		return BLUETOOTH_ERROR_FAIL;
	}

	return BLUETOOTH_ERROR_NONE;
}

gboolean Bluez5Adapter::handleDiscoveryTimeout(gpointer user_data)
{
	Bluez5Adapter *self = static_cast<Bluez5Adapter*>(user_data);

	DEBUG("Discovery has timed out. Stopping it.");

	self->cancelDiscovery([](BluetoothError error) { });

	return FALSE;
}

void Bluez5Adapter::startDiscoveryTimeout()
{
	resetDiscoveryTimeout();

	if (mDiscoveryTimeout > 0)
	{
		DEBUG("Starting discovery timeout with %d seconds", mDiscoveryTimeout);
		mDiscoveryTimeoutSource = g_timeout_add_seconds(mDiscoveryTimeout, Bluez5Adapter::handleDiscoveryTimeout, this);
	}
}

BluetoothError Bluez5Adapter::startDiscovery()
{
	if (mDiscovering)
		return BLUETOOTH_ERROR_NONE;

	DEBUG("Starting device discovery");

	GError *error = 0;
	bluez_adapter1_call_start_discovery_sync(mAdapterProxy, NULL, &error);
	if (error)
	{
		g_error_free(error);
		return BLUETOOTH_ERROR_FAIL;
	}

	startDiscoveryTimeout();

	return BLUETOOTH_ERROR_NONE;
}

void Bluez5Adapter::resetDiscoveryTimeout()
{
	if (mDiscoveryTimeoutSource != 0)
	{
		DEBUG("Stopping discovery timeout");
		g_source_remove(mDiscoveryTimeoutSource);
		mDiscoveryTimeoutSource = 0;
	}
}

void Bluez5Adapter::cancelDiscovery(BluetoothResultCallback callback)
{
	if (!mDiscovering)
	{
		callback(BLUETOOTH_ERROR_NONE);
		return;
	}

	resetDiscoveryTimeout();

	GError *error = 0;
	bluez_adapter1_call_stop_discovery_sync(mAdapterProxy, NULL, &error);
	if (error)
	{
		g_error_free(error);
		callback(BLUETOOTH_ERROR_FAIL);
		return;
	}

	mCancelDiscCallback = callback;
}

int32_t Bluez5Adapter::addLeDiscoveryFilter(const BluetoothLeDiscoveryFilter &filter)
{
	int32_t scanId = -1;
	if (!filter.getServiceUuid().getUuid().empty())
	{
		const BluetoothUuid uUiditem = BluetoothUuid(filter.getServiceUuid().getUuid());
		if (uUiditem.getType() == BluetoothUuid::UNKNOWN)
			return scanId;
		else if (uUiditem.getType() == BluetoothUuid::UUID16)
		{
			if (filter.getServiceUuid().getMask().empty())
			{
				std::string uuid16 = filter.getServiceUuid().getUuid().substr (0,4);
				filter.getServiceUuid().setUuid("0000" + uuid16 + BASEUUID);
			}
			else if (filter.getServiceUuid().getUuid().size() == filter.getServiceUuid().getMask().size())
			{
				std::string uuidmask16 = filter.getServiceUuid().getMask().substr (0,4);
				filter.getServiceUuid().setMask("0000" + uuidmask16 + "-0000-0000-0000-000000000000");
			}
			else if (filter.getServiceUuid().getMask().size() == BLUETOOTH_UUID_32_LENGTH)
			{
				std::string uuidmask32 = filter.getServiceUuid().getMask().substr (0,8);
				filter.getServiceUuid().setMask(uuidmask32 + "-0000-0000-0000-000000000000");
			}
			else if (filter.getServiceUuid().getMask().size() != BLUETOOTH_UUID_128_LENGTH)
				return scanId;
		}
		else if (uUiditem.getType() == BluetoothUuid::UUID32)
		{
			if (filter.getServiceUuid().getMask().empty())
			{
				std::string uuid32 = filter.getServiceUuid().getUuid().substr (0,8);
				filter.getServiceUuid().setUuid(uuid32 + BASEUUID);
			}
			else if (filter.getServiceUuid().getUuid().size() == filter.getServiceUuid().getMask().size())
			{
				std::string uuidmask32 = filter.getServiceUuid().getMask().substr (0,8);
				filter.getServiceUuid().setMask(uuidmask32 + "-0000-0000-0000-000000000000");
			}
			else if (filter.getServiceUuid().getMask().size() == BLUETOOTH_UUID_16_LENGTH)
			{
				std::string uuidmask16 = filter.getServiceUuid().getMask().substr (0,4);
				filter.getServiceUuid().setMask("0000" + uuidmask16 + "-0000-0000-0000-000000000000");
			}
			else if (filter.getServiceUuid().getMask().size() != BLUETOOTH_UUID_128_LENGTH)
				return scanId;
		}
		else if (uUiditem.getType() == BluetoothUuid::UUID128)
		{
			if (filter.getServiceUuid().getMask().size() == BLUETOOTH_UUID_16_LENGTH)
			{
				std::string uuidmask16 = filter.getServiceUuid().getMask().substr (0,4);
				filter.getServiceUuid().setMask("0000" + uuidmask16 + "-0000-0000-0000-000000000000");
			}
			else if (filter.getServiceUuid().getMask().size() == BLUETOOTH_UUID_32_LENGTH)
			{
				std::string uuidmask32 = filter.getServiceUuid().getMask().substr (0,8);
				filter.getServiceUuid().setMask(uuidmask32 + "-0000-0000-0000-000000000000");
			}
			else if ((!filter.getServiceUuid().getMask().empty() && filter.getServiceUuid().getMask().size() != BLUETOOTH_UUID_128_LENGTH))
				return scanId;
		}
	}
	else if (!filter.getServiceUuid().getMask().empty())
		return scanId;

	if (!filter.getServiceData().getUuid().empty() && filter.getServiceData().getData().size() > 0)
	{
		if (filter.getServiceData().getMask().empty() || (filter.getServiceData().getData().size() == filter.getServiceData().getMask().size()))
		{
			const BluetoothUuid uUiditem = BluetoothUuid(filter.getServiceData().getUuid());
			if (uUiditem.getType() == BluetoothUuid::UNKNOWN)
				return scanId;
			else if (uUiditem.getType() == BluetoothUuid::UUID16)
			{
				std::string uuid16 = filter.getServiceData().getUuid().substr (0,4);
				filter.getServiceData().setUuid("0000" + uuid16 + BASEUUID);
			}
			else if (uUiditem.getType() == BluetoothUuid::UUID32)
			{
				std::string uuid32 = filter.getServiceData().getUuid().substr (0,8);
				filter.getServiceData().setUuid(uuid32 + BASEUUID);
			}
		}
		else
			return scanId;
	}
	else if ((!filter.getServiceData().getUuid().empty() && filter.getServiceData().getData().size() < 1) ||
			(filter.getServiceData().getUuid().empty() && filter.getServiceData().getData().size() > 0) ||
			(!filter.getServiceData().getMask().empty()))
			return scanId;

	if (filter.getManufacturerData().getId() > 0 && filter.getManufacturerData().getData().size() > 0)
	{
		if (!filter.getManufacturerData().getMask().empty() && (filter.getManufacturerData().getData().size() != filter.getManufacturerData().getMask().size()))
		{
			return scanId;
		}
	}
	else if ((filter.getManufacturerData().getId() > 0 && filter.getManufacturerData().getData().size() < 1) ||
			(filter.getManufacturerData().getId() < 1 && filter.getManufacturerData().getData().size() > 0) ||
			(!filter.getManufacturerData().getMask().empty()))
			return scanId;

			scanId = nextScanId();
	mLeScanFilters.insert(std::pair<uint32_t, BluetoothLeDiscoveryFilter>(scanId, filter));

	return scanId;
}

BluetoothError Bluez5Adapter::removeLeDiscoveryFilter(uint32_t scanId)
{
	auto scanIter = mLeScanFilters.find(scanId);

	if (scanIter == mLeScanFilters.end())
	{
		DEBUG("Failed to remove LE Discovery Filter");
		return BLUETOOTH_ERROR_FAIL;
	}
	else
	{
		mLeScanFilters.erase(scanIter);
		return BLUETOOTH_ERROR_NONE;
	}
}

void Bluez5Adapter::matchLeDiscoveryFilterDevices(const BluetoothLeDiscoveryFilter &filter, uint32_t scanId)
{
	for (auto availableDeviceIter : mDevices)
	{
			Bluez5Device *device = availableDeviceIter.second;
			if (device->getType() == BLUETOOTH_DEVICE_TYPE_BLE && (filterMatchCriteria(filter, device)) && device->getConnected() == false)
			{
				auto devicesIter = mLeDevicesByScanId.find(scanId);
				if (devicesIter == mLeDevicesByScanId.end())
				{
					std::unordered_map<std::string, Bluez5Device*> dev;
					dev.insert(std::pair<std::string, Bluez5Device*>(device->getAddress(), device));

					mLeDevicesByScanId.insert(std::pair<uint32_t, std::unordered_map<std::string, Bluez5Device*>>(scanId, dev));
				}
				else
					(devicesIter->second).insert(std::pair<std::string, Bluez5Device*>(device->getAddress(), device));
				observer->leDeviceFoundByScanId(scanId, device->buildPropertiesList());
			}
	}
}

BluetoothError Bluez5Adapter::startLeDiscovery()
{
	if (mDiscovering)
		return BLUETOOTH_ERROR_NONE;

	DEBUG("Starting LE device discovery");

	GError *error = 0;
	bluez_adapter1_call_start_discovery_sync(mAdapterProxy, NULL, &error);
	if (error)
	{
		g_error_free(error);
		return BLUETOOTH_ERROR_FAIL;
	}
	startDiscoveryTimeout();
	return BLUETOOTH_ERROR_NONE;
}

BluetoothError Bluez5Adapter::cancelLeDiscovery()
{
	if (!mDiscovering)
		return BLUETOOTH_ERROR_NONE;

	if (mLeScanFilters.size() == 0)
	{
		resetDiscoveryTimeout();
		GError *error = 0;
		bluez_adapter1_call_stop_discovery_sync(mAdapterProxy, NULL, &error);
		if (error)
		{
			g_error_free(error);
			return BLUETOOTH_ERROR_FAIL;
		}
	}

	return BLUETOOTH_ERROR_NONE;
}

BluetoothProfile* Bluez5Adapter::createProfile(const std::string& profileId)
{
	Bluez5ProfileBase *profile = nullptr;

	if (profileId == BLUETOOTH_PROFILE_ID_FTP)
	{
		profile = new Bluez5ProfileFtp(this);
		mProfiles.insert(std::pair<std::string,BluetoothProfile*>(BLUETOOTH_PROFILE_ID_FTP, profile));
	}
	else if (profileId == BLUETOOTH_PROFILE_ID_OPP)
	{
		profile = new Bluez5ProfileOpp(this);
		mProfiles.insert(std::pair<std::string,BluetoothProfile*>(BLUETOOTH_PROFILE_ID_OPP, profile));
	}
	else if (profileId == BLUETOOTH_PROFILE_ID_GATT)
	{
		profile = new Bluez5ProfileGatt(this);
		mProfiles.insert(std::pair<std::string,BluetoothProfile*>(BLUETOOTH_PROFILE_ID_GATT, profile));
	}
	else if (profileId == BLUETOOTH_PROFILE_ID_SPP)
	{
		profile = new Bluez5ProfileSpp(this);
		mProfiles.insert(std::pair<std::string,BluetoothProfile*>(BLUETOOTH_PROFILE_ID_SPP, profile));
	}
	else if (profileId == BLUETOOTH_PROFILE_ID_A2DP)
	{
		profile = new Bluez5ProfileA2dp(this);
		mProfiles.insert(std::pair<std::string,BluetoothProfile*>(BLUETOOTH_PROFILE_ID_A2DP, profile));
	}
	else if (profileId == BLUETOOTH_PROFILE_ID_AVRCP)
	{
		profile = new Bluez5ProfileAvcrp(this);
		mProfiles.insert(std::pair<std::string,BluetoothProfile*>(BLUETOOTH_PROFILE_ID_AVRCP, profile));
	}

	if (profile)
	{
		std::string uuid = profile->getProfileUuid();
		// Profile is initialized with remote uuid which its connecting
		if (uuid == BLUETOOTH_PROFILE_AVRCP_REMOTE_UUID)
			mUuids.push_back(BLUETOOTH_PROFILE_AVRCP_TARGET_UUID);
		else
			mUuids.push_back(uuid);
	}

	return profile;
}

BluetoothProfile* Bluez5Adapter::getProfile(const std::string& profileId)
{
	auto iter = mProfiles.find(profileId);

	if (iter == mProfiles.end())
		return createProfile(profileId);

	return iter->second;
}

std::string Bluez5Adapter::getObjectPath() const
{
	return mObjectPath;
}

void Bluez5Adapter::addDevice(const std::string &objectPath)
{
	Bluez5Device *device = new Bluez5Device(this, std::string(objectPath));
	mDevices.insert(std::pair<std::string, Bluez5Device*>(device->getAddress(), device));

	// NOTE: this relies on the device class doing a sync call to its corresponding
	// dbus object which we should refactor later
	if (observer)
	{
		observer->deviceFound(device->buildPropertiesList());
		if (device->getType() == BLUETOOTH_DEVICE_TYPE_BLE)
		{
			for (auto it = mLeScanFilters.begin(); it != mLeScanFilters.end(); ++it)
			{
				if (filterMatchCriteria(it->second, device))
				{
					uint32_t scanId;
					scanId = it->first;
					auto devicesIter = mLeDevicesByScanId.find(scanId);
					if (devicesIter == mLeDevicesByScanId.end())
					{
						std::unordered_map<std::string, Bluez5Device*> dev;
						dev.insert(std::pair<std::string, Bluez5Device*>(device->getAddress(), device));
						mLeDevicesByScanId.insert(std::pair<uint32_t, std::unordered_map<std::string, Bluez5Device*>>(scanId, dev));
					}
					else
						(devicesIter->second).insert(std::pair<std::string, Bluez5Device*>(device->getAddress(), device));
					observer->leDeviceFoundByScanId(scanId, device->buildPropertiesList());
				}
			}
		}
	}
}

void Bluez5Adapter::removeDevice(const std::string &objectPath)
{
	std::string leaddress, address;

	for ( auto it = mLeScanFilters.begin(); it != mLeScanFilters.end(); ++it)
	{
		uint32_t scanId;
		scanId = it->first;
		auto devicesIter = mLeDevicesByScanId.find(scanId);
		if (devicesIter != mLeDevicesByScanId.end())
		{
			for (auto iter = devicesIter->second.begin(); iter != devicesIter->second.end(); ++iter)
			{
				if ((*iter).second->getObjectPath() == objectPath)
				{
					leaddress = (*iter).second->getAddress();
					devicesIter->second.erase(iter);
					break;
				}
			}
			std::string lelowerCaseAddress = convertAddressToLowerCase(leaddress);
			if (lelowerCaseAddress.length() > 0 && observer)
				observer->leDeviceRemovedByScanId(scanId, lelowerCaseAddress);
		}
	}
	for (auto iter = mDevices.begin(); iter != mDevices.end(); ++iter)
	{
		Bluez5Device *device = (*iter).second;
		if (device->getObjectPath() == objectPath)
		{
			address = device->getAddress();
			if (!device->getConnected())
			{
				Bluez5ProfileGatt *gattprofile = dynamic_cast<Bluez5ProfileGatt*> (getProfile(BLUETOOTH_PROFILE_ID_GATT));
				if (gattprofile)
					gattprofile->updateDeviceProperties(address);
			}
			mDevices.erase(iter);
			delete device;
			break;
		}
	}

	std::string lowerCaseAddress = convertAddressToLowerCase(address);

	if (lowerCaseAddress.length() > 0 && observer)
		observer->deviceRemoved(lowerCaseAddress);
}

void Bluez5Adapter::handleDevicePropertiesChanged(Bluez5Device *device)
{
	if (observer)
	{
		if (device->getType() == BLUETOOTH_DEVICE_TYPE_BLE)
		{
			for (auto it = mLeScanFilters.begin(); it != mLeScanFilters.end(); ++it)
			{
				uint32_t scanId;
				scanId = it->first;
				auto devicesIter = mLeDevicesByScanId.find(scanId);
				if (devicesIter != mLeDevicesByScanId.end())
				{
					for (auto iter = devicesIter->second.begin(); iter != devicesIter->second.end(); ++iter)
					{
						Bluez5Device *iterDevice = (*iter).second;
						if (iterDevice->getAddress() == device->getAddress())
						{
							std::string lowerCaseAddress = convertAddressToLowerCase(device->getAddress());
							observer->leDevicePropertiesChangedByScanId(scanId, lowerCaseAddress, device->buildPropertiesList());
							break;
						}
					}
				}
			}
		}
		observer->devicePropertiesChanged(device->getAddress(), device->buildPropertiesList());
	}
}

void Bluez5Adapter::assignAgent(Bluez5Agent *agent)
{
	mAgent = agent;
}

Bluez5Agent* Bluez5Adapter::getAgent()
{
	return mAgent;
}

void Bluez5Adapter::assignBleAdvertise(Bluez5Advertise *advertise)
{
	mAdvertise = advertise;
}

void Bluez5Adapter::assignGattManager(BluezGattManager1 *gattManager)
{
	mGattManagerProxy = gattManager;
}

void Bluez5Adapter::assingPlayer(Bluez5MprisPlayer* player)
{
	mPlayer = player;
}

Bluez5MprisPlayer* Bluez5Adapter::getPlayer()
{
	return mPlayer;
}

BluezGattManager1* Bluez5Adapter::getGattManager()
{
	return mGattManagerProxy;
}

Bluez5Advertise* Bluez5Adapter::getAdvertise()
{
	return mAdvertise;
}

void Bluez5Adapter::assignProfileManager(BluezProfileManager1* proxy)
{
	mProfileManager = proxy;
	return;
}

BluezProfileManager1* Bluez5Adapter::getProfileManager()
{
	return mProfileManager;
}

Bluez5Device* Bluez5Adapter::findDevice(const std::string &address)
{
	std::string upperCaseAddress = convertAddressToUpperCase(address);
	return mDevices.count(upperCaseAddress) ? mDevices[upperCaseAddress] : NULL;
}

bool Bluez5Adapter::filterMatchCriteria(const BluetoothLeDiscoveryFilter &filter, Bluez5Device *device)
{
	bool addressFilter = false;
	bool nameFilter = false;
	bool serviceUuid = false;
	bool serviceData = false;
	bool manuData = false;
	if (!filter.getAddress().empty())
	{
		std::string deviceAddress = device->getAddress();
		std::string filterDevAddress = filter.getAddress();
		deviceAddress = convertToLowerCase(deviceAddress);
		filterDevAddress = convertToLowerCase(filterDevAddress);
		if (deviceAddress == filterDevAddress)
			addressFilter = true;
	}
	else
		addressFilter = true;
	if (!filter.getName().empty())
	{
		std::string deviceName = device->getName();
		std::string filterDevName = filter.getName();
		deviceName = convertToLowerCase(deviceName);
		filterDevName = convertToLowerCase(filterDevName);
		if (deviceName == filterDevName)
			nameFilter = true;
	}
	else
		nameFilter = true;
	if (!filter.getServiceUuid().getUuid().empty())
	{
		serviceUuid = checkServiceUuid(filter, device);
	}
	else
		serviceUuid = true;
	if ((!filter.getServiceData().getUuid().empty()) && (!filter.getServiceData().getData().empty()))
	{
		serviceData = checkServiceData(filter, device);
	}
	else
		serviceData = true;
	if ((filter.getManufacturerData().getId() > 0) && (!filter.getManufacturerData().getData().empty()))
	{
		manuData = checkManufacturerData(filter, device);
	}
	else
		manuData = true;

	if (addressFilter && nameFilter && serviceUuid && serviceData && manuData)
		return true;
	else
		return false;
}

bool Bluez5Adapter::checkServiceUuid(const BluetoothLeDiscoveryFilter &filter, Bluez5Device *device)
{
	if (!filter.getServiceUuid().getMask().empty())
	{
		auto uuids = device->getUuids();
		std::string srvcUuidMasksStr = filter.getServiceUuid().getMask();
		for(int i = 0; i < uuids.size(); i++)
		{
			uuids[i] = convertToLowerCase(uuids[i]);
			std::string srvcUuidStr = filter.getServiceUuid().getUuid();
			srvcUuidStr = convertToLowerCase(srvcUuidStr);
			int srvcUuidSize = filter.getServiceUuid().getUuid().size();
			bool isMatching = true;
			for(int j=0; (j < srvcUuidSize) && isMatching; j++)
			{
				if (srvcUuidMasksStr[j] == '1')
				{
					if(srvcUuidStr[j] != uuids[i][j])
					{
						isMatching = false;
					}
				}

				if (j == srvcUuidSize-1)
					return true;
			}
		}
		return false;
	}
	else
	{
		auto uuids = device->getUuids();
		std::string srvcUuidStr = filter.getServiceUuid().getUuid();
		srvcUuidStr = convertToLowerCase(srvcUuidStr);
		for(int i = 0; i < uuids.size(); i++)
		{
			uuids[i] = convertToLowerCase(uuids[i]);
			if (uuids[i] == srvcUuidStr)
				return true;
		}
	}
	return false;
}

bool Bluez5Adapter::checkServiceData(const BluetoothLeDiscoveryFilter &filter, Bluez5Device *device)
{
	std::string srvcData_Uuid = device->getServiceDataUuid();
	std::string srvcUuidStr = filter.getServiceData().getUuid();
	srvcData_Uuid = convertToLowerCase(srvcData_Uuid);
	srvcUuidStr = convertToLowerCase(srvcUuidStr);
	if (srvcData_Uuid == srvcUuidStr)
	{
		if (!filter.getServiceData().getMask().empty())
		{
			std::vector<uint8_t> srvcDataMask = filter.getServiceData().getMask();
			std::vector<uint8_t> srvcData = filter.getServiceData().getData();
			std::vector<uint8_t> srvcDeviceData = device->getScanRecord();
			int srvcDataSize = filter.getServiceData().getData().size();
			for(int j=0; j < srvcDataSize; j++)
			{
				if (srvcDataMask[j] == 1)
				{
					if(srvcData[j] == srvcDeviceData[j])
					{
						continue;
					}
					else
					{
						return false;
					}
				}
			}
			return true;
		}
		else
		{
			if ((filter.getServiceData().getData()) == device->getScanRecord())
				return true;
		}
	}

	return false;
}

bool Bluez5Adapter::checkManufacturerData(const BluetoothLeDiscoveryFilter &filter, Bluez5Device *device)
{
	if (device->getManufactureData().size() > 2)
	{
		std::vector<uint8_t> manufData = device->getManufactureData();
		uint16_t devManufacturerId = 0;
		uint16_t manufacturerId = (uint16_t)filter.getManufacturerData().getId();

		uint16_t lsb = manufData[1] & 0XFFFF;
		uint16_t msb = manufData[0] << 8;
		devManufacturerId = msb | lsb;
		if (manufacturerId == devManufacturerId)
		{
			std::vector<uint8_t> manuDataMask = filter.getManufacturerData().getMask();
			std::vector<uint8_t> manuData = filter.getManufacturerData().getData();
			std::vector<uint8_t> deviceData = device->getManufactureData();
			int dataSize = filter.getManufacturerData().getData().size();
			if (!filter.getManufacturerData().getMask().empty())
			{
				for(int j=0; j < dataSize; j++)
				{
					if (manuDataMask[j] == 1)
					{
						if(manuData[j] == deviceData[j + 2])
						{
							continue;
						}
						else
						{
							return false;
						}
					}
				}
				return true;
			}
			else
			{
				for(int j=0; j < dataSize; j++)
				{
					if(manuData[j] == deviceData[j + 2])
					{
						continue;
					}
					else
					{
						return false;
					}
				}
				return true;
			}
		}
	}
	return false;
}

Bluez5Device* Bluez5Adapter::findDeviceByObjectPath(const std::string &objectPath)
{
	for(auto deviceIter : mDevices)
	{
		Bluez5Device *device = deviceIter.second;
		if (device->getObjectPath() == objectPath)
		{
			return device;
		}
	}
	return 0;
}

void Bluez5Adapter::reportPairingResult(bool success)
{
}

bool Bluez5Adapter::isPairingFor(const std::string &address) const
{
	if (mPairing && mCurrentPairingDevice)
		return mCurrentPairingDevice->getAddress() == address;

	return false;
}

bool Bluez5Adapter::isPairing() const
{
	return mPairing;
}

void Bluez5Adapter::pair(const std::string &address, BluetoothResultCallback callback)
{
	if (address.length() == 0)
	{
		callback(BLUETOOTH_ERROR_PARAM_INVALID);
		return;
	}

	if (mPairing)
	{
		callback(BLUETOOTH_ERROR_BUSY);
		return;
	}

	Bluez5Device *device = findDevice(address);
	if (!device)
	{
		callback(BLUETOOTH_ERROR_UNKNOWN_DEVICE_ADDR);
		return;
	}

	mPairing = true;
	mCurrentPairingDevice = device;

	device->pair(callback);
}

BluetoothError Bluez5Adapter::supplyPairingConfirmation(const std::string &address, bool accept)
{
	if (!mAgent)
		return BLUETOOTH_ERROR_FAIL;


	DEBUG("Got pairing confirmation: address %s accept %d", address.c_str(), accept);

	if (address.length() == 0)
	{
		return BLUETOOTH_ERROR_PARAM_INVALID;
	}

	if (mAgent->supplyPairingConfirmation(address, accept))
	{
		return BLUETOOTH_ERROR_NONE;
	}
	else
	{
		return BLUETOOTH_ERROR_FAIL;
	}
}

BluetoothError Bluez5Adapter::supplyPairingSecret(const std::string &address, BluetoothPasskey passkey)
{
	if (address.length() == 0)
		return BLUETOOTH_ERROR_PARAM_INVALID;

	if (!mAgent)
		return BLUETOOTH_ERROR_FAIL;

	mAgent->supplyPairingSecret(address, passkey);
	return BLUETOOTH_ERROR_NONE;
}

BluetoothError Bluez5Adapter::supplyPairingSecret(const std::string &address, const std::string &pin)
{
	if (address.length() == 0)
		return BLUETOOTH_ERROR_PARAM_INVALID;

	if (pin.length() == 0)
		return BLUETOOTH_ERROR_PARAM_INVALID;

	if (!mAgent)
		return BLUETOOTH_ERROR_FAIL;

	mAgent->supplyPairingSecret(address, pin);
	return BLUETOOTH_ERROR_NONE;
}

void Bluez5Adapter::unpair(const std::string &address, BluetoothResultCallback callback)
{
	if (address.length() == 0)
	{
		callback(BLUETOOTH_ERROR_PARAM_INVALID);
		return;
	}

	Bluez5Device *device = findDevice(address);
	if (!device)
	{
		callback(BLUETOOTH_ERROR_UNKNOWN_DEVICE_ADDR);
		return;
	}

	auto unPairCallback = [this, callback, address](GAsyncResult *result) {
		GError *error = 0;
		gboolean ret;

		ret = bluez_adapter1_call_remove_device_finish(mAdapterProxy, result, &error);
		if (error)
		{
			g_error_free(error);
			callback(BLUETOOTH_ERROR_FAIL);
			return;
		}

		Bluez5Device *device = findDevice(address);
		if (device)
		{
			device->setPaired(false);
			handleDevicePropertiesChanged(device);
		}
		callback(BLUETOOTH_ERROR_NONE);
	};

	bluez_adapter1_call_remove_device(mAdapterProxy, device->getObjectPath().c_str(), NULL,
	                                  glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(unPairCallback));
}

void Bluez5Adapter::cancelPairing(const std::string &address, BluetoothResultCallback callback)
{
	if (address.length() == 0)
	{
		callback(BLUETOOTH_ERROR_PARAM_INVALID);
		return;
	}

	if (!mPairing)
	{
		callback(BLUETOOTH_ERROR_NOT_READY);
		return;
	}

	Bluez5Device *device = findDevice(address);
	if (!device)
	{
		callback(BLUETOOTH_ERROR_UNKNOWN_DEVICE_ADDR);
		return;
	}

	if (device == mCurrentPairingDevice)
	{
		DEBUG("Canceling outgoing pairing to address %s", address.c_str());
		device->cancelPairing(callback);
	}
	else
	{
		DEBUG("Canceling incoming pairing from address %s", address.c_str());

		if (getAgent()->cancelPairing(address))
		{
			callback(BLUETOOTH_ERROR_NONE);
			setPairing(false);
		}
		else
		{
			callback(BLUETOOTH_ERROR_FAIL);
		}
	}
}

void Bluez5Adapter::configureAdvertisement(bool connectable, bool includeTxPower, bool includeName,
                                           BluetoothLowEnergyData manufacturerData, BluetoothLowEnergyServiceList services,
                                           BluetoothResultCallback callback, uint8_t TxPower, BluetoothUuid solicitedService128)
{
	callback(BLUETOOTH_ERROR_UNSUPPORTED);
	return;
}

void Bluez5Adapter::configureAdvertisement(bool connectable, bool includeTxPower, bool includeName, bool isScanResponse,
                                           BluetoothLowEnergyData manufacturerData, BluetoothLowEnergyServiceList services,
                                           ProprietaryDataList dataList,
                                           BluetoothResultCallback callback, uint8_t TxPower, BluetoothUuid solicitedService128)
{
	callback(BLUETOOTH_ERROR_UNSUPPORTED);
	return;
}

void Bluez5Adapter::startAdvertising(BluetoothResultCallback callback)
{
	callback(BLUETOOTH_ERROR_UNSUPPORTED);
	return;
}

void Bluez5Adapter::startAdvertising(uint8_t advertiserId, const AdvertiseSettings &settings,
					const AdvertiseData &advertiseData, const AdvertiseData &scanResponse, AdvertiserStatusCallback callback)
{
	if (settings.connectable)
	{
		std::string adRole("peripheral");
		mAdvertise->setAdRole(advertiserId, adRole);
		//Set discoverable by default
		mAdvertise->advertiseDiscoverable(advertiserId, true);
	}
	else
	{
		std::string adRole("broadcast");
		mAdvertise->setAdRole(advertiserId, adRole);
	}


	if (settings.timeout > 0)
	{
		mAdvertise->advertiseTimeout(advertiserId, settings.timeout);
	}

	if (scanResponse.includeName || advertiseData.includeTxPower)
	{
		mAdvertise->advertiseIncludes(advertiserId, advertiseData.includeTxPower, scanResponse.includeName, false);
	}

	if (advertiseData.manufacturerData.size() > 1)
	{
		if (advertiseData.manufacturerData.size() > 31)
		{
			DEBUG("Failed to configure advertisements, too much manufacturer data");
			callback(BLUETOOTH_ERROR_PARAM_INVALID);
			return;
		}
		mAdvertise->advertiseManufacturerData(advertiserId, advertiseData.manufacturerData);
	}

	if (advertiseData.services.size() > 0)
	{
		mAdvertise->advertiseServiceUuids(advertiserId, advertiseData.services);
		for (auto it = advertiseData.services.begin(); it != advertiseData.services.end(); it++)
		{
			if ((it->second).size())
				mAdvertise->advertiseServiceData(advertiserId, (it->first).c_str(), it->second);
		}
	}

	mAdvertise->registerAdvertisement(advertiserId, callback);
}

void Bluez5Adapter::stopAdvertising(BluetoothResultCallback callback)
{
	callback(BLUETOOTH_ERROR_UNSUPPORTED);
	return;
}

void Bluez5Adapter::registerAdvertiser(AdvertiserIdStatusCallback callback)
{
	mAdvertise->createAdvertisementId(callback);
}

void Bluez5Adapter::unregisterAdvertiser(uint8_t advertiserId, AdvertiserStatusCallback callback)
{
	gboolean ret = mAdvertise->unRegisterAdvertisement(advertiserId);
	if (ret)
		callback(BLUETOOTH_ERROR_NONE);
	else
		callback(BLUETOOTH_ERROR_FAIL);
}

void Bluez5Adapter::disableAdvertiser(uint8_t advertiserId, AdvertiserStatusCallback callback)
{
	callback(BLUETOOTH_ERROR_NONE);
}

BluetoothError Bluez5Adapter::updateFirmware(const std::string &deviceName,
		const std::string &fwFileName,
		const std::string &miniDriverName,
		bool isShared)
{
	return BLUETOOTH_ERROR_UNSUPPORTED;
}

BluetoothError Bluez5Adapter::resetModule(const std::string &deviceName, bool isShared)
{
	return BLUETOOTH_ERROR_UNSUPPORTED;
}

void Bluez5Adapter::updateProfileConnectionStatus(const std::string PROFILE_ID, std::string address, bool isConnected)
{
	if (PROFILE_ID == BLUETOOTH_PROFILE_ID_AVRCP)
	{
		Bluez5ProfileAvcrp *avrcp = dynamic_cast<Bluez5ProfileAvcrp*> (getProfile(BLUETOOTH_PROFILE_ID_AVRCP));
		if (avrcp) avrcp->updateConnectionStatus(address, isConnected);
	}
	else if (PROFILE_ID == BLUETOOTH_PROFILE_ID_A2DP)
	{
		Bluez5ProfileA2dp *a2dp = dynamic_cast<Bluez5ProfileA2dp*> (getProfile(BLUETOOTH_PROFILE_ID_A2DP));
		if (a2dp) a2dp->updateConnectionStatus(address, isConnected);
	}
}

void Bluez5Adapter::recievePassThroughCommand(std::string address, std::string key, std::string state)
{
	Bluez5ProfileAvcrp *avrcp = dynamic_cast<Bluez5ProfileAvcrp*> (getProfile(BLUETOOTH_PROFILE_ID_AVRCP));
	if (avrcp) avrcp->recievePassThroughCommand(address, key, state);
}

void Bluez5Adapter::mediaPlayStatusRequest(std::string address)
{
	Bluez5ProfileAvcrp *avrcp = dynamic_cast<Bluez5ProfileAvcrp*> (getProfile(BLUETOOTH_PROFILE_ID_AVRCP));
	if (avrcp) avrcp->mediaPlayStatusRequested(address);
}

void Bluez5Adapter::mediaMetaDataRequest(std::string address)
{
	Bluez5ProfileAvcrp *avrcp = dynamic_cast<Bluez5ProfileAvcrp*> (getProfile(BLUETOOTH_PROFILE_ID_AVRCP));
	if (avrcp) avrcp->mediaMetaDataRequested(address);
}
