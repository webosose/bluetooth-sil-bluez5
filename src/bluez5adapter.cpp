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

#include <sys/stat.h>
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
#include "bluez5profilepbap.h"
#include "bluez5profilehfp.h"
#include "bluez5mprisplayer.h"
#include "bluez5profilemap.h"
#include "bluez5profilemesh.h"
#include <fstream>

const std::string BASEUUID = "-0000-1000-8000-00805f9b34fb";
const std::string BLUETOOTH_PROFILE_AVRCP_TARGET_UUID = "0000110c-0000-1000-8000-00805f9b34fb";
const std::string BLUETOOTH_PROFILE_AVRCP_REMOTE_UUID = "0000110e-0000-1000-8000-00805f9b34fb";
const std::string BLUETOOTH_PROFILE_REMOTE_HFP_HF_UUID = "0000111e-0000-1000-8000-00805f9b34fb";
const std::string BLUETOOTH_PROFILE_REMOTE_HFP_AG_UUID = "0000111f-0000-1000-8000-00805f9b34fb";
const std::string BLUETOOTH_PROFILE_A2DP_SOURCE_UUID	= "0000110a-0000-1000-8000-00805f9b34fb";
const std::string BLUETOOTH_PROFILE_A2DP_SINK_UUID = "0000110b-0000-1000-8000-00805f9b34fb";

#define CONFIG "/var/lib/bluetooth/adaptersAssignment.json"

Bluez5Adapter::Bluez5Adapter(const std::string &objectPath) :
	mObjectPath(objectPath),
	mAdapterProxy(0),
	mGattManagerProxy(0),
	mPropertiesProxy(0),
	mPowered(false),
	mDiscovering(false),
	mSILDiscovery(false),
	mUseBluezFilter(false),
	mLegacyScan(false),
	mFilterType(0x00),
	mDiscoveryTimeout(0),
	mDiscoveryTimeoutSource(0),
	mAgent(0),
	mAdvertise(0),
	mProfileManager(0),
	mPairing(false),
	mCurrentPairingDevice(0),
	mCurrentPairingCallback(0),
	mObexClient(0),
	mCancelDiscCallback(0),
	mAdvertising(false),
	mPlayer(nullptr),
	mMediaManager(nullptr)
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

	std::size_t found = mObjectPath.find("hci");
	if (found != std::string::npos)
	{
		mInterfaceName = mObjectPath.substr(found);
	}

	g_signal_connect(G_OBJECT(mPropertiesProxy), "properties-changed", G_CALLBACK(handleAdapterPropertiesChanged), this);

	mObexClient = new Bluez5ObexClient(this);

	BluezLEAdvertisingManager1 *bleAdvManager =
		bluez_leadvertising_manager1_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
		G_DBUS_PROXY_FLAGS_NONE, "org.bluez", objectPath.c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_FAILED_TO_CREATE_AGENT_MGR_PROXY, 0,
			"Failed to create dbus proxy for agent manager on path %s: %s",
			objectPath.c_str(), error->message);
		g_error_free(error);
		return;
	}
	mAdvertise = new (std::nothrow) Bluez5Advertise(bleAdvManager);
	if (!mAdvertise)
	{
		DEBUG("ERROR in creating memory %s", objectPath.c_str());
		g_object_unref(bleAdvManager);
	}

	mGattManagerProxy = bluez_gatt_manager1_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
																"org.bluez", objectPath.c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_FAILED_TO_CREATE_AGENT_MGR_PROXY, 0, "Failed to create dbus proxy for agent manager on path %s: %s",
			  objectPath.c_str(), error->message);
		g_error_free(error);
		return;
	}
}

Bluez5Adapter::~Bluez5Adapter()
{
	for(auto profile = mProfiles.begin(); profile != mProfiles.end(); profile++)
	{
		delete profile->second;
	}

	for (auto device = mDevices.begin(); device != mDevices.end(); device++)
	{
		delete device->second;
	}

	if (mAdapterProxy)
		g_object_unref(mAdapterProxy);

	if (mPropertiesProxy)
		g_object_unref(mPropertiesProxy);

	if (mMediaManager)
		g_object_unref(mMediaManager);

	if (mGattManagerProxy)
		g_object_unref(mGattManagerProxy);

	if (mPlayer)
		delete mPlayer;

	if (mObexClient)
		delete mObexClient;

	if (mAdvertise)
		delete mAdvertise;
}

void Bluez5Adapter::addMediaManager(std::string objectPath)
{
	GError *error = 0;
	mMediaManager = bluez_media1_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
                                                                "org.bluez", objectPath.c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_FAILED_TO_CREATE_AGENT_MGR_PROXY, 0, "Failed to create dbus proxy for media manager on path %s: %s",
			  objectPath.c_str(), error->message);
		g_error_free(error);
		return;
	}

	mPlayer = new Bluez5MprisPlayer(mMediaManager, this);
}

void Bluez5Adapter::removeMediaManager(const std::string &objectPath)
{
	if (!mMediaManager)
		return;
	g_object_unref(mMediaManager);
	mMediaManager = nullptr;

	delete mPlayer;
	mPlayer = nullptr;

}

void Bluez5Adapter::updateRemoteFeatures(uint8_t features, const std::string &role, const std::string &address)
{
	Bluez5ProfileAvcrp *avrcp = dynamic_cast<Bluez5ProfileAvcrp*> (getProfile(BLUETOOTH_PROFILE_ID_AVRCP));
	if (!avrcp)
		return;

	avrcp->updateRemoteFeatures(features, role, address);
}

void Bluez5Adapter::updateSupportedNotificationEvents(uint16_t notificationEvents, const std::string& address)
{
	Bluez5ProfileAvcrp* avrcp = dynamic_cast<Bluez5ProfileAvcrp*> (getProfile(BLUETOOTH_PROFILE_ID_AVRCP));
	if (!avrcp)
		return;

	avrcp->updateSupportedNotificationEvents(notificationEvents, address);
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
                GVariant *realValueVar = g_variant_get_variant(valueVar);

		changed |= adapter->addPropertyFromVariant(properties ,key, realValueVar);

		g_variant_unref(valueVar);
		g_variant_unref(keyVar);
		g_variant_unref(propertyVar);
		g_variant_unref(realValueVar);
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
			properties.push_back(BluetoothProperty(BluetoothProperty::Type::INTERFACE_NAME, mInterfaceName));
			changed = true;
		}
	}
	else if (key == "Alias")
	{
		mAlias = g_variant_get_string(valueVar, NULL);
		DEBUG ("%s: Got alias property as %s", __func__, mAlias.c_str());
#ifdef WEBOS_AUTO
		std::ifstream ifile(CONFIG);
		if (!ifile)
		{
			std::size_t found = mObjectPath.find("hci");

			if (found != std::string::npos)
			{
				mAlias = std::string("sa8155") + " "+ std::string("Bluetooth ") + mObjectPath.substr(found);
				BluetoothProperty alias(BluetoothProperty::Type::ALIAS, mAlias);
				setAdapterPropertySync(alias);

				Bluez5ProfileA2dp *a2dp = dynamic_cast<Bluez5ProfileA2dp*> (getProfile(BLUETOOTH_PROFILE_ID_A2DP));
				if (a2dp)
				{
					if (mAlias == "sa8155 Bluetooth hci2") //AVN
						a2dp->enable(BLUETOOTH_PROFILE_A2DP_SINK_UUID, NULL);
					else // RSE0 RSE1
						a2dp->enable(BLUETOOTH_PROFILE_A2DP_SOURCE_UUID, NULL);
				}
			}
		}
#endif
		properties.push_back(BluetoothProperty(BluetoothProperty::Type::NAME, mAlias));
		properties.push_back(BluetoothProperty(BluetoothProperty::Type::INTERFACE_NAME, mInterfaceName));
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
	else if (key == "Powered")
	{
		bool powered = g_variant_get_boolean(valueVar);
		if (powered != mPowered)
		{
			mPowered = powered;
			if (observer)
			{
				mPowered = powered;
				observer->adapterStateChanged(mPowered);
			}
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
	else if (key == "UUIDs")
	{
		mUuids.clear();

		for (int m = 0; m < g_variant_n_children(valueVar); m++)
		{
			GVariant *uuidVar = g_variant_get_child_value(valueVar, m);

			std::string uuid = g_variant_get_string(uuidVar, NULL);
			mUuids.push_back(std::move(uuid));

			g_variant_unref(uuidVar);
		}
		properties.push_back(BluetoothProperty(BluetoothProperty::Type::UUIDS, mUuids));
		changed = true;
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
			GVariant *realValueVar = g_variant_get_variant(valueVar);

			std::string key = g_variant_get_string(keyVar, NULL);

			addPropertyFromVariant(properties, key, realValueVar);

			g_variant_unref(valueVar);
			g_variant_unref(keyVar);
			g_variant_unref(propertyVar);
			g_variant_unref(realValueVar);
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

	GError *error = 0;

	GVariant *propVar, *realPropVar;
	free_desktop_dbus_properties_call_get_sync(mPropertiesProxy, "org.bluez.Adapter1", propertyName.c_str(), &propVar, NULL, &error);

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
			g_variant_unref(realPropVar);
			g_variant_unref(propVar);
			callback(BLUETOOTH_ERROR_FAIL, BluetoothProperty());
			return;
		}

	g_variant_unref(realPropVar);
	g_variant_unref(propVar);

	callback(BLUETOOTH_ERROR_NONE, property);
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

	if (callback)
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

bool Bluez5Adapter::setAdapterDelayReport(bool delayReporting)
{
	GVariant *valueVar = 0;
	valueVar = g_variant_new_boolean(delayReporting);
	if (!valueVar)
		return false;

	GError *error = 0;

	free_desktop_dbus_properties_call_set_sync(mPropertiesProxy, "org.bluez.Adapter1", "DelayReport",
						g_variant_new_variant(valueVar), NULL, &error);

	if (error)
	{
		DEBUG ("%s: error is %s", __func__, error->message);
		g_error_free(error);
		return false;
	}

	return true;
}

bool Bluez5Adapter::getAdapterDelayReport(bool &delayReporting)
{
	GError *error = 0;

	GVariant *propVar, *realPropVar;
	free_desktop_dbus_properties_call_get_sync(mPropertiesProxy, "org.bluez.Adapter1", "DelayReport", &propVar, NULL, &error);

	if (error)
	{
		g_error_free(error);
		return false;
	}

	realPropVar = g_variant_get_variant(propVar);

	delayReporting = g_variant_get_boolean(realPropVar);

	return true;
}

void Bluez5Adapter::notifyA2dpRoleChnange (const std::string &uuid )
{
	std::replace_if(mUuids.begin(),mUuids.end(),[](std::string pUuid)
	{ return ((pUuid == BLUETOOTH_PROFILE_A2DP_SOURCE_UUID)|| (pUuid == BLUETOOTH_PROFILE_A2DP_SINK_UUID));},uuid);
	BluetoothPropertiesList properties;
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::UUIDS, mUuids));
	observer->adapterPropertiesChanged(properties);
}

void Bluez5Adapter::notifyAvrcpRoleChange(const std::string &uuid )
{
        std::replace_if(mUuids.begin(),mUuids.end(),[](std::string pUuid)
        { return ((pUuid == BLUETOOTH_PROFILE_AVRCP_TARGET_UUID) || (pUuid == BLUETOOTH_PROFILE_AVRCP_REMOTE_UUID));},uuid);
        BluetoothPropertiesList properties;
        properties.push_back(BluetoothProperty(BluetoothProperty::Type::UUIDS, mUuids));
        observer->adapterPropertiesChanged(properties);
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

BluetoothError Bluez5Adapter::forceRepower()
{
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

	sleep(1);

	valueVar = g_variant_new_boolean(TRUE);
	free_desktop_dbus_properties_call_set_sync(mPropertiesProxy, "org.bluez.Adapter1",
						"Powered", g_variant_new_variant(valueVar),
						NULL, &error);
	if (error)
	{
		g_error_free(error);
		return BLUETOOTH_ERROR_FAIL;
	}

	mAdvertise->assignAdvertiseManager(mObjectPath);

	return BLUETOOTH_ERROR_NONE;
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
	mLegacyScan = true;
	if (mUseBluezFilter)
	{
		mUseBluezFilter = false;
		BluetoothError err = prepareFilterForDiscovery();
		if (err == BLUETOOTH_ERROR_FAIL)
			return BLUETOOTH_ERROR_FAIL;
	}
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

	mLegacyScan = false;
	if (mLeScanFilters.size() == 0)
	{
		mSILDiscovery = false;
		resetDiscoveryTimeout();

		GError *error = 0;
		bluez_adapter1_call_stop_discovery_sync(mAdapterProxy, NULL, &error);
		if (error)
		{
			g_error_free(error);
			callback(BLUETOOTH_ERROR_FAIL);
			return;
		}
		if (mUseBluezFilter)
		{
			if (!clearPreviousFilter())
			{
				callback(BLUETOOTH_ERROR_FAIL);
				return;
			}
			mUseBluezFilter = false;
		}
		mCancelDiscCallback = callback;
	}
	else
		callback(BLUETOOTH_ERROR_NONE);
}

bool Bluez5Adapter::isServiceUuidValid(const BluetoothLeDiscoveryFilter &filter)
{
	const BluetoothUuid uUiditem = BluetoothUuid(filter.getServiceUuid().getUuid());
	if (uUiditem.getType() == BluetoothUuid::UNKNOWN)
		return false;
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
			return false;
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
			return false;
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
			return false;
	}
	return true;
}

bool Bluez5Adapter::isServiceDataValid(const BluetoothLeDiscoveryFilter &filter)
{
	if (filter.getServiceData().getMask().empty() || (filter.getServiceData().getData().size() == filter.getServiceData().getMask().size()))
	{
		const BluetoothUuid uUiditem = BluetoothUuid(filter.getServiceData().getUuid());
		if (uUiditem.getType() == BluetoothUuid::UNKNOWN)
			return false;
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
		return true;
	}
	return false;
}

bool Bluez5Adapter::isFilterValid(const BluetoothLeDiscoveryFilter &filter)
{
	mFilterType = 0x00;

	if (filter.isFilterEmpty())
		mFilterType |= FILTER_NONE;
	else
	{
		if (!filter.getServiceUuid().getUuid().empty())
		{
			if (!isServiceUuidValid(filter))
				return false;
			if (filter.getServiceUuid().getMask().empty())
				mFilterType |= FILTER_SERVICEUUID;
			else
				mFilterType |= FILTER_SERVICEUUIDMASK;
		}
		else if (!filter.getServiceUuid().getMask().empty())
			return false;

		if (!filter.getServiceData().getUuid().empty() && filter.getServiceData().getData().size() > 0)
		{
			if (!isServiceDataValid(filter))
				return false;
			mFilterType |= FILTER_SERVICEDATA;
		}
		else if ((!filter.getServiceData().getUuid().empty() && filter.getServiceData().getData().size() < 1) ||
				(filter.getServiceData().getUuid().empty() && filter.getServiceData().getData().size() > 0) ||
				(!filter.getServiceData().getMask().empty()))
				return false;

		if (filter.getManufacturerData().getId() > 0 && filter.getManufacturerData().getData().size() > 0)
		{
			if (!filter.getManufacturerData().getMask().empty() && (filter.getManufacturerData().getData().size() != filter.getManufacturerData().getMask().size()))
			{
				return false;
			}
			mFilterType |= FILTER_MANUFACTURERDATA;
		}
		else if ((filter.getManufacturerData().getId() > 0 && filter.getManufacturerData().getData().size() < 1) ||
				(filter.getManufacturerData().getId() < 1 && filter.getManufacturerData().getData().size() > 0) ||
				(!filter.getManufacturerData().getMask().empty()))
				return false;

		if (!filter.getName().empty())
			mFilterType |= FILTER_NAME;
		if (!filter.getAddress().empty())
			mFilterType |= FILTER_ADDRESS;
	}
	return true;
}

int32_t Bluez5Adapter::addLeDiscoveryFilter(const BluetoothLeDiscoveryFilter &filter)
{
	int32_t scanId = -1;
	BluetoothError err = BLUETOOTH_ERROR_NONE;

	if (!isFilterValid(filter))
		return scanId;

	if (mLegacyScan)
		goto SUCCESS;

	mUseBluezFilter = bluezFilterUsageCriteria(mFilterType);

	if (mSILDiscovery)
		err = prepareFilterForDiscovery();

	if (err == BLUETOOTH_ERROR_NONE)
	{
		if (!mUseBluezFilter)
			goto SUCCESS;

		if (!setBluezFilter(filter))
			return scanId;
	}
	else
		return scanId;

	if (mSILDiscovery)
	{
		if (!resumeLeDiscovery())
			return -1;
	}
	mSILDiscovery = true;

SUCCESS:
	scanId = nextScanId();
	mLeScanFilters.insert(std::pair<uint32_t, BluetoothLeDiscoveryFilter>(scanId, filter));
	mLeScanFilterTypes.insert(std::pair<uint32_t, unsigned char>(scanId, mFilterType));
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
		removeFilterType(scanId);
		mLeScanFilters.erase(scanIter);
		return BLUETOOTH_ERROR_NONE;
	}
}

void Bluez5Adapter::removeFilterType(uint32_t scanId)
{
	auto tmpIter = mLeScanFilterTypes.find(scanId);
	mLeScanFilterTypes.erase(tmpIter);
}

BluetoothError Bluez5Adapter::prepareFilterForDiscovery()
{
	if (!stopLeDiscovery() || !clearPreviousFilter())
		return BLUETOOTH_ERROR_FAIL;

	if (!mUseBluezFilter)
	{
		if (!resumeLeDiscovery())
			return BLUETOOTH_ERROR_FAIL;
	}
	return BLUETOOTH_ERROR_NONE;
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

	if (mLeScanFilters.size() == 0  && mLegacyScan == false)
	{
		mSILDiscovery = false;
		resetDiscoveryTimeout();
		GError *error = 0;
		bluez_adapter1_call_stop_discovery_sync(mAdapterProxy, NULL, &error);
		if (error)
		{
			g_error_free(error);
			return BLUETOOTH_ERROR_FAIL;
		}
		if (mUseBluezFilter)
		{
			if (!clearPreviousFilter())
				return BLUETOOTH_ERROR_FAIL;
			mUseBluezFilter = false;
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
	else if (profileId == BLUETOOTH_PROFILE_ID_PBAP)
	{
		profile = new Bluez5ProfilePbap(this);
		mProfiles.insert(std::pair<std::string,BluetoothProfile*>(BLUETOOTH_PROFILE_ID_PBAP, profile));
	}
	else if (profileId == BLUETOOTH_PROFILE_ID_HFP)
	{
		profile = new Bluez5ProfileHfp(this);
		mProfiles.insert(std::pair<std::string,BluetoothProfile*>(BLUETOOTH_PROFILE_ID_HFP, profile));
	}
	else if (profileId == BLUETOOTH_PROFILE_ID_MAP)
	{
		profile = new Bluez5ProfileMap(this);
		mProfiles.insert(std::pair<std::string,BluetoothProfile*>(BLUETOOTH_PROFILE_ID_MAP, profile));
	}
	else if (profileId == BLUETOOTH_PROFILE_ID_MESH)
	{
		profile = new Bluez5ProfileMesh(this);
		mProfiles.insert(std::pair<std::string,BluetoothProfile*>(BLUETOOTH_PROFILE_ID_MESH, profile));
	}

	if (profile)
	{
		std::string uuid = profile->getProfileUuid();
		// Profile is initialized with remote uuid which its connecting
		if (uuid == BLUETOOTH_PROFILE_AVRCP_REMOTE_UUID)
		{
			mUuids.push_back(BLUETOOTH_PROFILE_AVRCP_TARGET_UUID);
		}
		else if (uuid == BLUETOOTH_PROFILE_REMOTE_HFP_AG_UUID)
		{
			mUuids.push_back(BLUETOOTH_PROFILE_REMOTE_HFP_HF_UUID);
		}
		else
			mUuids.push_back(std::move(uuid));
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

bool Bluez5Adapter::stopLeDiscovery()
{
	resetDiscoveryTimeout();

	GError *error = 0;
	bluez_adapter1_call_stop_discovery_sync(mAdapterProxy, NULL, &error);
	if (error)
	{
		g_error_free(error);
		return false;
	}
	return true;
}

bool Bluez5Adapter::resumeLeDiscovery()
{
	GError *error = 0;
	bluez_adapter1_call_start_discovery_sync(mAdapterProxy, NULL, &error);
	if (error)
	{
		g_error_free(error);
		return false;
	}

	startDiscoveryTimeout();
	return true;
}

bool Bluez5Adapter::setBluezFilter(const BluetoothLeDiscoveryFilter &filter)
{
	GError *error = 0;
	GVariantBuilder *builder = 0;
	GVariant *arguments = 0;

	builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
	GVariantBuilder *uuidArrayVarBuilder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
	std::string srvcUuidStr = filter.getServiceUuid().getUuid();
	srvcUuidStr = convertToLowerCase(srvcUuidStr);
	g_variant_builder_add(uuidArrayVarBuilder, "s", &srvcUuidStr[0]);
	for (auto it = mLeScanFilters.begin(); it != mLeScanFilters.end(); ++it)
	{
		srvcUuidStr = it->second.getServiceUuid().getUuid();
		srvcUuidStr = convertToLowerCase(srvcUuidStr);
		g_variant_builder_add(uuidArrayVarBuilder, "s", &srvcUuidStr[0]);
	}
	GVariant *uuids = g_variant_builder_end(uuidArrayVarBuilder);
	g_variant_builder_unref (uuidArrayVarBuilder);

	g_variant_builder_add (builder, "{sv}", "UUIDs", uuids);
	g_variant_builder_add (builder, "{sv}", "Transport", g_variant_new_string ("le"));
	arguments = g_variant_builder_end (builder);
	g_variant_builder_unref(builder);

	bluez_adapter1_call_set_discovery_filter_sync(mAdapterProxy, arguments, NULL, &error);
	if (error)
	{
		g_error_free(error);
		return false;
	}
	return true;
}

bool Bluez5Adapter::clearPreviousFilter()
{
	GVariantBuilder *builder = 0;
	GVariant *arguments = 0;
	builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
	GVariantBuilder *uuidArrayVarBuilder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
	GVariant *uuids = g_variant_builder_end(uuidArrayVarBuilder);
	g_variant_builder_unref (uuidArrayVarBuilder);
	g_variant_builder_add (builder, "{sv}", "UUIDs", uuids);
	arguments = g_variant_builder_end (builder);
	g_variant_builder_unref(builder);

	GError *error = 0;
	bluez_adapter1_call_set_discovery_filter_sync(mAdapterProxy, arguments, NULL, &error);
	if (error)
	{
		g_error_free(error);
		return false;
	}

	return true;
}

bool Bluez5Adapter::bluezFilterUsageCriteria(unsigned char mFilterType)
{
	unsigned char tmpFilterType = 0x00;
	for (auto it = mLeScanFilterTypes.begin(); it != mLeScanFilterTypes.end(); ++it)
		tmpFilterType |= it->second;

	tmpFilterType |= mFilterType;

	if (tmpFilterType == FILTER_SERVICEUUID)
		return true;
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

	if (mAgent->supplyPairingConfirmation(this, address, accept))
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

void Bluez5Adapter::updateProfileConnectionStatus(const std::string PROFILE_ID, std::string address, bool isConnected,
	const std::string &uuid)
{
	auto profile = dynamic_cast<Bluez5ProfileBase*> (getProfile(PROFILE_ID));
	if (profile)
		profile->updateConnectionStatus(address, isConnected, uuid);
}

void Bluez5Adapter::updateAvrcpVolume(std::string address, guint16 volume)
{
	Bluez5ProfileAvcrp *avrcp = dynamic_cast<Bluez5ProfileAvcrp*> (getProfile(BLUETOOTH_PROFILE_ID_AVRCP));
		if (avrcp) avrcp->updateVolume(address, volume);
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
