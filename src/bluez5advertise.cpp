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
#define BLUEZ5_ADVERTISE_OBJECT_PATH        "/advetise/advId"

Bluez5Advertise::Bluez5Advertise(BluezLEAdvertisingManager1 *advManager, Bluez5SIL *sil):
	mBusId(0),
	mTxPower(false),
	mAdvManager(advManager),
	mSIL(sil),
	mConn(nullptr)
{
	mBusId = g_bus_own_name(G_BUS_TYPE_SYSTEM, BLUEZ5_ADVERTISE_BUS_NAME,
				G_BUS_NAME_OWNER_FLAGS_NONE,
				handleBusAcquired, NULL, NULL, this, NULL);
}

Bluez5Advertise::~Bluez5Advertise()
{
	mAdvertiserMap.clear();

	if (mConn)
	{
		g_object_unref(mConn);
	}

	if (mBusId)
	{
		g_bus_unown_name(mBusId);
	}
}

void Bluez5Advertise::handleBusAcquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	Bluez5Advertise *adv = static_cast<Bluez5Advertise*>(user_data);
	adv->setConnection(connection);
	g_object_unref(connection);
}

std::string Bluez5Advertise::createInterface(uint8_t advId)
{
	BluezLEAdvertisement1 *interface = bluez_leadvertisement1_skeleton_new();
	GError *error = 0;

	std::string path = BLUEZ5_ADVERTISE_OBJECT_PATH + std::string("/") + std::to_string(advId);

	g_signal_connect(interface, "handle-release", G_CALLBACK(handleRelease), this);

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(interface), mConn,
                                          path.c_str(), &error))
	{
		DEBUG("Failed to initialize agent on bus: %s",error->message);
		g_error_free(error);
		return nullptr;
	}

	AdvertiseObject *advObject = new (std::nothrow) AdvertiseObject(interface, path);
	if (advObject)
	{
		std:: unique_ptr <AdvertiseObject> p(advObject);
		mAdvertiserMap[advId] = std::move(p);
	}
	return path;
}

uint8_t Bluez5Advertise::nextAdvId()
{
	static uint8_t nextAdvId = 1;
	return nextAdvId++;
}

void Bluez5Advertise::createAdvertisementId(AdvertiserIdStatusCallback callback)
{
	uint8_t advId = nextAdvId();
	std::string path = createInterface(advId);
	callback(BLUETOOTH_ERROR_NONE, advId);
}

void Bluez5Advertise::registerAdvertisement(uint8_t advId, AdvertiserStatusCallback callback)
{
	guchar supportedInstace = bluez_leadvertising_manager1_get_supported_instances(mAdvManager);
	if (!supportedInstace)
	{
		ERROR(MSGID_BLE_ADVERTIMENT_ERROR, 0, "activeInstaces full");
		callback(BLUETOOTH_ERROR_FAIL);
		return;
	}

	GVariantBuilder *builder = 0;
	GVariant *arguments = 0;
	builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
	arguments = g_variant_builder_end (builder);
	g_variant_builder_unref(builder);

	auto registerCallback = [this, callback, advId](GAsyncResult *result)
	{
		GError *error = 0;
		gboolean ret;

		ret = bluez_leadvertising_manager1_call_register_advertisement_finish(mAdvManager, result, &error);
		if (error)
		{
			ERROR(MSGID_BLE_ADVERTIMENT_ERROR, 0, "adv registration failed due to %s", error->message);
			g_error_free(error);
			callback(BLUETOOTH_ERROR_FAIL);
			return;
		}
		callback(BLUETOOTH_ERROR_NONE);
	};

	std::string path = getPath(advId);

	bluez_leadvertising_manager1_call_register_advertisement(mAdvManager, path.c_str(), arguments, NULL,
                                                             glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(registerCallback));
}

gboolean Bluez5Advertise::unRegisterAdvertisement(uint8_t advId)
{
	std::string path = getPath(advId);
	removeAdvertise(advId);
	return bluez_leadvertising_manager1_call_unregister_advertisement_sync(mAdvManager, path.c_str(), NULL, NULL);
}

void Bluez5Advertise::advertiseServiceUuids(uint8_t advId, std::unordered_map<std::string, std::vector<uint8_t> > serviceList)
{
	BluezLEAdvertisement1 *interface = getInteface(advId);
	if (!interface)
		return;

	const char *uuids[serviceList.size()+1];

	int i = 0;
	for (auto it = serviceList.begin(); it != serviceList.end(); it++)
	{
		uuids[i++] = it->first.c_str();
	}

	uuids[i] = NULL;
	bluez_leadvertisement1_set_service_uuids (interface, uuids);
}

void Bluez5Advertise::advertiseServiceData(uint8_t advId, const char* uuids, std::vector<uint8_t> serviceData)
{
	BluezLEAdvertisement1 *interface = getInteface(advId);
	if (!interface)
		return;

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

	bluez_leadvertisement1_set_service_data(interface, arguments);
}

void Bluez5Advertise::advertiseManufacturerData(uint8_t advId, std::vector<uint8_t> data)
{
	BluezLEAdvertisement1 *interface = getInteface(advId);
	if (!interface)
		return;

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
	else
	{
		DEBUG("Manufacturer data is less than 2 bytes, manufacturer data is ignored");
		return;
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

	bluez_leadvertisement1_set_manufacturer_data(interface, arguments);
}

void Bluez5Advertise::advertiseTxPower(uint8_t advId, bool value)
{
	BluezLEAdvertisement1 *interface = getInteface(advId);
	if (!interface)
		return;

	if (mTxPower == value)
	{
		DEBUG("advertiseTxPower already in same state ");
		return;
	}

	mTxPower = value;
	bluez_leadvertisement1_set_include_tx_power(interface, mTxPower);
}

void Bluez5Advertise::advertiseLocalName(uint8_t advId, const char *name)
{
	BluezLEAdvertisement1 *interface = getInteface(advId);
	if (!interface)
		return;
	bluez_leadvertisement1_set_local_name(interface, name);
}

void Bluez5Advertise::advertiseAppearance(uint8_t advId, guint16 value)
{
	BluezLEAdvertisement1 *interface = getInteface(advId);
	if (!interface)
		return;
	bluez_leadvertisement1_set_appearance(interface, value);
}

void Bluez5Advertise::advertiseIncludes(uint8_t advId, bool isTxPowerInclude, bool isLocalNameInclude, bool isAppearanceInclude)
{
	BluezLEAdvertisement1 *interface = getInteface(advId);
	if (!interface)
		return;
	const char* includes[3] = {0,0,0};
	int includeIndex = 0;

	if (isTxPowerInclude)
		includes[includeIndex++] = "tx-power";
	if (isLocalNameInclude)
		includes[includeIndex++] = "local-name";
	if (isAppearanceInclude)
		includes[includeIndex++] = "appearance";

	bluez_leadvertisement1_set_includes(interface, includes);
	const gchar *const *include = bluez_leadvertisement1_get_includes(interface);
	DEBUG("ad include property changed to %s", include[includeIndex-1]);
}

void Bluez5Advertise::advertiseDuration(uint8_t advId, uint16_t value)
{
	BluezLEAdvertisement1 *interface = getInteface(advId);
	if (!interface)
		return;

	bluez_leadvertisement1_set_duration(interface, value);
}

void Bluez5Advertise::advertiseTimeout(uint8_t advId, uint16_t value)
{
	BluezLEAdvertisement1 * interface = getInteface(advId);
	if (!interface)
		return;

	bluez_leadvertisement1_set_timeout(interface, value);
}

void Bluez5Advertise::setAdRole(uint8_t advId, std::string role)
{
	BluezLEAdvertisement1 * interface = getInteface(advId);
	if (!interface)
		return;

	bluez_leadvertisement1_set_type_(interface, role.c_str());
	DEBUG("role %s", role.c_str());
}

gboolean Bluez5Advertise::handleRelease(BluezLEAdvertisement1 *proxy, GDBusMethodInvocation *invocation, gpointer user_data)
{
	DEBUG("Advertising released");
	Bluez5Advertise *pThis = static_cast<Bluez5Advertise*> (user_data);
	//pThis->unRegisterAdvertisement();
	bluez_leadvertisement1_complete_release(proxy, invocation);
	return TRUE;
}

void Bluez5Advertise::clearAllProperties(uint8_t advId)
{
	BluezLEAdvertisement1 * interface = getInteface(advId);
	if (!interface)
		return;

	bluez_leadvertisement1_set_service_uuids(interface, NULL);
	bluez_leadvertisement1_set_service_data(interface, NULL);
	bluez_leadvertisement1_set_manufacturer_data(interface, NULL);
	bluez_leadvertisement1_set_type_(interface, NULL);
	bluez_leadvertisement1_set_timeout(interface, 0);
	bluez_leadvertisement1_set_include_tx_power(interface, FALSE);
	bluez_leadvertisement1_set_duration(interface, 0);
	bluez_leadvertisement1_set_local_name(interface, NULL);
	bluez_leadvertisement1_set_includes(interface, NULL);
	bluez_leadvertisement1_set_appearance(interface, UINT16_MAX);
}

BluezLEAdvertisement1 *Bluez5Advertise::getInteface(uint8_t advId)
{
	auto it = mAdvertiserMap.find(advId);
	if (it == mAdvertiserMap.end())
	{
		return nullptr;
	}

	AdvertiseObject *obj = (it->second).get();
	return obj->mInterface;
}

std::string Bluez5Advertise::getPath(uint8_t advId)
{
	auto it = mAdvertiserMap.find(advId);
	if (it == mAdvertiserMap.end())
	{
		return nullptr;
	}

	AdvertiseObject *obj = (it->second).get();
	return obj->mPath;
}

void Bluez5Advertise::removeAdvertise(uint8_t advId)
{
	mAdvertiserMap.erase(advId);
}
