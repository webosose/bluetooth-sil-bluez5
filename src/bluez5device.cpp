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

#include "logging.h"
#include "bluez5device.h"
#include "bluez5adapter.h"
#include "bluez5agent.h"
#include "asyncutils.h"

#define SWAP_INT32(x) (((x) >> 24) | (((x) & 0x00FF0000) >> 8) | (((x) & 0x0000FF00) << 8) | ((x) << 24))

const std::string BLUETOOTH_PROFILE_AVRCP_REMOTE_UUID = "0000110e-0000-1000-8000-00805f9b34fb";
const std::string BLUETOOTH_PROFILE_AVRCP_TARGET_UUID = "0000110c-0000-1000-8000-00805f9b34fb";
const std::string BLUETOOTH_PROFILE_A2DP_SINK_UUID = "0000110b-0000-1000-8000-00805f9b34fb";

static const std::map<std::string,BluetoothDeviceRole> uuidtoRoleMap ={
	{"0000111e-0000-1000-8000-00805f9b34fb", BLUETOOTH_DEVICE_ROLE_HFP_HF},
	{"0000111f-0000-1000-8000-00805f9b34fb", BLUETOOTH_DEVICE_ROLE_HFP_AG},
	{"0000110a-0000-1000-8000-00805f9b34fb" ,BLUETOOTH_DEVICE_ROLE_A2DP_SRC},
	{"0000110b-0000-1000-8000-00805f9b34fb", BLUETOOTH_DEVICE_ROLE_A2DP_SINK},
	{"0000110e-0000-1000-8000-00805f9b34fb", BLUETOOTH_DEVICE_ROLE_AVRCP_RMT},
	{"0000110c-0000-1000-8000-00805f9b34fb", BLUETOOTH_DEVICE_ROLE_AVRCP_TGT},
	{}
};

static const std::map<std::string,std::string> profileIdUuidMap ={
	{"0000111e-0000-1000-8000-00805f9b34fb", BLUETOOTH_PROFILE_ID_HFP},
	{"0000111f-0000-1000-8000-00805f9b34fb", BLUETOOTH_PROFILE_ID_HFP},
	{"0000110a-0000-1000-8000-00805f9b34fb", BLUETOOTH_PROFILE_ID_A2DP},
	{"0000110b-0000-1000-8000-00805f9b34fb", BLUETOOTH_PROFILE_ID_A2DP},
	{"0000110e-0000-1000-8000-00805f9b34fb", BLUETOOTH_PROFILE_ID_AVRCP},
	{"0000110c-0000-1000-8000-00805f9b34fb", BLUETOOTH_PROFILE_ID_AVRCP},
	{}
};

static const std::array<std::string, 4> supportedMessageTypes = {"EMAIL", "SMS_GSM", "SMS_CDMA", "MMS"};

Bluez5Device::Bluez5Device(Bluez5Adapter *adapter, const std::string &objectPath) :
	mAdapter(adapter),
	mObjectPath(objectPath),
	mClassOfDevice(0),
	mType(BLUETOOTH_DEVICE_TYPE_UNKNOWN),
	mPaired(false),
	mConnected(false),
	mTrusted(false),
	mBlocked(false),
	mTxPower(0),
	mRSSI(0),
	mDeviceProxy(0),
	mPropertiesProxy(0),
	mConnectedRole(BLUETOOTH_DEVICE_ROLE)
{
	GError *error = 0;
	GVariant *propsVar = NULL;

	mDeviceProxy = bluez_device1_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
														"org.bluez", objectPath.c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_FAILED_TO_CREATE_ADAPTER_PROXY, 0, "Failed to create dbus proxy for device on path %s: %s",
			  objectPath.c_str(), error->message);
		g_error_free(error);
		return;
	}

	mPropertiesProxy = free_desktop_dbus_properties_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
																		   "org.bluez", objectPath.c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_FAILED_TO_CREATE_ADAPTER_PROXY, 0, "Failed to create dbus proxy for device on path %s: %s",
			  objectPath.c_str(), error->message);
		g_error_free(error);
		return;
	}

	DEBUG("Successfully created proxy for device on path %s", objectPath.c_str());

	g_signal_connect(G_OBJECT(mPropertiesProxy), "properties-changed", G_CALLBACK(handlePropertiesChanged), this);

	free_desktop_dbus_properties_call_get_all_sync(mPropertiesProxy, "org.bluez.Device1", &propsVar, NULL, NULL);

	DEBUG("Before calling g_variant_n_children() propsVar value %p\n", propsVar);
	if(!error && propsVar != NULL)
	{
		for (int n = 0; n < g_variant_n_children(propsVar); n++)
		{
			GVariant *propertyVar = g_variant_get_child_value(propsVar, n);
			GVariant *keyVar = g_variant_get_child_value(propertyVar, 0);
			GVariant *valueVar = g_variant_get_child_value(propertyVar, 1);

			std::string key = g_variant_get_string(keyVar, NULL);

			parsePropertyFromVariant(key, g_variant_get_variant(valueVar));

			g_variant_unref(propertyVar);
			g_variant_unref(keyVar);
			g_variant_unref(valueVar);
		}
	}

	g_signal_connect(G_OBJECT(mDeviceProxy), "media-play-request", G_CALLBACK(handleMediaPlayRequest), this);

	g_signal_connect(G_OBJECT(mDeviceProxy), "media-meta-request", G_CALLBACK(handleMediaMetaRequest), this);
}

Bluez5Device::~Bluez5Device()
{
	if (mDeviceProxy)
		g_object_unref(mDeviceProxy);

	if (mPropertiesProxy)
		g_object_unref(mPropertiesProxy);
}

bool Bluez5Device::isLittleEndian()
{
	unsigned int i = 1;
	char *c = (char*)&i;
	if (*c)
		return true;
	return false;
}

void Bluez5Device::handleMediaPlayRequest(BluezDevice1 *, gpointer userData)
{
	DEBUG("handleMediaPlayRequest");
	auto device = static_cast<Bluez5Device*>(userData);
	if (device)
		device->mAdapter->mediaPlayStatusRequest(device->getAddress());
}

void Bluez5Device::handleMediaMetaRequest(BluezDevice1 *, gpointer userData)
{
	DEBUG("handleMediaMetaRequest");
	auto device = static_cast<Bluez5Device*>(userData);
	if (device)
		device->mAdapter->mediaMetaDataRequest(device->getAddress());
}

void Bluez5Device::handlePropertiesChanged(BluezDevice1 *, gchar *interface,  GVariant *changedProperties,
												   GVariant *invalidatedProperties, gpointer userData)
{
	bool propertiesChanged = false;
	auto device = static_cast<Bluez5Device*>(userData);

	for (int n = 0; n < g_variant_n_children(changedProperties); n++)
	{
		GVariant *propertyVar = g_variant_get_child_value(changedProperties, n);
		GVariant *keyVar = g_variant_get_child_value(propertyVar, 0);
		GVariant *valueVar = g_variant_get_child_value(propertyVar, 1);

		std::string key = g_variant_get_string(keyVar, NULL);

		if (device->parsePropertyFromVariant(key, g_variant_get_variant(valueVar)))
			propertiesChanged = true;

		g_variant_unref(valueVar);
		g_variant_unref(keyVar);
		g_variant_unref(propertyVar);
	}

	if (propertiesChanged)
	{
		DEBUG("Firing devicePropertiesChanged from sil for address %s", device->getAddress().c_str());
		device->mAdapter->handleDevicePropertiesChanged(device);
	}
}

bool Bluez5Device::parsePropertyFromVariant(const std::string &key, GVariant *valueVar)
{
	bool changed = false;

	if (key == "Name")
	{
		if(mAlias.empty())       //prefer Alias over Name
		{
			mName = g_variant_get_string(valueVar, NULL);
			DEBUG("Alias name is empty, got name as %s", mName.c_str());
			changed = true;
		}
	}
	if (key == "Alias")
	{
		mAlias = g_variant_get_string(valueVar, NULL);
		DEBUG("Got alias as %s", mAlias.c_str());
		mName = mAlias;
		changed = true;
	}
	else if (key == "Address")
	{
		mAddress = g_variant_get_string(valueVar, NULL);
		changed = true;
	}
	else if (key == "Class")
	{
		mClassOfDevice = g_variant_get_uint32(valueVar);
		changed = true;
	}
	else if (key == "DeviceType")
	{
		mType = (BluetoothDeviceType) g_variant_get_uint32(valueVar);
		changed = true;
	}
	else if (key == "Paired")
	{
		mPaired = g_variant_get_boolean(valueVar);
		changed = true;
	}
	else if (key == "Connected")
	{
		mConnected = g_variant_get_boolean(valueVar);
		changed = true;
	}
	else if (key == "ConnectedUUIDS")
	{
		auto prevConnectedUuis = mConnectedUuids;

		mConnectedUuids.clear();

		for (int m = 0; m < g_variant_n_children(valueVar); m++)
		{
			GVariant *uuidVar = g_variant_get_child_value(valueVar, m);

			std::string uuid = g_variant_get_string(uuidVar, NULL);
			mConnectedUuids.push_back(uuid);
			g_variant_unref(uuidVar);
		}
		updateConnectedRole();
		updateProfileConnectionStatus(prevConnectedUuis);
		changed = true;
	}
	else if (key == "UUIDs")
	{
		mUuids.clear();

		for (int m = 0; m < g_variant_n_children(valueVar); m++)
		{
			GVariant *uuidVar = g_variant_get_child_value(valueVar, m);

			std::string uuid = g_variant_get_string(uuidVar, NULL);
			mUuids.push_back(uuid);

			g_variant_unref(uuidVar);
		}

		changed = true;
	}
	else if (key == "MapInstances")
	{
		mMapInstancesName.clear();

		for (int m = 0; m < g_variant_n_children(valueVar); m++)
		{
			GVariant *mapInstanceVar = g_variant_get_child_value(valueVar, m);

			std::string mapInstance = g_variant_get_string(mapInstanceVar, NULL);
			mMapInstancesName.push_back(mapInstance);

			g_variant_unref(mapInstanceVar);
		}

		changed = true;
	}
	else if (key == "MapInstanceProperties")
	{
		mMapSupportedMessageTypes.clear();

		for (int m = 0; m < g_variant_n_children(valueVar) && m < mMapInstancesName.size(); m++)
		{
			GVariant *mapInstanceVar = g_variant_get_child_value(valueVar, m);
			std::uint32_t mapInstance = (std::uint32_t)g_variant_get_int32(mapInstanceVar);
			if (!isLittleEndian())
			{
				mapInstance  = SWAP_INT32(mapInstance);
			}
			std::uint8_t data = (std::uint8_t)mapInstance;
			std::vector<std::string> supportedMessageTypesList;
			convertToSupportedtypes(data & 0x000F ,supportedMessageTypesList);
			mMapSupportedMessageTypes.insert(std::pair<std::string, std::vector<std::string>>(mMapInstancesName.at(m),supportedMessageTypesList));
			g_variant_unref(mapInstanceVar);
		}

		changed = true;
	}
	else if (key == "Trusted")
	{
		mTrusted = g_variant_get_boolean(valueVar);
		DEBUG("Got trusted as %d for address %s", mTrusted, mAddress.c_str());
		changed = true;
	}
	else if (key == "Blocked")
	{
		mBlocked = g_variant_get_boolean(valueVar);
		DEBUG("Got blocked as %d for address %s", mBlocked, mAddress.c_str());
		changed = true;
	}
	else if (key == "ManufacturerData")
	{
		GVariantIter *iter;
		g_variant_get (valueVar, "a{qv}", &iter);

		GVariant *array;
		uint16_t key;
		uint8_t val;

		while (g_variant_iter_loop(iter, "{qv}", &key, &array))
		{
			if (isLittleEndian())
			{
				mManufacturerData.push_back((key & 0xFF00) >> 8);
				mManufacturerData.push_back(key & 0x00FF);
			}
			else
			{
				mManufacturerData.push_back(key & 0x00FF);
				mManufacturerData.push_back((key & 0xFF00) >> 8);
			}
			GVariantIter it_array;
			g_variant_iter_init(&it_array, array);
			while(g_variant_iter_loop(&it_array, "y", &val))
			{
				mManufacturerData.push_back(val);
			}
			break;
		}
		g_variant_iter_free(iter);
		changed = true;
	}
	else if (key == "ServiceData")
	{
		GVariantIter *iter;
		g_variant_get (valueVar, "a{sv}", &iter);

		GVariant *array;
		const gchar *key;
		uint8_t val;

		while (g_variant_iter_loop(iter, "{&sv}", &key, &array))
		{
			mServiceData.mServiceDataUuid = key;
			GVariantIter it_array;
			g_variant_iter_init(&it_array, array);
			while(g_variant_iter_loop(&it_array, "y", &val))
			{
				mServiceData.mScanRecord.push_back(val);
			}
			break;
		}
		g_variant_iter_free(iter);
		changed = true;
	}
	else if (key == "TxPower")
	{
		mTxPower = g_variant_get_int16(valueVar);
		changed = true;
	}
	else if (key == "RSSI")
	{
		mRSSI = g_variant_get_int16(valueVar);
		changed = true;
	}
	else if (key == "KeyCode")
	{
		GVariantIter *iter;
		g_variant_get (valueVar, "a{sv}", &iter);
		char state[32] = {0x00};
		std::string key_code;

		GVariant *array;
		const gchar *key;
		char val;

		while (g_variant_iter_loop(iter, "{&sv}", &key, &array))
		{
			key_code = key;
			GVariantIter it_array;
			g_variant_iter_init(&it_array, array);
			int i = 0;
			while(g_variant_iter_loop(&it_array, "y", &val))
			{
				state[i++] = val;
			}
			break;
		}
		g_variant_iter_free(iter);
		DEBUG("key[%s] and state[%s]", key_code.c_str(), state);
		mAdapter->recievePassThroughCommand(getAddress(), key, state);
	}
	else if(key == "AvrcpCTFeatures")
	{
		mAdapter->updateRemoteFeatures(getRemoteControllerFeatures(), "CT", mAddress);
	}
	else if (key == "AvrcpTGFeatures")
	{
		mAdapter->updateRemoteFeatures(getRemoteTargetFeatures(), "TG", mAddress);
	}
	else if (key == "AvrcpCTSupportedEvents")
	{
		uint16_t events = g_variant_get_uint16(valueVar);
		mAdapter->updateSupportedNotificationEvents(events, mAddress);
	}

	return changed;
}

GVariant* Bluez5Device::devPropertyValueToVariant(const BluetoothProperty& property)
{
	GVariant *valueVar = 0;

	switch (property.getType())
	{
	case BluetoothProperty::Type::TRUSTED:
	case BluetoothProperty::Type::BLOCKED:
		valueVar = g_variant_new_boolean(property.getValue<bool>());
		break;
	default:
		break;
	}

	return valueVar;
}

std::string Bluez5Device::devPropertyTypeToString(BluetoothProperty::Type type)
{
	std::string propertyName;

	switch (type)
	{
	case BluetoothProperty::Type::TRUSTED:
		propertyName = "Trusted";
		break;
	case BluetoothProperty::Type::BLOCKED:
		propertyName = "Blocked";
		break;
	default:
		break;
	}

	return propertyName;
}

void Bluez5Device::convertToSupportedtypes(std::uint8_t data,std::vector<std::string>& supportedMessageTypesList)
{
	for (unsigned int j = 0; j < 4; j++)
	{
		if(data & 1<<j )
		{
			supportedMessageTypesList.push_back(supportedMessageTypes.at(j));
		}
	}
}

void Bluez5Device::setDevicePropertyAsync(const BluetoothProperty& property, BluetoothResultCallback callback)
{
	std::string propertyName = devPropertyTypeToString(property.getType());

	DEBUG ("%s: property name is %s", __func__, propertyName.c_str());

	GVariant *valueVar = devPropertyValueToVariant(property);
	if (!valueVar)
	{
		callback(BLUETOOTH_ERROR_FAIL);
		return;
	}

	auto setStateCallback = [this, callback](GAsyncResult *result) {
		GError *error = 0;
		gboolean ret;

		ret = free_desktop_dbus_properties_call_set_finish(mPropertiesProxy, result, &error);
		if (error)
		{
			g_error_free(error);
			callback(BLUETOOTH_ERROR_FAIL);
			return;
		}

		callback(BLUETOOTH_ERROR_NONE);
	};

	free_desktop_dbus_properties_call_set(mPropertiesProxy, "org.bluez.Device1", propertyName.c_str(),
                                               g_variant_new_variant(valueVar), NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(setStateCallback));
}

bool Bluez5Device::setDevicePropertySync(const BluetoothProperty& property)
{
	std::string propertyName = devPropertyTypeToString(property.getType());

	DEBUG ("%s: property name is %s", __func__, propertyName.c_str());

	GVariant *valueVar = devPropertyValueToVariant(property);
	if (!valueVar)
		return false;

	GError *error = 0;
	free_desktop_dbus_properties_call_set_sync(mPropertiesProxy, "org.bluez.Device1", propertyName.c_str(),
                                               g_variant_new_variant(valueVar), NULL, &error);
	if (error)
	{
		DEBUG ("%s: error is %s", __func__, error->message);
		g_error_free(error);
		return false;
	}

	return true;
}

void Bluez5Device::pair(BluetoothResultCallback callback)
{
	DEBUG("Pairing with device %s", mAddress.c_str());

	if (!mAdapter->getAgent())
	{
		ERROR(MSGID_AGENT_NOT_READY, 0,
		      "Not able to pair with device %s because no agent was yet assigned to our adapter",
		      mAddress.c_str());

		callback(BLUETOOTH_ERROR_NOT_READY);
		return;
	}

	auto pairCallback = [this, callback](GAsyncResult *result) {
		GError *error = 0;
		gboolean ret;

		ret = bluez_device1_call_pair_finish(mDeviceProxy, result, &error);

		mAdapter->getAgent()->stopPairingForDevice(this);

		if (error)
		{
			DEBUG("Pairing error: %s", error->message);

			auto cancelPairingCallback = [this](GAsyncResult *result) {
				GError *error = 0;
				gboolean ret;

				ret = bluez_device1_call_cancel_pairing_finish(mDeviceProxy, result, &error);
				if (error)
				{
					ERROR(MSGID_PAIRING_CANCEL_ERROR, 0,
					      "Not able to cancel pairing after timeout, error: %s",
						  error->message);
					g_error_free(error);
				}
			};

			if (g_strcmp0(error->message, "GDBus.Error:org.bluez.Error.AlreadyExists: Already Exists") == 0)
				callback(BLUETOOTH_ERROR_DEVICE_ALREADY_PAIRED);
			else if (g_strcmp0(error->message, "GDBus.Error:org.bluez.Error.AuthenticationFailed: Authentication Failed") == 0)
				callback(BLUETOOTH_ERROR_AUTHENTICATION_FAILED);
			else if (g_strcmp0(error->message, "GDBus.Error:org.bluez.Error.AuthenticationCanceled: Authentication Canceled") == 0)
				callback(BLUETOOTH_ERROR_AUTHENTICATION_CANCELED);
			else if (g_strcmp0(error->message, "Timeout was reached") == 0)
			{
				//Some devices (like USB dongle) need a cancel to be sent after timeout, so that the pairing is complete with a cancel
				bluez_device1_call_cancel_pairing(mDeviceProxy, NULL,
                                                  glibAsyncMethodWrapper,
                                                  new GlibAsyncFunctionWrapper(cancelPairingCallback));
				callback(BLUETOOTH_ERROR_AUTHENTICATION_TIMEOUT);
			}
			else
				callback(BLUETOOTH_ERROR_FAIL);

			g_error_free(error);

			return;
		}

		setPaired(true);
		mAdapter->handleDevicePropertiesChanged(this);
		if(callback)
			callback(BLUETOOTH_ERROR_NONE);
	};

	mAdapter->getAgent()->startPairingForDevice(this);

	bluez_device1_call_pair(mDeviceProxy, NULL,
							glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(pairCallback));
}

void Bluez5Device::cancelPairing(BluetoothResultCallback callback)
{
	DEBUG("Cancel current ongoing pairing process");

	auto cancelPairingCallback = [this, callback](GAsyncResult *result) {
		GError *error = 0;
		gboolean ret;

		ret = bluez_device1_call_cancel_pairing_finish(mDeviceProxy, result, &error);
		if (error)
		{
			g_error_free(error);
			if (callback)
				callback(BLUETOOTH_ERROR_FAIL);
			return;
		}

		mAdapter->setPairing(false);

		if (callback)
			callback(BLUETOOTH_ERROR_NONE);
	};

	bluez_device1_call_cancel_pairing(mDeviceProxy, NULL,
							glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(cancelPairingCallback));
}

void Bluez5Device::connect(const std::string &uuid, BluetoothResultCallback callback)
{
	auto connectCallback = [this, callback](GAsyncResult *result) {
		GError *error = 0;
		gboolean ret;

		ret = bluez_device1_call_connect_profile_finish(mDeviceProxy, result, &error);
		if (error)
		{
			g_error_free(error);
			callback(BLUETOOTH_ERROR_FAIL);
			return;
		}

		callback(BLUETOOTH_ERROR_NONE);
	};

	bluez_device1_call_connect_profile(mDeviceProxy, uuid.c_str(), NULL,
	                                   glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(connectCallback));
}

void Bluez5Device::disconnect(const std::string &uuid, BluetoothResultCallback callback)
{
	auto disconnectCallback = [this, callback](GAsyncResult *result) {
		GError *error = 0;
		gboolean ret;

		ret = bluez_device1_call_disconnect_profile_finish(mDeviceProxy, result, &error);
		if (error)
		{
			g_error_free(error);
			callback(BLUETOOTH_ERROR_FAIL);
			return;
		}

		callback(BLUETOOTH_ERROR_NONE);
	};

	bluez_device1_call_disconnect_profile(mDeviceProxy, uuid.c_str(), NULL,
	                                      glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(disconnectCallback));
}

void Bluez5Device::connect(BluetoothResultCallback callback)
{
	auto connectCallback = [this, callback](GAsyncResult *result) {
		GError *error = 0;
		gboolean ret;

		ret = bluez_device1_call_connect_finish(mDeviceProxy, result, &error);
		if (error)
		{
			g_error_free(error);
			callback(BLUETOOTH_ERROR_FAIL);
			return;
		}

		callback(BLUETOOTH_ERROR_NONE);
	};

	bluez_device1_call_connect(mDeviceProxy, NULL,
	                                   glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(connectCallback));
}

void Bluez5Device::disconnect(BluetoothResultCallback callback)
{
	auto disconnectCallback = [this, callback](GAsyncResult *result) {
		GError *error = 0;
		gboolean ret;

		ret = bluez_device1_call_disconnect_finish(mDeviceProxy, result, &error);
		if (error)
		{
			g_error_free(error);
			callback(BLUETOOTH_ERROR_FAIL);
			return;
		}

		callback(BLUETOOTH_ERROR_NONE);
	};

	bluez_device1_call_disconnect(mDeviceProxy, NULL,
	                                      glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(disconnectCallback));
}

void Bluez5Device::connectGatt(BluetoothResultCallback callback)
{
	auto connectCallback = [this, callback](GAsyncResult *result) {
		GError *error = 0;
		gboolean ret;

		ret = bluez_device1_call_connect_finish(mDeviceProxy, result, &error);
		if (error)
		{
			g_error_free(error);
			callback(BLUETOOTH_ERROR_FAIL);
			return;
		}

		callback(BLUETOOTH_ERROR_NONE);
	};

	bluez_device1_call_connect_gatt(mDeviceProxy, NULL,
	                                   glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(connectCallback));
}

BluetoothPropertiesList Bluez5Device::buildPropertiesList() const
{
	BluetoothPropertiesList properties;

	properties.push_back(BluetoothProperty(BluetoothProperty::Type::NAME, mName));
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::BDADDR, mAddress));
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::CLASS_OF_DEVICE, mClassOfDevice));
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::TYPE_OF_DEVICE, (uint32_t)mType));
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::UUIDS, mUuids));
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::MAP_INSTANCES_NAME, mMapInstancesName));
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::MAP_SUPPORTED_MESSAGE_TYPE, mMapSupportedMessageTypes));
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::PAIRED, mPaired));
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::CONNECTED, mConnected));
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::TRUSTED, mTrusted));
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::BLOCKED, mBlocked));
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::MANUFACTURER_DATA, mManufacturerData));
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::SCAN_RECORD, mServiceData.mScanRecord));
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::TXPOWER, mTxPower));
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::RSSI, mRSSI));
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::ROLE, mConnectedRole));
	return properties;
}

std::string Bluez5Device::getObjectPath() const
{
	return mObjectPath;
}

std::string Bluez5Device::getAddress() const
{
	return mAddress;
}

std::string Bluez5Device::getName() const
{
	return mName;
}

uint32_t Bluez5Device::getClassOfDevice() const
{
	return mClassOfDevice;
}

BluetoothDeviceType Bluez5Device::getType() const
{
	return mType;
}

std::vector<std::string> Bluez5Device::getUuids() const
{
	return mUuids;
}

std::vector<std::string> Bluez5Device::getMapInstancesName() const
{
	return mMapInstancesName;
}

std::map<std::string, std::vector<std::string>> Bluez5Device::getSupportedMessageTypes() const
{
	return mMapSupportedMessageTypes;
}

bool Bluez5Device::getConnected() const
{
	return mConnected;
}

Bluez5Adapter* Bluez5Device::getAdapter() const
{
	return mAdapter;
}

std::vector<uint8_t> Bluez5Device::getScanRecord() const
{
	return mServiceData.mScanRecord;
}

std::string Bluez5Device::getServiceDataUuid() const
{
	return mServiceData.mServiceDataUuid;
}

std::vector<uint8_t> Bluez5Device::getManufactureData() const
{
	return mManufacturerData;
}

void Bluez5Device::updateConnectedRole()
{
	mConnectedRole = BLUETOOTH_DEVICE_ROLE;
	for(auto connectedUuidEntry : mConnectedUuids)
	{
		auto it = uuidtoRoleMap.find(connectedUuidEntry);
		if(it != uuidtoRoleMap.end())
		{
			mConnectedRole |= it->second;
		}
	}
}

void Bluez5Device::updateProfileConnectionStatus(std::vector <std::string> prevConnectedUuids)
{
	if (prevConnectedUuids.size() < mConnectedUuids.size())
	{
		DEBUG("connectedUUID added");
		for (auto it = mConnectedUuids.begin(); it != mConnectedUuids.end(); it++)
		{
			auto uuidIt = std::find(prevConnectedUuids.begin(), prevConnectedUuids.end(), *it);
			if(uuidIt == prevConnectedUuids.end())
			{
				auto mapIt = profileIdUuidMap.find(*it);
				if (mapIt != profileIdUuidMap.end())
					mAdapter->updateProfileConnectionStatus(mapIt->second, mAddress, true, *it);
			}
		}
	}
	else if (prevConnectedUuids.size() > mConnectedUuids.size())
	{
		DEBUG("connectedUUID removed");
		for (auto it = prevConnectedUuids.begin(); it != prevConnectedUuids.end(); it++)
		{
			auto uuidIt = std::find(mConnectedUuids.begin(), mConnectedUuids.end(), *it);
			if (uuidIt == mConnectedUuids.end())
			{
				auto mapIt = profileIdUuidMap.find(*it);
				if (mapIt != profileIdUuidMap.end())
					mAdapter->updateProfileConnectionStatus(mapIt->second, mAddress, false, *it);
			}
		}
	}
}
