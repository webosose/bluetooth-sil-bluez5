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

#include "logging.h"
#include "utils.h"
#include "bluez5profilegatt.h"
#include "bluetooth-sil-api.h"
#include "bluez5gattremoteattribute.h"

const std::map <std::string, BluetoothGattCharacteristic::Property> GattRemoteCharacteristic::characteristicPropertyMap = {
	{ "read", BluetoothGattCharacteristic::PROPERTY_READ},
	{ "broadcast", BluetoothGattCharacteristic::PROPERTY_BROADCAST},
	{ "write-without-response", BluetoothGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE},
	{ "write", BluetoothGattCharacteristic::PROPERTY_WRITE},
	{ "notify", BluetoothGattCharacteristic::PROPERTY_NOTIFY},
	{ "indicate", BluetoothGattCharacteristic::PROPERTY_INDICATE},
	{ "authenticated-signed-writes", BluetoothGattCharacteristic::PROPERTY_AUTHENTICATED_SIGNED_WRITES}
};

const std::map <BluetoothGattPermission, std::string> GattRemoteDescriptor::descriptorPermissionMap = {
	{ PERMISSION_READ, "read"},
	{ PERMISSION_READ_ENCRYPTED, "encrypt-read"},
	{ PERMISSION_READ_ENCRYPTED_MITM, "encrypt-authenticated-read"},
	{ PERMISSION_WRITE, "write"},
	{ PERMISSION_WRITE_ENCRYPTED, "encrypt-write"},
	{ PERMISSION_WRITE_ENCRYPTED_MITM, "encrypt-authenticated-write"},
	{ PERMISSION_WRITE_SIGNED, "secure-write"}
};

bool GattRemoteCharacteristic::startNotify()
{
	GError *error = NULL;
	bool result = bluez_gatt_characteristic1_call_start_notify_sync(mInterface, NULL, &error);
	if (error)
	{
		ERROR(MSGID_GATT_PROFILE_ERROR, 0, "startNotify failed due to %s for path %s", error->message, objectPath.c_str());
		g_error_free(error);
		return false;
	}
	return result;
}

bool GattRemoteCharacteristic::stopNotify()
{
	GError *error = NULL;
	bool result = bluez_gatt_characteristic1_call_stop_notify_sync(mInterface, NULL, &error);
	if (error)
	{
		ERROR(MSGID_GATT_PROFILE_ERROR, 0, "startNotify failed due to %s for path %s", error->message, objectPath.c_str());
		g_error_free(error);
		return false;
	}
	return result;
}

void GattRemoteCharacteristic::onCharacteristicPropertiesChanged(GDBusProxy *proxy, GVariant *changed_properties, GStrv invalidated_properties, gpointer userdata)
{
	auto characteristic = static_cast<GattRemoteCharacteristic*>(userdata);
	if (characteristic && characteristic->mGattProfile)
		characteristic->mGattProfile->onCharacteristicPropertiesChanged(characteristic, changed_properties);
}

std::vector<unsigned char> GattRemoteCharacteristic::readValue(uint16_t offset)
{
	GError *error = NULL;
	std::vector<unsigned char> result;

	GVariantDict dict;
	g_variant_dict_init(&dict, NULL);

	if (offset)
		g_variant_dict_insert_value(&dict, "offset", g_variant_new_uint16(offset));

	GVariant *variant = g_variant_dict_end(&dict);

	GVariant *value;

	bluez_gatt_characteristic1_call_read_value_sync(mInterface, variant, &value, NULL, &error);

	if (error)
	{
		ERROR(MSGID_GATT_PROFILE_ERROR, 0, "readValue failed due to %s", error->message);
		g_error_free(error);
		return result;
	}

	result = convertArrayByteGVariantToVector(value);

	return result;
}

bool GattRemoteCharacteristic::writeValue(const std::vector<unsigned char> &characteristicValue, uint16_t offset)
{
	GError *error = NULL;
	bool result = true;

	GVariant *variantValue = convertVectorToArrayByteGVariant(characteristicValue);
	GVariantDict dict;
	g_variant_dict_init(&dict, NULL);

	if (offset)
		g_variant_dict_insert_value(&dict, "offset", g_variant_new_uint16(offset));

	GVariant *variant = g_variant_dict_end(&dict);

	result = bluez_gatt_characteristic1_call_write_value_sync(mInterface, variantValue, variant, NULL, &error);

	if (error)
	{
		ERROR(MSGID_GATT_PROFILE_ERROR, 0, "WriteValue failed due to %s", error->message);
		g_error_free(error);
		return false;
	}

	return result;
}

BluetoothGattCharacteristicProperties GattRemoteCharacteristic::readProperties()
{
	BluetoothGattCharacteristicProperties properties = 0;
	GVariant* flagVariant = bluez_gatt_characteristic1_get_flags(mInterface);
	if (flagVariant) {
		std::vector <std::string> characteristicFlags = convertArrayStringGVariantToVector(flagVariant);

		for (auto it = characteristicFlags.begin(); it != characteristicFlags.end(); it++)
		{
			auto property = characteristicPropertyMap.find(*it);
			if (property != characteristicPropertyMap.end())
				properties |= property->second;
		}
	}
	return 	properties;
}

std::vector<unsigned char> GattRemoteDescriptor::readValue(uint16_t offset)
{
	GError *error = NULL;
	std::vector<unsigned char> result;

	GVariantDict dict;
	g_variant_dict_init(&dict, NULL);

	if (offset)
		g_variant_dict_insert_value(&dict, "offset", g_variant_new_uint16(offset));

	GVariant *variant = g_variant_dict_end(&dict);

	GVariant *value;

	bluez_gatt_descriptor1_call_read_value_sync(mInterface, variant, &value, NULL, &error);

	if (error)
	{
		ERROR(MSGID_GATT_PROFILE_ERROR, 0, "readValue failed due to %s", error->message);
		g_error_free(error);
		return result;
	}

	result = convertArrayByteGVariantToVector(value);

	return result;
}

bool GattRemoteDescriptor::writeValue(const std::vector<unsigned char> &descriptorValue, uint16_t offset)
{
	GError *error = NULL;
	bool result = true;

	GVariant *variantValue = convertVectorToArrayByteGVariant(descriptorValue);
	GVariantDict dict;
	g_variant_dict_init(&dict, NULL);

	if (offset)
		g_variant_dict_insert_value(&dict, "offset", g_variant_new_uint16(offset));

	GVariant *variant = g_variant_dict_end(&dict);

	result = bluez_gatt_descriptor1_call_write_value_sync(mInterface, variantValue, variant, NULL, &error);

	if (error)
	{
		ERROR(MSGID_GATT_PROFILE_ERROR, 0, "WriteValue failed due to %s", error->message);
		g_error_free(error);
		return false;
	}

	return result;
}
