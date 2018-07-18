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

#include "glib.h"
#include "bluez5advertise.h"
#include "bluez5sil.h"
#include "asyncutils.h"
#include "logging.h"

#define BLUEZ5_ADVERTISE_BUS_NAME           "com.webos.service.bleadvertise"
#define BLUEZ5_ADVERTISE_OBJECT_PATH        "/"

Bluez5Advertise::Bluez5Advertise(BluezLEAdvertisingManager1 *advManager, Bluez5SIL *sil):
	mBusId(0),
	mTxPower(false),
	mInterface(0),
	mAdvManager(advManager),
	mPath(BLUEZ5_ADVERTISE_OBJECT_PATH),
	mSIL(sil)
{
	mBusId = g_bus_own_name(G_BUS_TYPE_SYSTEM, BLUEZ5_ADVERTISE_BUS_NAME,
				G_BUS_NAME_OWNER_FLAGS_NONE,
				handleBusAcquired, NULL, NULL, this, NULL);
}

Bluez5Advertise::~Bluez5Advertise()
{
	if (mInterface)
	{
		g_object_unref(mInterface);
	}

	if (mBusId)
	{
		g_bus_unown_name(mBusId);
	}
}

void Bluez5Advertise::handleBusAcquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	Bluez5Advertise *adv = static_cast<Bluez5Advertise*>(user_data);
	adv->createInterface(connection);
}

void Bluez5Advertise::createInterface(GDBusConnection *connection)
{
	mInterface = bluez_leadvertisement1_skeleton_new();
	GError *error = 0;

	g_signal_connect(mInterface, "handle-release", G_CALLBACK(handleRelease), this);

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(mInterface), connection,
                                          mPath.c_str(), &error))
	{
		DEBUG("Failed to initialize agent on bus: %s",error->message);
		g_error_free(error);
		return;
	}
	g_object_unref(connection);
}

void Bluez5Advertise::registerAdvertisement(BluetoothResultCallback callback)
{
	GError *error = 0;
	GVariantBuilder *builder = 0;
	GVariant *arguments = 0;
	builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
	arguments = g_variant_builder_end (builder);
	g_variant_builder_unref(builder);

	auto registerCallback = [this, callback](GAsyncResult *result)
	{
		GError *error = 0;
		gboolean ret;

		ret = bluez_leadvertising_manager1_call_register_advertisement_finish(mAdvManager, result, &error);
		if (error)
		{
			g_error_free(error);
			callback(BLUETOOTH_ERROR_FAIL);
			return;
		}
		callback(BLUETOOTH_ERROR_NONE);
	};

	bluez_leadvertising_manager1_call_register_advertisement(mAdvManager, mPath.c_str(), arguments, NULL,
                                                             glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(registerCallback));
}

void Bluez5Advertise::unRegisterAdvertisement()
{
	bluez_leadvertising_manager1_call_unregister_advertisement_sync(mAdvManager, mPath.c_str(), NULL, NULL);
}

void Bluez5Advertise::advertiseServiceUuids(std::unordered_map<std::string, std::vector<uint8_t> > serviceList)
{
	const char *uuids[serviceList.size()+1];

	int i = 0;
	for (auto it = serviceList.begin(); it != serviceList.end(); it++)
	{
		uuids[i++] = it->first.c_str();
	}

	uuids[i] = NULL;
	bluez_leadvertisement1_set_service_uuids (mInterface, uuids);
}

void Bluez5Advertise::advertiseServiceData(const char* uuids, std::vector<uint8_t> serviceData)
{
	GVariantBuilder *dataBuilder;
	dataBuilder = g_variant_builder_new (G_VARIANT_TYPE ("ay"));

	for (auto it = serviceData.begin(); it != serviceData.end(); it++)
	{
		//TODO bluetoothctl checks for size 25
		g_variant_builder_add(dataBuilder, "y", *it);
	}

	GVariant *dataValue = g_variant_builder_end(dataBuilder);
	g_variant_builder_unref(dataBuilder);

	GVariantBuilder *builder = 0;
	GVariant *arguments = 0;
	builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add (builder, "{sv}", uuids, dataValue);
	arguments = g_variant_builder_end(builder);
	g_variant_builder_unref(builder);

	bluez_leadvertisement1_set_service_data(mInterface, arguments);
}

void Bluez5Advertise::advertiseManufacturerData(std::vector<uint8_t> data)
{
	uint16_t manufacturerId;

	GVariantBuilder *dataBuilder = 0;
	GVariant *dataValue = 0;
	dataBuilder = g_variant_builder_new (G_VARIANT_TYPE ("ay"));

	unsigned int i = 1;
	bool isLittleEndian = false;
	char *c = (char*)&i;
	if (*c)
		isLittleEndian = true;

	if (data.size() > 2)
	{
		if (isLittleEndian)
		{
			uint16_t lsb = data[0] & 0XFFFF;
			uint16_t msb = data[1] << 8;
			manufacturerId = msb | lsb;
		}
		else
		{
			uint16_t lsb = data[1] & 0XFFFF;
			uint16_t msb = data[0] << 8;
			manufacturerId = msb | lsb;
		}
	}

	for (int i = 2; i < data.size(); i++)
	{
		g_variant_builder_add(dataBuilder, "y", data[i]);
	}

	dataValue = g_variant_builder_end(dataBuilder);
	g_variant_builder_unref(dataBuilder);

	GVariantBuilder *builder = 0;
	GVariant *arguments = 0;
	builder = g_variant_builder_new (G_VARIANT_TYPE ("a{qv}"));
	g_variant_builder_add (builder, "{qv}", manufacturerId, dataValue);
	arguments = g_variant_builder_end(builder);
	g_variant_builder_unref(builder);

	bluez_leadvertisement1_set_manufacturer_data(mInterface, arguments);
}

void Bluez5Advertise::advertiseTxPower(bool value)
{
	if (mTxPower == value)
	{
		DEBUG("advertiseTxPower already in same state ");
		return;
	}

	mTxPower = value;
	bluez_leadvertisement1_set_include_tx_power(mInterface, mTxPower);
}

void Bluez5Advertise::advertiseLocalName(const char *name)
{
	bluez_leadvertisement1_set_local_name(mInterface, name);
}

void Bluez5Advertise::advertiseAppearance(guint16 value)
{
	bluez_leadvertisement1_set_appearance(mInterface, value);
}

void Bluez5Advertise::advertiseIncludes(bool isTxPowerInclude, bool isLocalNameInclude, bool isAppearanceInclude)
{
	const char* includes[3] = {0,0,0};
	int includeIndex = 0;

	if (isTxPowerInclude)
		includes[includeIndex++] = "tx-power";
	if (isLocalNameInclude)
		includes[includeIndex++] = "local-name";
	if (isAppearanceInclude)
		includes[includeIndex++] = "appearance";

	bluez_leadvertisement1_set_includes(mInterface, includes);
	const gchar *const *include = bluez_leadvertisement1_get_includes(mInterface);
	DEBUG("ad include property changed to %s", include[includeIndex-1]);
}

void Bluez5Advertise::advertiseDuration(uint16_t value)
{
	bluez_leadvertisement1_set_duration(mInterface, value);
}

void Bluez5Advertise::advertiseTimeout(uint16_t value)
{
	bluez_leadvertisement1_set_timeout(mInterface, value);
}

void Bluez5Advertise::setAdRole(std::string role)
{
	bluez_leadvertisement1_set_type_(mInterface, role.c_str());
	DEBUG("role %s", role.c_str());
}

gboolean Bluez5Advertise::handleRelease(BluezLEAdvertisement1 *proxy, GDBusMethodInvocation *invocation, gpointer user_data)
{
	DEBUG("Advertising released");
	Bluez5Advertise *pThis = static_cast<Bluez5Advertise*> (user_data);
	pThis->unRegisterAdvertisement();
	bluez_leadvertisement1_complete_release(proxy, invocation);
	return TRUE;
}

void Bluez5Advertise::clearAllProperties()
{
	bluez_leadvertisement1_set_service_uuids(mInterface, NULL);
	bluez_leadvertisement1_set_service_data(mInterface, NULL);
	bluez_leadvertisement1_set_manufacturer_data(mInterface, NULL);
	bluez_leadvertisement1_set_type_(mInterface, NULL);
	bluez_leadvertisement1_set_timeout(mInterface, 0);
	bluez_leadvertisement1_set_include_tx_power(mInterface, FALSE);
	bluez_leadvertisement1_set_duration(mInterface, 0);
	bluez_leadvertisement1_set_local_name(mInterface, NULL);
	bluez_leadvertisement1_set_includes(mInterface, NULL);
	bluez_leadvertisement1_set_appearance(mInterface, UINT16_MAX);
}
