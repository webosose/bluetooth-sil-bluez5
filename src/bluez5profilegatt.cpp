// Copyright (c) 2018-2019 LG Electronics, Inc.
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

#include <string>
#include <unordered_map>

#include "logging.h"
#include "bluez5adapter.h"
#include "bluez5agent.h"
#include "asyncutils.h"
#include "utils.h"
#include "bluez5profilegatt.h"
#include "bluez5gattremoteattribute.h"

const std::string BLUETOOTH_PROFILE_GATT_UUID = "00001801-0000-1000-8000-00805f9b34fb";

#define BLUEZ5_GATT_BUS_NAME           "com.webos.gatt"
#define BLUEZ5_GATT_OBJECT_PATH        "/org/bluez/gattApp"

#define CLIENT_PATH "/client"
#define SERVER_PATH "/server"

#define BLUEZ5_GATT_OBJECT_CLIENT_PATH BLUEZ5_GATT_OBJECT_PATH CLIENT_PATH
#define BLUEZ5_GATT_OBJECT_SERVER_PATH BLUEZ5_GATT_OBJECT_PATH SERVER_PATH


Bluez5ProfileGatt::Bluez5ProfileGatt(Bluez5Adapter *adapter):
	Bluez5ProfileBase(adapter, BLUETOOTH_PROFILE_GATT_UUID),
	mBusId(0),
	mLastCharId(0),
	mConn(nullptr),
	mAdapter(adapter),
	mObjectManagerGattServer(nullptr)
{
	DEBUG("Bluez5ProfileGatt created");
	mBusId = g_bus_own_name(G_BUS_TYPE_SYSTEM, BLUEZ5_GATT_BUS_NAME,
				G_BUS_NAME_OWNER_FLAGS_NONE,
				handleBusAcquired, NULL, NULL, this, NULL);
	registerSignalHandlers();
}

Bluez5ProfileGatt::~Bluez5ProfileGatt()
{
	DEBUG("Bluez5ProfileGatt dtor");

	if (mObjectManagerGattServer)
	{
		g_object_unref(mObjectManagerGattServer);
		mObjectManagerGattServer = 0;
	}

	if (mBusId)
	{
		g_bus_unown_name(mBusId);
		mBusId = 0;
	}
}

void Bluez5ProfileGatt::handleBusAcquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	Bluez5ProfileGatt *pThis = static_cast<Bluez5ProfileGatt*>(user_data);
	pThis->setGdbusConnection(connection);
	pThis->createObjectManagers();
}

void Bluez5ProfileGatt::addRemoteServiceToDevice(GattRemoteService* gattService)
{
	Bluez5Device* device = mAdapter->findDeviceByObjectPath(gattService->parentObjectPath);
	if(!device)
		return;

	std::string deviceAddress = device->getAddress();
	std::string lowerCaseAddress = convertAddressToLowerCase(deviceAddress);

	auto deviceServicesIter = mDeviceServicesMap.find(lowerCaseAddress);

	if (deviceServicesIter == mDeviceServicesMap.end())
	{
		mDeviceServicesMap.insert({ lowerCaseAddress, { gattService }});
		getGattObserver()->serviceFound(lowerCaseAddress, gattService->service);

		/* Send connect status*/
		BluetoothPropertiesList properties;
		properties.push_back(BluetoothProperty(BluetoothProperty::Type::CONNECTED, true));
		getObserver()->propertiesChanged(lowerCaseAddress, properties);
	}
	else
	{
		auto &servicesList = deviceServicesIter->second;
		auto serviceIter = std::find_if (servicesList.begin(), servicesList.end(),
										 [gattService]( GattRemoteService* devService)
		{
			return devService->service.getUuid() == gattService->service.getUuid();
		});

		if (serviceIter == servicesList.end())
		{
			servicesList.push_back(gattService);
			getGattObserver()->serviceFound(lowerCaseAddress, gattService->service);
			updateRemoteDeviceServices();
		}
	}
}

void Bluez5ProfileGatt::createRemoteGattService(const std::string &serviceObjectPath)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	GError *error = 0;

	BluezGattService1 *interface = bluez_gatt_service1_proxy_new_for_bus_sync(
									G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, "org.bluez",
									serviceObjectPath.c_str(), NULL, &error);

	if (error)
	{
		ERROR(MSGID_GATT_PROFILE_ERROR, 0, "Failed to get Gatt Service on path %s: %s",
			  serviceObjectPath.c_str(), error->message);
		g_error_free(error);
		return;
	}

	BluetoothGattService service;
	const char* uuid = bluez_gatt_service1_get_uuid(interface);
	if (uuid)
		service.setUuid(BluetoothUuid(uuid));

	if (bluez_gatt_service1_get_primary(interface))
		service.setType(BluetoothGattService::PRIMARY);
	else
		service.setType(BluetoothGattService::SECONDARY);

	GattRemoteService* gattService = new (std::nothrow) GattRemoteService(interface);
	if (!gattService)
		return;

	gattService->service = service;
	gattService->objectPath = serviceObjectPath;

	const char* deviceObjectPath = bluez_gatt_service1_get_device(interface);
	if (deviceObjectPath)
		gattService->parentObjectPath = deviceObjectPath;

	addRemoteServiceToDevice(gattService);
}

GattRemoteService* Bluez5ProfileGatt::getRemoteGattService(std::string& serviceObjectPath)
{
	GattRemoteService* foundService = NULL;
	std::string deviceObjectPath, serviceName;
	splitInPathAndName(serviceObjectPath, deviceObjectPath, serviceName);

	Bluez5Device* device = mAdapter->findDeviceByObjectPath(deviceObjectPath);
	if(!device)
		return foundService;

	std::string deviceAddress = device->getAddress();
	std::string lowerCaseAddress = convertAddressToLowerCase(deviceAddress);

	auto deviceServicesIter = mDeviceServicesMap.find(lowerCaseAddress);

	if (deviceServicesIter != mDeviceServicesMap.end())
	{
		auto &servicesList = deviceServicesIter->second;
		auto serviceIter = std::find_if (servicesList.begin(), servicesList.end(),
										 [serviceObjectPath](GattRemoteService* devService)
		{
			return devService->objectPath == serviceObjectPath;
		});

		if (serviceIter != servicesList.end())
		{
			foundService = (*serviceIter);
		}
	}

	return foundService;
}

void Bluez5ProfileGatt::addRemoteCharacteristicToService(GattRemoteCharacteristic* gattCharacteristic)
{
	GattRemoteService* service = getRemoteGattService(gattCharacteristic->parentObjectPath);
	if (service)
	{
		service->gattRemoteCharacteristics.push_back(gattCharacteristic);
		service->service.addCharacteristic(gattCharacteristic->characteristic);
	}
}

void Bluez5ProfileGatt::createRemoteGattCharacteristic(const std::string &characteristicObjectPath)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	GError *error = 0;
	BluezGattCharacteristic1 *interface = bluez_gatt_characteristic1_proxy_new_for_bus_sync(
											G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
											"org.bluez", characteristicObjectPath.c_str(), NULL, &error);

	if (error)
	{
		ERROR(MSGID_GATT_PROFILE_ERROR, 0, "Failed to get Gatt Characteristic on path %s: %s",
			  characteristicObjectPath.c_str(), error->message);
		g_error_free(error);
		return;
	}

	BluetoothGattCharacteristic gattCharacteristic;

	const char* uuid = bluez_gatt_characteristic1_get_uuid(interface);
	if (uuid)
		gattCharacteristic.setUuid(BluetoothUuid(uuid));



	GattRemoteCharacteristic* gattRemoteCharacteristic = new (std::nothrow) GattRemoteCharacteristic(interface, this);
	if (!gattRemoteCharacteristic)
		return;

	gattRemoteCharacteristic->objectPath = characteristicObjectPath;

	const char* gattServiceObjectPath = bluez_gatt_characteristic1_get_service(interface);
	if (gattServiceObjectPath)
		gattRemoteCharacteristic->parentObjectPath = gattServiceObjectPath;

	gattCharacteristic.setProperties(gattRemoteCharacteristic->readProperties());
	gattRemoteCharacteristic->characteristic = gattCharacteristic;

	if (gattRemoteCharacteristic->characteristic.isPropertySet(BluetoothGattCharacteristic::Property::PROPERTY_READ))
	{
		BluetoothGattValue charValue = gattRemoteCharacteristic->readValue();
		gattRemoteCharacteristic->characteristic.setValue(charValue);
	}
	addRemoteCharacteristicToService(gattRemoteCharacteristic);
}

void Bluez5ProfileGatt::removeRemoteGattCharacteristic(const std::string &characteristicObjectPath)
{
	std::string serviceObjectPath, characteristicName;
	splitInPathAndName(characteristicObjectPath, serviceObjectPath, characteristicName);

	GattRemoteService* service = getRemoteGattService(serviceObjectPath);
	if (service)
	{
		auto &characteristicList = service->gattRemoteCharacteristics;

		auto characteristicIter = std::find_if (characteristicList.begin(), characteristicList.end(),
												[characteristicObjectPath](GattRemoteCharacteristic* characteristic)
		{
			return characteristic->objectPath == characteristicObjectPath;
		});

		if (characteristicIter != characteristicList.end())
		{
			g_object_unref((*characteristicIter)->mInterface);
			delete (*characteristicIter);
			characteristicList.erase(characteristicIter);
		}
	}
}

void Bluez5ProfileGatt::addRemoteDescriptorToCharacteristic(GattRemoteDescriptor* gattDescriptor)
{
	std::string characteristicObjectPath = gattDescriptor->parentObjectPath;
	std::string serviceObjectPath, characteristicName;

	splitInPathAndName(characteristicObjectPath, serviceObjectPath, characteristicName);

	GattRemoteService* remoteService = getRemoteGattService(serviceObjectPath);
	if (remoteService)
	{
		auto &characteristicList = remoteService->gattRemoteCharacteristics;

		auto characteristicIter = std::find_if (characteristicList.begin(), characteristicList.end(),
												[characteristicObjectPath, &remoteService](GattRemoteCharacteristic* characteristic)
		{
			return characteristic->objectPath == characteristicObjectPath;
		});

		if (characteristicIter != characteristicList.end())
		{
			if ((*characteristicIter)->characteristic.isPropertySet(BluetoothGattCharacteristic::Property::PROPERTY_READ))
			{
				BluetoothGattValue descValue = gattDescriptor->readValue();
				gattDescriptor->descriptor.setValue(descValue);
			}

			(*characteristicIter)->gattRemoteDescriptors.push_back(gattDescriptor);
			(*characteristicIter)->characteristic.addDescriptor(gattDescriptor->descriptor);

			const BluetoothUuid characteristicUuid = (*characteristicIter)->characteristic.getUuid();
			BluetoothGattCharacteristicList serviceCharacteristicList =  remoteService->service.getCharacteristics();

			auto serviceCharacteristicIter = std::find_if (serviceCharacteristicList.begin(), serviceCharacteristicList.end(),
														   [characteristicUuid](BluetoothGattCharacteristic characteristic)
			{
				return characteristic.getUuid() == characteristicUuid;
			});

			if (serviceCharacteristicIter != serviceCharacteristicList.end())
			{
				(*serviceCharacteristicIter).addDescriptor(gattDescriptor->descriptor);
				remoteService->service.setCharacteristics(serviceCharacteristicList);
			}
		}
	}
}

void Bluez5ProfileGatt::createRemoteGattDescriptor(const std::string &descriptorObjectPath)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	GError *error = 0;
	BluezGattDescriptor1 *interface = bluez_gatt_descriptor1_proxy_new_for_bus_sync(
										G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
										"org.bluez", descriptorObjectPath.c_str(), NULL, &error);

	if (error)
	{
		ERROR(MSGID_GATT_PROFILE_ERROR, 0, "Failed to get Gatt Descriptor on path %s: %s",
			  descriptorObjectPath.c_str(), error->message);
		g_error_free(error);
		return;
	}

	BluetoothGattDescriptor gattDescriptor;

	const char* uuid = bluez_gatt_descriptor1_get_uuid(interface);
	if (uuid)
		gattDescriptor.setUuid(BluetoothUuid(uuid));


	GattRemoteDescriptor* gattRemoteDescriptor = new (std::nothrow) GattRemoteDescriptor(interface);
	if (!gattRemoteDescriptor)
		return;

	gattRemoteDescriptor->objectPath = descriptorObjectPath;

	const char* gattCharacteristic = bluez_gatt_descriptor1_get_characteristic(interface);
	if (gattCharacteristic)
		gattRemoteDescriptor->parentObjectPath = gattCharacteristic;

	gattRemoteDescriptor->descriptor = gattDescriptor;

	addRemoteDescriptorToCharacteristic(gattRemoteDescriptor);
}

void Bluez5ProfileGatt::removeRemoteGattDescriptor(const std::string &descriptorObjectPath)
{
	std::string characteristicObjectPath, descriptorName;
	splitInPathAndName(descriptorObjectPath, characteristicObjectPath, descriptorName);

	std::string serviceObjectPath, characteristicName;
	splitInPathAndName(characteristicObjectPath, serviceObjectPath, characteristicName);

	GattRemoteService* service = getRemoteGattService(serviceObjectPath);
	if (service)
	{
		auto &characteristicList = service->gattRemoteCharacteristics;

		auto characteristicIter = std::find_if (characteristicList.begin(), characteristicList.end(),
												[characteristicObjectPath](GattRemoteCharacteristic* characteristic)
		{
			return characteristic->objectPath == characteristicObjectPath;
		});

		if (characteristicIter != characteristicList.end())
		{
			auto &descriptorsList = (*characteristicIter)->gattRemoteDescriptors;
			auto descriptorsIter = std::find_if (descriptorsList.begin(), descriptorsList.end(),
												 [descriptorObjectPath]( GattRemoteDescriptor* descriptor)
			{
				return descriptor->objectPath == descriptorObjectPath;
			});

			if (descriptorsIter != descriptorsList.end())
			{
				if ((*descriptorsIter)->mInterface)
				{
					g_object_unref((*descriptorsIter)->mInterface);
					(*descriptorsIter)->mInterface = nullptr;
				}
				delete (*descriptorsIter);
				descriptorsList.erase(descriptorsIter);
			}
		}
	}
}

void Bluez5ProfileGatt::removeRemoteGattService(const std::string &serviceObjectPath)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	std::string deviceObjPath, serviceName;
	splitInPathAndName(serviceObjectPath, deviceObjPath, serviceName);

	Bluez5Device* device = mAdapter->findDeviceByObjectPath(deviceObjPath);
	if(!device)
		return;

	std::string deviceAddress = device->getAddress();
	std::string lowerCaseAddress = convertAddressToLowerCase(deviceAddress);

	auto deviceServicesIter = mDeviceServicesMap.find(lowerCaseAddress);

	if (deviceServicesIter != mDeviceServicesMap.end())
	{
		auto &servicesList = deviceServicesIter->second;
		auto serviceIter = std::find_if (servicesList.begin(), servicesList.end(),
										 [serviceObjectPath]( GattRemoteService* devService)
		{
			return devService->objectPath == serviceObjectPath;
		});

		if (serviceIter != servicesList.end())
		{
			getGattObserver()->serviceLost(lowerCaseAddress, (*serviceIter)->service);
			g_object_unref((*serviceIter)->mInterface);
			delete (*serviceIter);
			servicesList.erase(serviceIter);
		}
		if (servicesList.size() == 0)
		{
			mDeviceServicesMap.erase(deviceServicesIter);
			BluetoothPropertiesList properties;
			properties.push_back(BluetoothProperty(BluetoothProperty::Type::CONNECTED, false));
			getObserver()->propertiesChanged(lowerCaseAddress, properties);
		}
	}
}

void Bluez5ProfileGatt::handleObjectAdded(GDBusObjectManager *objectManager, GDBusObject *object,
											void *user_data)
{
	DEBUG("%s::%s %s",__FILE__,__FUNCTION__,g_dbus_object_get_object_path(object));
	UNUSED(objectManager);

	Bluez5ProfileGatt *pThis = static_cast<Bluez5ProfileGatt*>(user_data);

	if (!pThis)
		return;

	if (auto serviceInterface = g_dbus_object_get_interface(object, "org.bluez.GattService1"))
	{
		auto objectPath = g_dbus_object_get_object_path(object);

		pThis->createRemoteGattService(std::string(objectPath));
		g_object_unref(serviceInterface);
	}
	else if (auto characteristicInterface = g_dbus_object_get_interface(object, "org.bluez.GattCharacteristic1"))
	{
		auto objectPath = g_dbus_object_get_object_path(object);

		pThis->createRemoteGattCharacteristic(std::string(objectPath));
		g_object_unref(characteristicInterface);
	}
	else if (auto descriptorInterface = g_dbus_object_get_interface(object, "org.bluez.GattDescriptor1"))
	{
		auto objectPath = g_dbus_object_get_object_path(object);

		pThis->createRemoteGattDescriptor(std::string(objectPath));
		g_object_unref(descriptorInterface);
	}
}

void Bluez5ProfileGatt::handleObjectRemoved(GDBusObjectManager *objectManager, GDBusObject *object,
											void *user_data)
{
	DEBUG("%s::%s objectpath %s",__FILE__,__FUNCTION__, g_dbus_object_get_object_path(object));
	UNUSED(objectManager);

	Bluez5ProfileGatt *pThis = static_cast<Bluez5ProfileGatt*>(user_data);
	if (!pThis)
		return;

	if (auto adapterInterface = g_dbus_object_get_interface(object, "org.bluez.GattService1"))
	{
		auto objectPath = g_dbus_object_get_object_path(object);

		pThis->removeRemoteGattService(std::string(objectPath));
		g_object_unref(adapterInterface);
	}
	else if (auto characteristicInterface = g_dbus_object_get_interface(object, "org.bluez.GattCharacteristic1"))
	{
		auto objectPath = g_dbus_object_get_object_path(object);

		pThis->removeRemoteGattCharacteristic(std::string(objectPath));
		g_object_unref(characteristicInterface);
	}
	else if (auto descriptorInterface = g_dbus_object_get_interface(object, "org.bluez.GattDescriptor1"))
	{
		auto objectPath = g_dbus_object_get_object_path(object);

		pThis->removeRemoteGattDescriptor(std::string(objectPath));
		g_object_unref(descriptorInterface);
	}
}

void Bluez5ProfileGatt::updateDeviceProperties(std::string deviceAddress)
{
	std::string lowerCaseAddress = convertAddressToLowerCase(deviceAddress);
	int connectId = getConnectId(lowerCaseAddress);
	if (mConnectedDevices.find(connectId) != mConnectedDevices.end())
		mConnectedDevices.erase(connectId);
	auto deviceServicesIter = mDeviceServicesMap.find(lowerCaseAddress);
	if (deviceServicesIter != mDeviceServicesMap.end())
		mDeviceServicesMap.erase(deviceServicesIter);
	auto deviceRemoteServicesIter = mRemoteDeviceServicesMap.find(lowerCaseAddress);
	if (deviceRemoteServicesIter != mRemoteDeviceServicesMap.end())
		mRemoteDeviceServicesMap.erase(deviceRemoteServicesIter);
	BluetoothPropertiesList properties;
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::CONNECTED, false));
	getObserver()->propertiesChanged(lowerCaseAddress, properties);
}

void Bluez5ProfileGatt::registerSignalHandlers()
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	GError *error = 0;

	mObjectManager = g_dbus_object_manager_client_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
											G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
											"org.bluez", "/", NULL, NULL, NULL, NULL, &error);

	if (error)
	{
		ERROR(MSGID_OBJECT_MANAGER_CREATION_FAILED, 0, "Failed to create object manager: %s", error->message);
		g_error_free(error);
		return;
	}

	g_signal_connect(mObjectManager, "object-added", G_CALLBACK(handleObjectAdded), this);
	g_signal_connect(mObjectManager, "object-removed", G_CALLBACK(handleObjectRemoved), this);
}

void Bluez5ProfileGatt::connectGatt(const uint16_t & appId, bool autoConnection, const std::string & address, BluetoothConnectCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	Bluez5Device *device = mAdapter->findDevice(address);
	if (!device)
	{
		callback(BLUETOOTH_ERROR_PARAM_INVALID, -1);
		return;
	}
	std::string deviceAddress = device->getAddress();
	std::string lowerCaseAddress = convertAddressToLowerCase(deviceAddress);
	auto isConnectCallback = [this, lowerCaseAddress, callback, appId](BluetoothError error) {
		if (error != BLUETOOTH_ERROR_NONE)
		{
			callback(error, -1);
			return;
		}

		if (mConnectedDevices.find(appId) == mConnectedDevices.end())
		{
			mConnectedDevices.insert({ appId, lowerCaseAddress});
			callback(BLUETOOTH_ERROR_NONE, appId);
		}
	};
	device->connectGatt(isConnectCallback);
}

void Bluez5ProfileGatt::disconnectGatt(const uint16_t &appId, const uint16_t &connectId, const std::string &address, BluetoothResultCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	std::string deviceAddress;
	auto deviceInfo = mConnectedDevices.find(appId);
	if (deviceInfo == mConnectedDevices.end())
	{
		callback(BLUETOOTH_ERROR_FAIL);
		return;
	}
	else
	{
		deviceAddress = deviceInfo->second;
	}

	Bluez5Device *device = mAdapter->findDevice(deviceAddress);
	if (!device)
	{
		DEBUG("Could not find device with address %s while trying to disconnect", address.c_str());
		callback(BLUETOOTH_ERROR_PARAM_INVALID);
		return;
	}

	auto isDisconnectCallback = [this, deviceInfo, deviceAddress, callback](BluetoothError error)
	{
		if (error != BLUETOOTH_ERROR_NONE)
		{
			callback(error);
			return;
		}
		mConnectedDevices.erase(deviceInfo);

		GError *err = nullptr;
		Bluez5Device *device = mAdapter->findDevice(deviceAddress);
		if (device)
		{
			bluez_adapter1_call_remove_device_sync(mAdapter->getAdapterProxy(), device->getObjectPath().c_str(), NULL, &err);
			if (err)
			{
				g_error_free(err);
			}
		}

		callback(BLUETOOTH_ERROR_NONE);
	};

	device->disconnect(isDisconnectCallback);
}

uint16_t Bluez5ProfileGatt::nextAppId()
{
	static uint16_t nextAppId = 1;
	return nextAppId++;
}

uint16_t Bluez5ProfileGatt::nextServiceId()
{
	static uint16_t nextServiceId = 1;
	return nextServiceId++;
}

uint16_t Bluez5ProfileGatt::nextCharId()
{
	static uint16_t nextCharId = 1;
	return nextCharId++;
}

uint16_t Bluez5ProfileGatt::nextDescId()
{
	static uint16_t nextDescId = 1;
	return nextDescId++;
}

void Bluez5ProfileGatt::getProperties(const std::string &address, BluetoothPropertiesResultCallback  callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
}

void Bluez5ProfileGatt::getProperty(const std::string &address, BluetoothProperty::Type type,
					BluetoothPropertyResultCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	BluetoothProperty prop(type);
	auto deviceIter = mDeviceServicesMap.find(address);
	if (deviceIter != mDeviceServicesMap.end())
	{
		prop.setValue<bool>(true);
		callback(BLUETOOTH_ERROR_NONE, prop);
		return;
	}
	else
	{
		prop.setValue<bool>(false);
		callback(BLUETOOTH_ERROR_NONE, prop);
		return;
	}
}

uint16_t Bluez5ProfileGatt::addApplication(const BluetoothUuid &appUuid, ApplicationType type)
{
	int appId = nextAppId();

	if (type == CLIENT)
	{
		DEBUG("RegisterApplication as client");
	}
	else if (type == SERVER)
	{
		DEBUG("RegisterApplication as server");
		std::unique_ptr <BluezGattLocalApplication> application(new BluezGattLocalApplication());
		mGattLocalApplications.insert(std::make_pair(appId, std::move(application)));
	}

	return appId;
}

bool Bluez5ProfileGatt::removeApplication(uint16_t appId, ApplicationType type)
{
	if (type == CLIENT)
	{
		DEBUG("removeApplication as client");
	}
	else if (type == SERVER)
	{
		DEBUG("removeApplication as server");
		std::string objPath = g_dbus_object_manager_get_object_path(G_DBUS_OBJECT_MANAGER(mObjectManagerGattServer));
		auto it = mGattLocalApplications.find(appId);

		if (it == mGattLocalApplications.end())
		{
			return true;
		}

		auto &services = it->second->mGattLocalServices;

		//Remove all services
		for (auto &serviceIt : services)
		{
			std::unique_ptr <Bluez5GattLocalService> &service = serviceIt.second;
			removeLocalServices(service.get());
		}

		auto registerCallback = [this](BluetoothError error)
		{
			if (error == BLUETOOTH_ERROR_NONE)
			{
				DEBUG("Removed application and Registered Application successfully");
			}
			else
			{
				ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Removed application  and register application failed %d", error);
			}
			return;
		};

		registerLocalApplication(registerCallback, objPath, true);
	}
	return true;
}

void Bluez5ProfileGatt::registerLocalApplication(BluetoothResultCallback callback, const std::string &objPath, bool unRegisterFirst)
{
	GVariantBuilder *builder = 0;
	GVariant *arguments = 0;
	builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
	arguments = g_variant_builder_end (builder);
	g_variant_builder_unref(builder);

	auto registerCallback = [this, callback](GAsyncResult *result)
	{
		GError *error = NULL;
		gboolean ret;

		ret = bluez_gatt_manager1_call_register_application_finish(mAdapter->getGattManager(), result, &error);

		if (error)
		{
			ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Failed to register the application: %s", error->message);
			g_error_free(error);
			callback(BLUETOOTH_ERROR_FAIL);
			return;
		}

		callback(BLUETOOTH_ERROR_NONE);
		return;
	};

	if (unRegisterFirst)
	{
		GError *error = NULL;
		bluez_gatt_manager1_call_unregister_application_sync(mAdapter->getGattManager(), objPath.c_str(), NULL, &error);
		if (error)
		{
			ERROR("MSGID_GATT_PROFILE_ERROR", 0, "unRegister the application: %s", error->message);
			g_error_free(error);
		}
	}

	bluez_gatt_manager1_call_register_application(mAdapter->getGattManager(), objPath.c_str(), arguments, NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(registerCallback));
}

gboolean Bluez5ProfileGatt::handleRelease(BluezGattProfile1 *proxy, GDBusMethodInvocation *invocation, gpointer user_data)
{
	DEBUG("Bluez5ProfileGatt released");
	bluez_gatt_profile1_complete_release(proxy, invocation);
	return TRUE;
}

void Bluez5ProfileGatt::discoverServices(BluetoothResultCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	if (mRemoteDeviceServicesMap.size())
		callback(BLUETOOTH_ERROR_NONE);
	else
		callback(BLUETOOTH_ERROR_FAIL);
}

void Bluez5ProfileGatt::updateRemoteDeviceServices()
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	if (mRemoteDeviceServicesMap.size())
		mRemoteDeviceServicesMap.clear();

	for (auto &deviceServicesIter : mDeviceServicesMap)
	{
		auto &servicesList = deviceServicesIter.second;
		BluetoothGattServiceList serviceList;

		std::for_each(servicesList.begin(), servicesList.end(),
					  [&serviceList](GattRemoteService* devService)
		{
			serviceList.push_back(devService->service);
		});

		mRemoteDeviceServicesMap[deviceServicesIter.first] = serviceList;
	}

}

void Bluez5ProfileGatt::discoverServices(const std::string &address, BluetoothResultCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	if (mRemoteDeviceServicesMap.size())
		mRemoteDeviceServicesMap.clear();

	auto deviceServicesIter = mDeviceServicesMap.find(address);
	if (deviceServicesIter != mDeviceServicesMap.end())
	{
		auto &servicesList = deviceServicesIter->second;
		BluetoothGattServiceList serviceList;

		std::for_each(servicesList.begin(), servicesList.end(),
					  [&serviceList](GattRemoteService* devService)
		{
			serviceList.push_back(devService->service);
		});

		mRemoteDeviceServicesMap[deviceServicesIter->first] = serviceList;
	}

	if (mRemoteDeviceServicesMap.size())
		callback(BLUETOOTH_ERROR_NONE);
	else
		callback(BLUETOOTH_ERROR_FAIL);
}

void Bluez5ProfileGatt::writeDescriptor(const std::string &address, const BluetoothUuid &service, const BluetoothUuid &characteristic,
						const BluetoothGattDescriptor &descriptor, BluetoothResultCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	GattRemoteService* remoteService = findService(address, service);
	if (!remoteService)
	{
		callback(BLUETOOTH_ERROR_FAIL);
		return;
	}
	GattRemoteCharacteristic* remoteChar = findCharacteristic(remoteService, characteristic);
	if (remoteChar && remoteChar->characteristic.isPropertySet(BluetoothGattCharacteristic::Property::PROPERTY_WRITE))
	{
		GattRemoteDescriptor* remoteDesc = findDescriptor(remoteChar, descriptor.getUuid());
		if (remoteDesc && remoteDesc->writeValue(descriptor.getValue()))
		{
			remoteChar->characteristic.updateDescriptorValue(descriptor.getUuid(), descriptor.getValue());
			remoteService->service.updateDescriptorValue(remoteChar->characteristic.getUuid(),
														 descriptor.getUuid(),
														 descriptor.getValue());
			updateRemoteDeviceServices();
			callback(BLUETOOTH_ERROR_NONE);
			return;
		}
	}

	callback(BLUETOOTH_ERROR_FAIL);
}

BluetoothGattService Bluez5ProfileGatt::getService(const std::string &address, const BluetoothUuid &uuid)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	std::string lowerCaseAddress = convertAddressToLowerCase(address);

	auto deviceIter = mRemoteDeviceServicesMap.find(lowerCaseAddress);
	if (deviceIter == mRemoteDeviceServicesMap.end())
		return BluetoothGattService();

	for (auto service : deviceIter->second)
	{
		if (service.getUuid() == uuid)
			return service;
	}

	return BluetoothGattService();
}

BluetoothGattServiceList Bluez5ProfileGatt::getServices(const std::string &address)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	std::string lowerCaseAddress = convertAddressToLowerCase(address);

	auto deviceIter = mRemoteDeviceServicesMap.find(lowerCaseAddress);
	if (deviceIter == mRemoteDeviceServicesMap.end())
		return BluetoothGattServiceList();

	return deviceIter->second;
}

void Bluez5ProfileGatt::readCharacteristic(const uint16_t &connId, const BluetoothUuid& service,
									 const BluetoothUuid &characteristics,
									 BluetoothGattReadCharacteristicCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	BluetoothGattCharacteristic readCharacteristicValue;
	std::string deviceAddress = getAddress(connId);
	if (deviceAddress.empty())
	{
		callback(BLUETOOTH_ERROR_FAIL, readCharacteristicValue);
		return;
	}

	readCharacteristicValue = readCharacteristic(deviceAddress, service, characteristics);
	if (readCharacteristicValue.isValid())
		callback(BLUETOOTH_ERROR_NONE, readCharacteristicValue);
	else
		callback(BLUETOOTH_ERROR_FAIL, readCharacteristicValue);
}

void Bluez5ProfileGatt::readCharacteristics(const uint16_t &connId, const BluetoothUuid& service,
									 const BluetoothUuidList &characteristics,
									 BluetoothGattReadCharacteristicsCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	BluetoothGattCharacteristicList serviceCharacteristicList;
	bool found = false;

	std::string deviceAddress = getAddress(connId);
	if (deviceAddress.empty())
	{
		callback(BLUETOOTH_ERROR_FAIL, serviceCharacteristicList);
		return;
	}
	serviceCharacteristicList = readCharacteristics(deviceAddress, service, characteristics, found);
	if (found)
		callback(BLUETOOTH_ERROR_NONE, serviceCharacteristicList);
	else
		callback(BLUETOOTH_ERROR_FAIL, serviceCharacteristicList);

}

void Bluez5ProfileGatt::writeCharacteristic(const uint16_t &connId, const BluetoothUuid& service,
									 const BluetoothGattCharacteristic &characteristic,
									 BluetoothResultCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	std::string deviceAddress = getAddress(connId);
	if (deviceAddress.empty())
	{
		callback(BLUETOOTH_ERROR_FAIL);
		return;
	}

	GattRemoteService* remoteService = findService(deviceAddress, service);
	if (!remoteService)
	{
		callback(BLUETOOTH_ERROR_FAIL);
		return;
	}
	GattRemoteCharacteristic* remoteChar = findCharacteristic(remoteService, characteristic.getUuid());
	if (remoteChar && remoteChar->characteristic.isPropertySet(BluetoothGattCharacteristic::Property::PROPERTY_WRITE))
	{
		if (remoteChar->writeValue(characteristic.getValue()))
		{
			remoteService->service.updateCharacteristicValue(characteristic.getUuid(), characteristic.getValue());
			updateRemoteDeviceServices();
			getGattObserver()->characteristicValueChanged(deviceAddress, service, characteristic);
			callback(BLUETOOTH_ERROR_NONE);
			return;
		}
	}

	callback(BLUETOOTH_ERROR_FAIL);
}

void Bluez5ProfileGatt::readDescriptor(const uint16_t &connId, const BluetoothUuid& service, const BluetoothUuid &characteristic,
								 const BluetoothUuid &descriptor, BluetoothGattReadDescriptorCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	BluetoothGattDescriptor readDescriptorValue;

	std::string deviceAddress = getAddress(connId);
	if (deviceAddress.empty())
	{
		callback(BLUETOOTH_ERROR_FAIL, readDescriptorValue);
		return;
	}

	readDescriptorValue = readDescriptor(deviceAddress, service, characteristic,descriptor);
	if (readDescriptorValue.isValid())
		callback(BLUETOOTH_ERROR_NONE, readDescriptorValue);
	else
		callback(BLUETOOTH_ERROR_FAIL, readDescriptorValue);
}

void Bluez5ProfileGatt::readDescriptors(const uint16_t &connId, const BluetoothUuid& service, const BluetoothUuid &characteristic,
								 const BluetoothUuidList &descriptors, BluetoothGattReadDescriptorsCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	BluetoothGattDescriptorList characteristicDescriptorList;
	bool found = false;

	std::string deviceAddress = getAddress(connId);
	if (deviceAddress.empty())
	{
		callback(BLUETOOTH_ERROR_FAIL, characteristicDescriptorList);
		return;
	}
	characteristicDescriptorList = readDescriptors(deviceAddress, service, characteristic, descriptors, found);
	if (found)
		callback(BLUETOOTH_ERROR_NONE, characteristicDescriptorList);
	else
		callback(BLUETOOTH_ERROR_FAIL, characteristicDescriptorList);
}

void Bluez5ProfileGatt::writeDescriptor(const uint16_t &connId, const BluetoothUuid &service, const BluetoothUuid &characteristic,
								 const BluetoothGattDescriptor &descriptor, BluetoothResultCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	std::string deviceAddress = getAddress(connId);
	if (deviceAddress.empty())
	{
		callback(BLUETOOTH_ERROR_FAIL);
		return;
	}

	GattRemoteService* remoteService = findService(deviceAddress, service);
	if (!remoteService)
	{
		callback(BLUETOOTH_ERROR_FAIL);
		return;
	}
	GattRemoteCharacteristic* remoteChar = findCharacteristic(remoteService, characteristic);
	if (remoteChar && remoteChar->characteristic.isPropertySet(BluetoothGattCharacteristic::Property::PROPERTY_WRITE))
	{
		GattRemoteDescriptor* remoteDesc = findDescriptor(remoteChar, descriptor.getUuid());
		if (remoteDesc && remoteDesc->writeValue(descriptor.getValue()))
		{
			remoteChar->characteristic.updateDescriptorValue(descriptor.getUuid(), descriptor.getValue());
			remoteService->service.updateDescriptorValue(remoteChar->characteristic.getUuid(),
														 descriptor.getUuid(),
														 descriptor.getValue());
			updateRemoteDeviceServices();
			callback(BLUETOOTH_ERROR_NONE);
			return;
		}
	}
	callback(BLUETOOTH_ERROR_FAIL);
}

void Bluez5ProfileGatt::changeCharacteristicWatchStatus(const std::string &address, const BluetoothUuid &service,
										const BluetoothUuid &characteristic, bool enabled,
										BluetoothResultCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	bool result = false;
	auto deviceServicesIter = mDeviceServicesMap.find(address);

	if (deviceServicesIter == mDeviceServicesMap.end())
	{
		ERROR(MSGID_GATT_PROFILE_ERROR, 0, "Device is not connected");
		callback(BLUETOOTH_ERROR_FAIL);
		return;
	}

	auto &servicesList = deviceServicesIter->second;
	for (auto serviceIt : servicesList)
	{
		if (serviceIt->service.getUuid() == service)
		{
			for (auto charIt = serviceIt->gattRemoteCharacteristics.begin(); charIt != serviceIt->gattRemoteCharacteristics.end(); charIt++)
			{
				if ((*charIt)->characteristic.getUuid() == characteristic)
				{
					if (enabled)
					{
						result = (*charIt)->startNotify();
					}
					else
					{
						result = (*charIt)->stopNotify();
					}
					break;
				}
			}
		}
	}

	if (result)
		callback(BLUETOOTH_ERROR_NONE);
	else
		callback(BLUETOOTH_ERROR_FAIL);
}

void Bluez5ProfileGatt::readCharacteristic(const std::string &address, const BluetoothUuid& service,
									 const BluetoothUuid &characteristic,
									 BluetoothGattReadCharacteristicCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	BluetoothGattCharacteristic readCharacteristicValue;

	readCharacteristicValue = readCharacteristic(address, service, characteristic);
	if (readCharacteristicValue.isValid())
		callback(BLUETOOTH_ERROR_NONE, readCharacteristicValue);
	else
		callback(BLUETOOTH_ERROR_FAIL, readCharacteristicValue);
}

void Bluez5ProfileGatt::readCharacteristics(const std::string &address, const BluetoothUuid& service,
									const BluetoothUuidList &characteristics,
									BluetoothGattReadCharacteristicsCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	BluetoothGattCharacteristicList serviceCharacteristicList;
	bool found = false;
	serviceCharacteristicList = readCharacteristics(address, service, characteristics, found);
	if (found)
		callback(BLUETOOTH_ERROR_NONE, serviceCharacteristicList);
	else
		callback(BLUETOOTH_ERROR_FAIL, serviceCharacteristicList);
}

void Bluez5ProfileGatt::writeCharacteristic(const std::string &address, const BluetoothUuid& service,
							const BluetoothGattCharacteristic &characteristic,
							BluetoothResultCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	GattRemoteService* remoteService = findService(address, service);
	if (!remoteService)
	{
		callback(BLUETOOTH_ERROR_FAIL);
		return;
	}
	GattRemoteCharacteristic* remoteChar = findCharacteristic(remoteService, characteristic.getUuid());
	if (remoteChar && remoteChar->characteristic.isPropertySet(BluetoothGattCharacteristic::Property::PROPERTY_WRITE))
	{
		if (remoteChar->writeValue(characteristic.getValue()))
		{
			remoteService->service.updateCharacteristicValue(characteristic.getUuid(), characteristic.getValue());
			updateRemoteDeviceServices();
			getGattObserver()->characteristicValueChanged(address, service, characteristic);
			callback(BLUETOOTH_ERROR_NONE);
			return;
		}
	}
	callback(BLUETOOTH_ERROR_FAIL);
}

void Bluez5ProfileGatt::readDescriptor(const std::string &address, const BluetoothUuid& service, const BluetoothUuid &characteristic,
								 const BluetoothUuid &descriptor, BluetoothGattReadDescriptorCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	BluetoothGattDescriptor readDescriptorValue;

	readDescriptorValue = readDescriptor(address, service, characteristic, descriptor);
	if (readDescriptorValue.isValid())
		callback(BLUETOOTH_ERROR_NONE, readDescriptorValue);
	else
		callback(BLUETOOTH_ERROR_FAIL, readDescriptorValue);
}

void Bluez5ProfileGatt::readDescriptors(const std::string &address, const BluetoothUuid& service, const BluetoothUuid &characteristic,
						const BluetoothUuidList &descriptors, BluetoothGattReadDescriptorsCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	BluetoothGattDescriptorList characteristicDescriptorList;
	bool found = false;

	characteristicDescriptorList = readDescriptors(address, service, characteristic, descriptors, found);
	if (found)
		callback(BLUETOOTH_ERROR_NONE, characteristicDescriptorList);
	else
		callback(BLUETOOTH_ERROR_FAIL, characteristicDescriptorList);
}

uint16_t Bluez5ProfileGatt::getConnectId(const std::string &address)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	std::string lowerCaseAddress = convertAddressToLowerCase(address);

	for ( auto it = mConnectedDevices.begin(); it != mConnectedDevices.end(); ++it )
	{
		if (lowerCaseAddress == it->second)
		{
			return it->first;
		}
	}
	return 0;
}

std::string Bluez5ProfileGatt::getAddress(const uint16_t &connId)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	std::string deviceAddress;
	if (mConnectedDevices.find(connId) == mConnectedDevices.end())
	{
		ERROR(MSGID_GATT_PROFILE_ERROR, 0, "Device not connected");
	}
	else
	{
		deviceAddress = mConnectedDevices.find(connId)->second;
	}
	return deviceAddress;
}

BluetoothGattCharacteristic Bluez5ProfileGatt::readCharacteristic(const std::string &address, const BluetoothUuid& service,
									 const BluetoothUuid &characteristic)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	BluetoothGattCharacteristic readCharacteristicValue;

	GattRemoteService* remoteService = findService(address, service);
	if (!remoteService)
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "remote GATT service object is null");
		return readCharacteristicValue;
	}
	GattRemoteCharacteristic* remoteChar = findCharacteristic(remoteService, characteristic);
	if (remoteChar && remoteChar->characteristic.isPropertySet(BluetoothGattCharacteristic::Property::PROPERTY_READ))
		readCharacteristicValue = readCharValue(remoteService, remoteChar, characteristic);
	return readCharacteristicValue;
}

BluetoothGattCharacteristicList Bluez5ProfileGatt::readCharacteristics(const std::string &address, const BluetoothUuid& service,
									const BluetoothUuidList &characteristics, bool &found)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	BluetoothGattCharacteristicList serviceCharacteristicList;
	BluetoothGattCharacteristicList result;
	GattRemoteService* remoteService = findService(address, service);
	if (!remoteService)
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "remote GATT service object is null");
		return result;
	}

	serviceCharacteristicList =  remoteService->service.getCharacteristics();

	for (auto &currentCharacteristic : characteristics)
	{
		found = false;
		for (auto characteristic : serviceCharacteristicList)
		{
			if (characteristic.getUuid() == currentCharacteristic)
			{
				GattRemoteCharacteristic* remoteChar = findCharacteristic(remoteService, currentCharacteristic);
				if (remoteChar && remoteChar->characteristic.isPropertySet(BluetoothGattCharacteristic::Property::PROPERTY_READ))
				{
					BluetoothGattCharacteristic readCharacteristicValue;
					readCharacteristicValue = readCharValue(remoteService, remoteChar, currentCharacteristic);
					result.push_back(readCharacteristicValue);
					found = true;
				}
			}
		}

		if (!found)
		{
			ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Characteristic not found");
			return result;
		}
	}
	return result;
}

BluetoothGattDescriptor Bluez5ProfileGatt::readDescriptor(const std::string &address, const BluetoothUuid& service, const BluetoothUuid &characteristic,
								 const BluetoothUuid &descriptor)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	BluetoothGattDescriptor readDescriptorValue;
	GattRemoteService* remoteService = findService(address, service);
	if (!remoteService)
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "remote GATT service object is null");
		return readDescriptorValue;
	}
	GattRemoteCharacteristic* remoteChar = findCharacteristic(remoteService, characteristic);
	if (remoteChar && remoteChar->characteristic.isPropertySet(BluetoothGattCharacteristic::Property::PROPERTY_READ))
	{
		GattRemoteDescriptor* remoteDesc = findDescriptor(remoteChar, descriptor);
		if (!remoteDesc)
		{
			ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Descriptor not found");
			return readDescriptorValue;
		}
		readDescriptorValue = readDescValue(remoteService, remoteChar, remoteDesc, descriptor);
	}
	else
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Read property not available");
	return readDescriptorValue;
}

BluetoothGattDescriptorList Bluez5ProfileGatt::readDescriptors(const std::string &address, const BluetoothUuid& service, const BluetoothUuid &characteristic,
						const BluetoothUuidList &descriptors, bool &found)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	BluetoothGattDescriptorList descriptorList;
	BluetoothGattDescriptorList result;
	GattRemoteService* remoteService = findService(address, service);
	if (!remoteService)
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "remote GATT service object is null");
		return result;
	}

	GattRemoteCharacteristic* remoteChar = findCharacteristic(remoteService, characteristic);
	if (remoteChar && remoteChar->characteristic.isPropertySet(BluetoothGattCharacteristic::Property::PROPERTY_READ))
	{
		descriptorList = remoteChar->characteristic.getDescriptors();

		for (auto &currentDescriptor : descriptors)
		{
			found = false;

			for (auto descriptor : descriptorList)
			{
				if (descriptor.getUuid() == currentDescriptor)
				{
					GattRemoteDescriptor* remoteDesc = findDescriptor(remoteChar, currentDescriptor);
					if (remoteDesc)
					{
						BluetoothGattDescriptor readDescriptorValue;
						readDescriptorValue = readDescValue(remoteService, remoteChar, remoteDesc, currentDescriptor);
						result.push_back(readDescriptorValue);
						found = true;
					}
				}
			}

			if (!found)
			{
				ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Descriptor not found");
				return result;
			}
		}
		return result;
	}
	else
	{
				ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Read property not available");
				return result;
	}
}

GattRemoteService* Bluez5ProfileGatt::findService(const std::string &address, const BluetoothUuid& service)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	auto deviceServicesIter = mDeviceServicesMap.find(address);

	if (deviceServicesIter == mDeviceServicesMap.end())
	{
		ERROR(MSGID_GATT_PROFILE_ERROR, 0, "Device not connected");
		return NULL;
	}

	auto &servicesList = deviceServicesIter->second;
	for (auto serviceIt : servicesList)
	{
		if (serviceIt->service.getUuid() == service)
		{
			return serviceIt;
		}
	}
	return NULL;
}

GattRemoteCharacteristic* Bluez5ProfileGatt::findCharacteristic(GattRemoteService* service, const BluetoothUuid &characteristic)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	for (auto charIt = service->gattRemoteCharacteristics.begin(); charIt != service->gattRemoteCharacteristics.end(); charIt++)
	{
		if ((*charIt)->characteristic.getUuid() == characteristic)
		{
			return (*charIt);
		}
	}
	return NULL;
}

GattRemoteDescriptor* Bluez5ProfileGatt::findDescriptor(GattRemoteCharacteristic* characteristic, const BluetoothUuid &descriptor)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	for (auto descIt = characteristic->gattRemoteDescriptors.begin(); descIt != characteristic->gattRemoteDescriptors.end(); descIt++)
	{
		if ((*descIt)->descriptor.getUuid() == descriptor)
		{
			return (*descIt);
		}
	}
	return NULL;
}

BluetoothGattDescriptor Bluez5ProfileGatt::readDescValue(GattRemoteService *remoteService, GattRemoteCharacteristic* remoteCharacteristic, GattRemoteDescriptor* remoteDescriptor, const BluetoothUuid &descriptor)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	BluetoothGattDescriptor readDescriptorValue;
	BluetoothGattValue descValue = remoteDescriptor->readValue();
	readDescriptorValue.setUuid(descriptor);
	readDescriptorValue.setValue(descValue);
	remoteCharacteristic->characteristic.updateDescriptorValue(descriptor, descValue);
	remoteService->service.updateDescriptorValue(remoteCharacteristic->characteristic.getUuid(),
												 descriptor,
												 descValue);
	updateRemoteDeviceServices();
	return readDescriptorValue;
}

BluetoothGattCharacteristic Bluez5ProfileGatt::readCharValue(GattRemoteService* remoteService, GattRemoteCharacteristic* remoteCharacteristic, const BluetoothUuid &characteristic)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	BluetoothGattCharacteristic readCharacteristicValue;
	readCharacteristicValue.setProperties(remoteCharacteristic->readProperties());
	BluetoothGattValue charValue = remoteCharacteristic->readValue();
	readCharacteristicValue.setUuid(characteristic);
	readCharacteristicValue.setValue(charValue);
	remoteService->service.updateCharacteristicValue(characteristic, charValue);
	updateRemoteDeviceServices();
	return readCharacteristicValue;
}

void Bluez5ProfileGatt::addService(uint16_t appId, const BluetoothGattService &service, BluetoothGattAddCallback callback)
{
	if (!mObjectManagerGattServer)
	{
		callback(BLUETOOTH_ERROR_FAIL, -1);
		return;
	}

	BluetoothUuid uuid = service.getUuid();
	auto serviceId = nextServiceId();

	bool isPrimary = (service.getType() == BluetoothGattService::PRIMARY) ? true : false;

	std::string objPath = g_dbus_object_manager_get_object_path(G_DBUS_OBJECT_MANAGER(mObjectManagerGattServer));
	std::string serviceObjPath = objPath  + "/App" + std::to_string(appId) + "/Service" + std::to_string(serviceId);

	std::vector<BluetoothUuid> BluetoothUuidList = service.getIncludedServices();
	const char *uuidArray[BluetoothUuidList.size()+1];

	int index = 0;
	for (auto it : BluetoothUuidList)
		uuidArray[index++] = it.toString().c_str();

	uuidArray[index] = NULL;

	BluezObjectSkeleton *object = bluez_object_skeleton_new(serviceObjPath.c_str());

	BluezGattService1 *skeletonGattService = bluez_gatt_service1_skeleton_new();
	bluez_gatt_service1_set_uuid(skeletonGattService, uuid.toString().c_str());
	bluez_gatt_service1_set_primary(skeletonGattService, isPrimary);
	bluez_gatt_service1_set_includes(skeletonGattService, uuidArray);
	bluez_object_skeleton_set_gatt_service1(object, skeletonGattService);
	g_dbus_object_manager_server_export(mObjectManagerGattServer, G_DBUS_OBJECT_SKELETON (object));
	g_dbus_object_manager_server_set_connection (mObjectManagerGattServer, mConn);

	auto registerCallback = [this, callback, object, skeletonGattService, appId, serviceId](BluetoothError error)
	{
		if (error == BLUETOOTH_ERROR_NONE)
		{
			DEBUG("Register application successfully");
			auto appIt = mGattLocalApplications.find(appId);

			if (appIt == mGattLocalApplications.end())
			{
				ERROR("MSGID_GATT_PROFILE_ERROR", 0, "application not present list");
				return;
			}

			std::unique_ptr <Bluez5GattLocalService> service(new Bluez5GattLocalService(G_DBUS_OBJECT(object)));
			service->mServiceInterface = skeletonGattService;
			auto &services = ((appIt->second).get())->mGattLocalServices;
			services.insert(make_pair(serviceId, std::move(service)));
			callback(BLUETOOTH_ERROR_NONE, serviceId);
			return;
		}
		else
		{
			ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Register application failed %d", error);
			callback(BLUETOOTH_ERROR_FAIL, -1);
		}
	};

	registerLocalApplication(registerCallback, objPath, true);
}

void Bluez5ProfileGatt::removeService(uint16_t appId, uint16_t serviceId, BluetoothResultCallback callback)
{
	if (!mObjectManagerGattServer)
	{
		callback(BLUETOOTH_ERROR_FAIL);
		return;
	}

	std::string objPath = g_dbus_object_manager_get_object_path(G_DBUS_OBJECT_MANAGER(mObjectManagerGattServer));
	auto it = mGattLocalApplications.find(appId);

	if (it == mGattLocalApplications.end())
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "appId not present");
		callback(BLUETOOTH_ERROR_FAIL);
		return;
	}

	auto &services = it->second->mGattLocalServices;

	auto serviceIt = services.find(serviceId);
	if (serviceIt == services.end())
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "service not present");
		callback(BLUETOOTH_ERROR_FAIL);
		return;
	}

	removeLocalServices(serviceIt->second.get());
	services.erase(serviceId);

	auto registerCallback = [this, callback](BluetoothError error)
	{
		if (error == BLUETOOTH_ERROR_NONE)
		{
			DEBUG("Removed service successfully");
		}
		else
		{
			ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Removed service failed %d", error);
		}
		callback(error);
		return;
	};

	registerLocalApplication(registerCallback, objPath, true);
}

void Bluez5ProfileGatt::removeLocalServices(Bluez5GattLocalService *service)
{
	if (service->mServiceObject)
	{
		removeLocalCharacteristics(service);
		g_dbus_object_manager_server_unexport (mObjectManagerGattServer, g_dbus_object_get_object_path(service->mServiceObject));

		if (service->mServiceInterface)
		{
			g_object_unref(service->mServiceInterface);
			service->mServiceInterface = 0;
		}

		g_object_unref(service->mServiceObject);
		service->mServiceObject = 0;
	}
}

void Bluez5ProfileGatt::createObjectManagers()
{
	std::string objectPath = mAdapter->getObjectPath() + SERVER_PATH;
	mObjectManagerGattServer = g_dbus_object_manager_server_new(objectPath.c_str());

	if (!mObjectManagerGattServer)
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Failed to create Object Manager for GATT Server");
	}
}

void Bluez5ProfileGatt::addCharacteristic(uint16_t appId, uint16_t serviceId, const BluetoothGattCharacteristic &characteristic, BluetoothGattAddCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	auto appIt = mGattLocalApplications.find(appId);
	if (appIt == mGattLocalApplications.end())
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Application not present for addCharacteristic");
		callback(BLUETOOTH_ERROR_PARAM_INVALID, -1);
		return;
	}

	auto &services = appIt->second->mGattLocalServices;

	auto srvIt = services.find(serviceId);

	if (srvIt == services.end())
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Service is not present list for addCharacteristic");
		callback(BLUETOOTH_ERROR_PARAM_INVALID, -1);
		return;
	}

	auto &chars = srvIt->second->mCharacteristics;

	BluezGattCharacteristic1 *skeletonGattChar = bluez_gatt_characteristic1_skeleton_new();
	if (!skeletonGattChar)
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Failed to allocate memroy for gatt characteristic interface");
		callback(BLUETOOTH_ERROR_NOMEM, -1);
		return;
	}

	uint16_t charId = nextCharId();

	std::string objPath = g_dbus_object_manager_get_object_path(G_DBUS_OBJECT_MANAGER(mObjectManagerGattServer));
	std::string serviceObjPath = objPath  + "/App" + std::to_string(appId) + "/Service" + std::to_string(serviceId);
	std::string charObjPath = serviceObjPath + "/Char"+ std::to_string(charId);

	BluezObjectSkeleton *object = bluez_object_skeleton_new(charObjPath.c_str());

	if (!object)
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Memory allocation failed %d", __LINE__);
		if (skeletonGattChar) g_object_unref(skeletonGattChar);
		callback(BLUETOOTH_ERROR_NOMEM, -1);
		return;
	}

	bluez_gatt_characteristic1_set_service(skeletonGattChar, serviceObjPath.c_str());
	bluez_gatt_characteristic1_set_uuid(skeletonGattChar, characteristic.getUuid().toString().c_str());

	const char *flags[GattRemoteCharacteristic::characteristicPropertyMap.size() + 1];
	updatePropertyFlags(characteristic, flags);

	BluetoothGattValue value = characteristic.getValue();

	GVariantBuilder *dataBuilder = g_variant_builder_new (G_VARIANT_TYPE ("ay"));

	if (!dataBuilder)
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Memory allocation failed %d", __LINE__);
		if (object) g_object_unref(object);
		if (skeletonGattChar) g_object_unref(skeletonGattChar);
		callback(BLUETOOTH_ERROR_NOMEM, -1);
		return;
	}

	for (auto &byteIt : value)
		g_variant_builder_add(dataBuilder, "y", byteIt);

	GVariant *dataValue = g_variant_builder_end(dataBuilder);
	g_variant_builder_unref(dataBuilder);

	bluez_gatt_characteristic1_set_value(skeletonGattChar, dataValue);

	GVariantBuilder *builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
	if (!builder)
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Memory allocation failed %d", __LINE__);
		if (object) g_object_unref(object);
		if (skeletonGattChar) g_object_unref(skeletonGattChar);
		callback(BLUETOOTH_ERROR_NOMEM, -1);
		return;
	}

	for (int i = 0; flags[i] != NULL; i++)
	{
		g_variant_builder_add(builder, "s", flags[i]);
	}

	GVariant *flagsVariant = g_variant_builder_end(builder);
	g_variant_builder_unref(builder);

	bluez_gatt_characteristic1_set_flags(skeletonGattChar, flagsVariant);

	bluez_object_skeleton_set_gatt_characteristic1(object, skeletonGattChar);

	g_signal_connect(skeletonGattChar,
		"handle_read_value",
		G_CALLBACK (Bluez5GattLocalCharacteristic::onHandleReadValue),
		this);

	g_signal_connect(skeletonGattChar,
		"handle_write_value",
		G_CALLBACK (Bluez5GattLocalCharacteristic::onHandleWriteValue),
		this);

	g_signal_connect(skeletonGattChar,
		"handle_start_notify",
		G_CALLBACK (Bluez5GattLocalCharacteristic::onHandleStartNotify),
		this);

	g_signal_connect(skeletonGattChar,
		"handle_stop_notify",
		G_CALLBACK (Bluez5GattLocalCharacteristic::onHandleStopNotify),
		this);

	g_dbus_object_manager_server_export(mObjectManagerGattServer, G_DBUS_OBJECT_SKELETON (object));
	g_dbus_object_manager_server_set_connection (mObjectManagerGattServer, mConn);

	auto registerCallback = [this, callback, charId, object, &chars, skeletonGattChar](BluetoothError error)
	{
		if (error == BLUETOOTH_ERROR_NONE)
		{
			DEBUG("Characterstic registered successfully");
			std::unique_ptr<Bluez5GattLocalCharacteristic> character(new Bluez5GattLocalCharacteristic(G_DBUS_OBJECT(object)));
			character->mInterface = skeletonGattChar;
			chars.insert(std::make_pair(charId, std::move(character)));
			mLastCharId = charId;
			callback(BLUETOOTH_ERROR_NONE, charId);
		}
		else
		{
			ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Removed application  and register application failed %d", error);
			g_object_unref(object);
			g_object_unref(skeletonGattChar);
			callback(BLUETOOTH_ERROR_FAIL, -1);
		}
		return;
	};

	registerLocalApplication(registerCallback, objPath, true);
}

void Bluez5ProfileGatt::removeLocalCharacteristics(Bluez5GattLocalService *service)
{
	if (!service)
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "removeCharacteristics service object is null");
		return;
	}

	auto &chars = service->mCharacteristics;

	for (auto &it : chars)
	{
		removeLocalDescriptors(it.second.get());

		if (it.second->mCharObject)
		{
			g_dbus_object_manager_server_unexport (mObjectManagerGattServer, g_dbus_object_get_object_path(it.second->mCharObject));

			if (it.second->mInterface)
			{
				g_object_unref(it.second->mInterface);
				it.second->mInterface = 0;
			}

			if (it.second->mCharObject)
			{
				g_object_unref(it.second->mCharObject);
				it.second->mCharObject = 0;
			}
		}

	}
	chars.clear();
}

void Bluez5ProfileGatt::addDescriptor(uint16_t appId, uint16_t serviceId, const BluetoothGattDescriptor &descriptor, BluetoothGattAddCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	auto descs = getLocalDescriptorList(appId, serviceId, mLastCharId);

	if (!descs)
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Failed to get desc list");
		callback(BLUETOOTH_ERROR_PARAM_INVALID, -1);
	}

	BluezGattDescriptor1 *skeletonGattDesc = bluez_gatt_descriptor1_skeleton_new();
	if (!skeletonGattDesc)
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Failed to allocate memroy for gatt descriptor interface");
		callback(BLUETOOTH_ERROR_NOMEM, -1);
		return;
	}

	uint16_t descId = nextDescId();
	std::string objPath = g_dbus_object_manager_get_object_path(G_DBUS_OBJECT_MANAGER(mObjectManagerGattServer));
	std::string serviceObjPath = objPath  + "/App" + std::to_string(appId) + "/Service" + std::to_string(serviceId);
	std::string charObjPath = serviceObjPath + "/Char"+ std::to_string(mLastCharId);
	std::string descObjPath = charObjPath + "/Desc" + std::to_string(descId);

	BluezObjectSkeleton *object = bluez_object_skeleton_new(descObjPath.c_str());
	if (!object)
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Memory allocation failed %d", __LINE__);
		if (skeletonGattDesc) g_object_unref(skeletonGattDesc);
		callback(BLUETOOTH_ERROR_FAIL, -1);
		return;
	}

	BluetoothGattValue value = descriptor.getValue();

	const char *flags[GattRemoteDescriptor::descriptorPermissionMap.size() + 1];

	updatePermissionFlags(descriptor, flags);

	GVariantBuilder *dataBuilder = g_variant_builder_new (G_VARIANT_TYPE ("ay"));
	for (auto &byteIt : value)
		g_variant_builder_add(dataBuilder, "y", byteIt);

	GVariant *dataValue = g_variant_builder_end(dataBuilder);
	g_variant_builder_unref(dataBuilder);

	bluez_gatt_descriptor1_set_value(skeletonGattDesc, dataValue);

	bluez_gatt_descriptor1_set_characteristic(skeletonGattDesc, charObjPath.c_str());
	bluez_gatt_descriptor1_set_uuid(skeletonGattDesc, descriptor.getUuid().toString().c_str());

	GVariantBuilder *builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
	if (!builder)
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Memory allocation failed %d", __LINE__);
		if (object) g_object_unref(object);
		if (skeletonGattDesc) g_object_unref(skeletonGattDesc);
		callback(BLUETOOTH_ERROR_NOMEM, -1);
		return;
	}

	for (int i = 0; flags[i] != NULL; i++)
	{
		g_variant_builder_add(builder, "s", flags[i]);
	}

	GVariant *flagsVariant = g_variant_builder_end(builder);
	g_variant_builder_unref(builder);

	bluez_gatt_descriptor1_set_flags(skeletonGattDesc, flagsVariant);

	bluez_object_skeleton_set_gatt_descriptor1(object, skeletonGattDesc);

	g_signal_connect(skeletonGattDesc,
		"handle_read_value",
		G_CALLBACK (Bluez5GattLocalDescriptor::onHandleReadValue),
		this);

	g_signal_connect(skeletonGattDesc,
		"handle_write_value",
		G_CALLBACK (Bluez5GattLocalDescriptor::onHandleWriteValue),
		this);

	g_dbus_object_manager_server_export(mObjectManagerGattServer, G_DBUS_OBJECT_SKELETON (object));
	g_dbus_object_manager_server_set_connection (mObjectManagerGattServer, mConn);

	auto registerCallback = [this, callback, descId, object, descs, skeletonGattDesc](BluetoothError error)
	{
		if (error == BLUETOOTH_ERROR_NONE)
		{
			DEBUG("Descriptor registered successfully");
			std::unique_ptr<Bluez5GattLocalDescriptor> desc(new Bluez5GattLocalDescriptor(G_DBUS_OBJECT(object)));
			desc->mInterface = skeletonGattDesc;
			descs->insert(std::make_pair(descId, std::move(desc)));
			callback(BLUETOOTH_ERROR_NONE, descId);
		}
		else
		{
			ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Descriptor register failed %d", error);
			if (object) g_object_unref(object);
			if (skeletonGattDesc) g_object_unref(skeletonGattDesc);
			callback(BLUETOOTH_ERROR_FAIL, -1);
		}
		return;
	};

	registerLocalApplication(registerCallback, objPath, true);
}

void Bluez5ProfileGatt::removeLocalDescriptors(Bluez5GattLocalCharacteristic *characteristic)
{
	if (!characteristic)
		return;

	auto &descs = characteristic->mDescriptors;

	for (auto &it: descs)
	{
		if (it.second->mDescObject)
			g_dbus_object_manager_server_unexport (mObjectManagerGattServer, g_dbus_object_get_object_path(it.second->mDescObject));
		else
			ERROR("MSGID_GATT_PROFILE_ERROR", 0, "removeDescriptors trying remove null object");

		if (it.second->mInterface)
		{
			g_object_unref(it.second->mInterface);
			it.second->mInterface = 0;
		}

		if (it.second->mDescObject)
		{
			g_object_unref(it.second->mDescObject);
			it.second->mDescObject = 0;
		}
	}

	descs.clear();
}

void Bluez5ProfileGatt::notifyCharacteristicValueChanged(uint16_t appId, uint16_t serviceId, BluetoothGattCharacteristic characteristic, uint16_t charId)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	auto appIt = mGattLocalApplications.find(appId);
	if (appIt == mGattLocalApplications.end())
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Application not present for notifyCharacteristicValueChanged");
		return;
	}

	auto &services = appIt->second->mGattLocalServices;

	auto srvIt = services.find(serviceId);

	if (srvIt == services.end())
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Service is not present list for notifyCharacteristicValueChanged");
		return;
	}

	auto &chars = srvIt->second->mCharacteristics;

	auto charIt = chars.find(charId);

	if (charIt == chars.end())
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Characteristic is not present list for notifyCharacteristicValueChanged");
		return;
	}

	GVariant *characteristicVariant= convertVectorToArrayByteGVariant(characteristic.getValue());
	bluez_gatt_characteristic1_set_value(charIt->second->mInterface, characteristicVariant);
	return;
}

void Bluez5ProfileGatt::notifyDescriptorValueChanged(uint16_t appId, uint16_t serviceId, uint16_t descId, BluetoothGattDescriptor descriptor, uint16_t charId)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	auto appIt = mGattLocalApplications.find(appId);
	if (appIt == mGattLocalApplications.end())
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Application not present for notifyDescriptorValueChanged");
		return;
	}

	auto &services = appIt->second->mGattLocalServices;

	auto srvIt = services.find(serviceId);

	if (srvIt == services.end())
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Service is not present list for notifyDescriptorValueChanged");
		return;
	}

	auto &chars = srvIt->second->mCharacteristics;

	auto charIt = chars.find(charId);

	if (charIt == chars.end())
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Characteristic is not present list for notifyDescriptorValueChanged");
		return;
	}

	auto &descs = charIt->second->mDescriptors;

	auto descIt = descs.find(descId);

	if (descIt == descs.end())
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Descriptor is not present list for notifyDescriptorValueChanged");
		return;
	}

	GVariant *descVariant = convertVectorToArrayByteGVariant(descriptor.getValue());
	bluez_gatt_descriptor1_set_value(descIt->second->mInterface, descVariant);
	return;
}

void Bluez5ProfileGatt::startService(uint16_t serviceId, BluetoothGattTransportMode mode, BluetoothResultCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	if (callback) callback(BLUETOOTH_ERROR_NONE);
}

void Bluez5ProfileGatt::startService(uint16_t appId, uint16_t serviceId, BluetoothGattTransportMode mode, BluetoothResultCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	if (callback) callback(BLUETOOTH_ERROR_NONE);
}

Bluez5ProfileGatt::GattLocalDescriptorsMap* Bluez5ProfileGatt::getLocalDescriptorList(uint16_t appId, uint16_t serviceId, uint16_t charId)
{
	auto appIt = mGattLocalApplications.find(appId);
	if (appIt == mGattLocalApplications.end())
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Application not present %d", __LINE__);
		return nullptr;
	}

	auto &services = appIt->second->mGattLocalServices;

	auto srvIt = services.find(serviceId);

	if (srvIt == services.end())
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Service is not present in list %d", __LINE__);
		return nullptr;
	}

	auto &chars = srvIt->second->mCharacteristics;

	auto charIt = chars.find(charId);
	if (charIt == chars.end())
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Characteristic is not present list %d",__LINE__);
		return nullptr;
	}

	return &charIt->second->mDescriptors;
}

void Bluez5ProfileGatt::updatePropertyFlags(const BluetoothGattCharacteristic &characteristic, const char **propertyflags)
{
	if (!propertyflags)
		return;

	int index = 0;

	for (auto &propIt : GattRemoteCharacteristic::characteristicPropertyMap)
	{
		if(characteristic.isPropertySet(propIt.second))
		{
			propertyflags[index++] = propIt.first.c_str();
		}
	}

	propertyflags[index] = NULL;
}

void Bluez5ProfileGatt::updatePermissionFlags(const BluetoothGattDescriptor &descriptor, const char **permissionflags)
{
	if (!permissionflags)
		return;

	int index = 0;

	for (auto &propIt : GattRemoteDescriptor::descriptorPermissionMap)
	{
		if(descriptor.isPermissionSet(propIt.first))
		{
			permissionflags[index++] = propIt.second.c_str();
		}
	}

	permissionflags[index] = NULL;
}

void Bluez5ProfileGatt::onCharacteristicPropertiesChanged(GattRemoteCharacteristic* characteristic, GVariant *changed_properties)
{
	std::string deviceObjPath, serviceName;
	splitInPathAndName(characteristic->parentObjectPath, deviceObjPath, serviceName);

	Bluez5Device* device = mAdapter->findDeviceByObjectPath(deviceObjPath);

	if (!device)
	{
		ERROR(MSGID_GATT_PROFILE_ERROR, 0, "onCharacteristicPropertiesChanged device is not present");
		return;
	}

	std::string deviceAddress = device->getAddress();
	std::string lowerCaseAddress = convertAddressToLowerCase(deviceAddress);

	GattRemoteService* service = getRemoteGattService(characteristic->parentObjectPath);

	if (!service)
	{
		ERROR(MSGID_GATT_PROFILE_ERROR, 0, "onCharacteristicPropertiesChanged unable to get service instance for deviceAddress %s",lowerCaseAddress.c_str());
		return;
	}

	const char* uuid = bluez_gatt_service1_get_uuid(service->mInterface);
	BluetoothUuid service_uuid(uuid, BluetoothUuid::UUID128);

	if(g_variant_n_children(changed_properties) > 0)
	{
		GVariantIter *iter = NULL;

		GVariant *value;
		const gchar *key;
		g_variant_get(changed_properties, "a{sv}", &iter);

		while (iter != nullptr && g_variant_iter_loop(iter, "{&sv}", &key, &value))
		{
			if (g_ascii_strncasecmp(key, "value", 5) == 0)
			{
				BluetoothGattValue charValue = convertArrayByteGVariantToVector(value);
				BluetoothGattCharacteristic remoteChar;
				BluetoothUuid charUuid(bluez_gatt_characteristic1_get_uuid(characteristic->mInterface), BluetoothUuid::UUID128);
				remoteChar.setUuid(charUuid);
				remoteChar.setValue(charValue);
				getGattObserver()->characteristicValueChanged(lowerCaseAddress, service_uuid, remoteChar);
			}
		}
		g_variant_iter_free (iter);
	}
}

gboolean Bluez5ProfileGatt::Bluez5GattLocalCharacteristic::onHandleReadValue(BluezGattCharacteristic1* interface,
																			 GDBusMethodInvocation *invocation,
																			 GVariant *arg_options,
																			 gpointer user_data)
{
	GVariant *value = bluez_gatt_characteristic1_get_value(interface);
	GVariant *tuple = g_variant_new_tuple(&value, 1);
	g_dbus_method_invocation_return_value(invocation, tuple);
	return true;
}

gboolean Bluez5ProfileGatt::Bluez5GattLocalCharacteristic::onHandleWriteValue(BluezGattCharacteristic1* interface,
																			  GDBusMethodInvocation *invocation,
																			  GVariant *arg_value,
																			  GVariant *arg_options,
																			  gpointer user_data)
{
	bluez_gatt_characteristic1_set_value(interface, arg_value);
	g_dbus_method_invocation_return_value(invocation, NULL);
	Bluez5ProfileGatt *pThis = static_cast<Bluez5ProfileGatt *> (user_data);
	pThis->onHandleCharacteriscticWriteValue(interface, arg_value);
	return true;
}

void Bluez5ProfileGatt::onHandleCharacteriscticWriteValue(BluezGattCharacteristic1* charInterface, GVariant * charValue)
{
	BluetoothGattCharacteristic characteristic;

	const char* charUuid = bluez_gatt_characteristic1_get_uuid(charInterface);
	if (charUuid)
		characteristic.setUuid(BluetoothUuid(charUuid));

	BluetoothGattCharacteristicProperties properties = 0;
	GVariant* flagVariant = bluez_gatt_characteristic1_get_flags(charInterface);
	if (flagVariant)
	{
		std::vector <std::string> characteristicFlags = convertArrayStringGVariantToVector(flagVariant);

		for (auto it = characteristicFlags.begin(); it != characteristicFlags.end(); it++)
		{
			auto property = GattRemoteCharacteristic::characteristicPropertyMap.find(*it);
			if (property != GattRemoteCharacteristic::characteristicPropertyMap.end())
				properties |= property->second;
		}
	}

	characteristic.setProperties(properties);

	BluetoothGattValue result;

	result = convertArrayByteGVariantToVector(charValue);

	characteristic.setValue(result);

	//Get service uuid for this characteristic
	std::string servicePath(bluez_gatt_characteristic1_get_service(charInterface));
	int serviceId = std::stoi(servicePath.substr(servicePath.size() - 1, servicePath.size()));

	std::string appPath = servicePath.substr(0, servicePath.find_last_of("\\/"));
	int appId = std::stoi(appPath.substr(appPath.size() - 1, appPath.size()));

	auto appIt = mGattLocalApplications.find(appId);

	if (appIt == mGattLocalApplications.end())
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "application not present for handleWriteCharacteristic");
		return;
	}

	auto &services = appIt->second->mGattLocalServices;

	auto srvIt = services.find(serviceId);

	if (srvIt == services.end())
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Service is not present list for handleWriteCharacteristic");
		return;
	}

	const char* uuid = bluez_gatt_service1_get_uuid(srvIt->second->mServiceInterface);
	if (!uuid)
	{
		ERROR(MSGID_GATT_PROFILE_ERROR, 0, "Failed to get Gatt Service uuid on %s",
			  servicePath.c_str());
		return;
	}
	BluetoothUuid service(uuid);

	getGattObserver()->characteristicValueChanged(service, characteristic);
	return;
}

gboolean Bluez5ProfileGatt::Bluez5GattLocalCharacteristic::onHandleStartNotify(BluezGattCharacteristic1 *object,
																			   GDBusMethodInvocation *invocation,
																			   gpointer user_data)
{
	bluez_gatt_characteristic1_set_notifying(object, true);
	g_dbus_method_invocation_return_value(invocation, NULL);
	return true;
}

gboolean Bluez5ProfileGatt::Bluez5GattLocalCharacteristic::onHandleStopNotify(BluezGattCharacteristic1 *object,
																			  GDBusMethodInvocation *invocation,
																			  gpointer user_data)
{
	bluez_gatt_characteristic1_set_notifying(object, false);
	g_dbus_method_invocation_return_value(invocation, NULL);
	return true;
}

gboolean Bluez5ProfileGatt::Bluez5GattLocalDescriptor::onHandleReadValue(BluezGattDescriptor1* interface,
																		 GDBusMethodInvocation *invocation,
																		 GVariant *arg_options,
																		 gpointer user_data)
{
	GVariant *value = bluez_gatt_descriptor1_get_value(interface);
	GVariant *tuple = g_variant_new_tuple(&value, 1);
	g_dbus_method_invocation_return_value(invocation, tuple);
	return true;
}
gboolean Bluez5ProfileGatt::Bluez5GattLocalDescriptor::onHandleWriteValue(BluezGattDescriptor1* interface,
																		  GDBusMethodInvocation *invocation,
																		  GVariant *arg_value,
																		  GVariant *arg_options,
																		  gpointer user_data)
{
	bluez_gatt_descriptor1_set_value(interface, arg_value);
	g_dbus_method_invocation_return_value(invocation, NULL);
	Bluez5ProfileGatt *pThis = static_cast<Bluez5ProfileGatt *> (user_data);
	pThis->onHandleDescrptorWriteValue(interface, arg_value);
	return true;
}

void Bluez5ProfileGatt::onHandleDescrptorWriteValue(BluezGattDescriptor1* interface, GVariant * descValue)
{
	const char* descUuid = bluez_gatt_descriptor1_get_uuid(interface);

	std::string charPath(bluez_gatt_descriptor1_get_characteristic(interface));
	int charId = std::stoi(charPath.substr(charPath.size() - 1, charPath.size()));

	std::string servicePath = charPath.substr(0, charPath.find_last_of("\\/"));
	int serviceId = std::stoi(servicePath.substr(servicePath.size() - 1, servicePath.size()));

	std::string appPath = servicePath.substr(0, servicePath.find_last_of("\\/"));
	int appId = std::stoi(appPath.substr(appPath.size() - 1, appPath.size()));
	auto appIt = mGattLocalApplications.find(appId);

	if (appIt == mGattLocalApplications.end())
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "application not present for onHandleDescrptorWriteValue");
		return;
	}

	auto &services = appIt->second->mGattLocalServices;

	auto srvIt = services.find(serviceId);

	if (srvIt == services.end())
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "Service is not present list for onHandleDescrptorWriteValue");
		return;
	}

	const char* serviceUuid = bluez_gatt_service1_get_uuid(srvIt->second->mServiceInterface);
	if (!serviceUuid)
	{
		ERROR(MSGID_GATT_PROFILE_ERROR, 0, "Failed to get Gatt Service uuid on %s",
			  servicePath.c_str());
		return;
	}

	auto &chars = srvIt->second->mCharacteristics;

	auto charIt = chars.find(charId);

	if (charIt == chars.end())
	{
		ERROR("MSGID_GATT_PROFILE_ERROR", 0, "char is not present list for onHandleDescrptorWriteValue");
		return;
	}

	const char* charUuid = bluez_gatt_characteristic1_get_uuid(charIt->second->mInterface);

	BluetoothGattDescriptor desc;
	desc.setUuid(BluetoothUuid(descUuid));
	BluetoothGattValue result = convertArrayByteGVariantToVector(descValue);
	desc.setValue(result);
	getGattObserver()->descriptorValueChanged(BluetoothUuid(serviceUuid), BluetoothUuid(charUuid), desc);
}
