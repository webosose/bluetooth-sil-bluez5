// Copyright (c) 2021 LG Electronics, Inc.
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
#include "utils.h"
#include "bluez5profilemesh.h"
#include "bluez5meshadv.h"
#include <cstdint>
#include <string>
#include <vector>

#define BLUEZ_MESH_NAME "org.bluez.mesh"

#define PRIMARY_ELEMENT_IDX 0x00

#define CONFIG_CLIENT_MODEL_ID 0001
#define GENERIC_ONOFF_CLIENT_MODEL_ID 1001

Bluez5ProfileMesh::Bluez5ProfileMesh(Bluez5Adapter *adapter) : Bluez5ProfileBase(adapter, "")
{
	GError *error = 0;

	g_bus_watch_name(G_BUS_TYPE_SYSTEM, BLUEZ_MESH_NAME, G_BUS_NAME_WATCHER_FLAGS_NONE,
					 handleBluezServiceStarted, handleBluezServiceStopped, this, NULL);

	std::vector<uint32_t> sigModelIds = {CONFIG_CLIENT_MODEL_ID, GENERIC_ONOFF_CLIENT_MODEL_ID};
	std::vector<uint32_t> vendorModelIds;

	mMeshAdv = new Bluez5MeshAdv(this, adapter);
	mMeshAdv->registerElement(PRIMARY_ELEMENT_IDX, sigModelIds, vendorModelIds);
}

Bluez5ProfileMesh::~Bluez5ProfileMesh()
{
	if (mMeshAdv)
	{
		delete mMeshAdv;
		mMeshAdv = nullptr;
	}
}

void Bluez5ProfileMesh::handleBluezServiceStarted(GDBusConnection *conn, const gchar *name,
												  const gchar *nameOwner, gpointer userData)
{
	Bluez5ProfileMesh *mesh = static_cast<Bluez5ProfileMesh *>(userData);

	GError *error = 0;
	mesh->mObjectManager = g_dbus_object_manager_client_new_sync(
		conn, G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
		"org.bluez", "/", NULL, NULL, NULL, NULL, &error);
	if (error)
	{
		ERROR(MSGID_MESH_PROFILE_ERROR, 0,
			  "Failed to create object manager: %s", error->message);
		g_error_free(error);
		return;
	}

	g_signal_connect(mesh->mObjectManager, "object-added", G_CALLBACK(handleObjectAdded), mesh);
	g_signal_connect(mesh->mObjectManager, "object-removed", G_CALLBACK(handleObjectRemoved), mesh);
}

void Bluez5ProfileMesh::handleBluezServiceStopped(GDBusConnection *conn, const gchar *name,
												  gpointer userData)
{
}

void Bluez5ProfileMesh::handleObjectAdded(GDBusObjectManager *objectManager,
										  GDBusObject *object, void *userData)
{
	DEBUG("handleObjectAdded");
	Bluez5ProfileMesh *mesh = static_cast<Bluez5ProfileMesh *>(userData);
	std::string objectPath = g_dbus_object_get_object_path(object);

	auto adapterPath = mesh->mAdapter->getObjectPath();
	if (objectPath.compare(0, adapterPath.length(), adapterPath))
		return;
}

void Bluez5ProfileMesh::handleObjectRemoved(GDBusObjectManager *objectManager,
											GDBusObject *object, void *userData)
{
}

BluetoothError Bluez5ProfileMesh::createNetwork(const std::string &bearer)
{
	if (bearer == "PB-GATT")
	{
		return BLUETOOTH_ERROR_UNSUPPORTED;
	}
	else if (bearer == "PB-ADV")
	{
		return mMeshAdv->createNetwork();
	}

	return BLUETOOTH_ERROR_PARAM_INVALID;
}

BluetoothError Bluez5ProfileMesh::attach(const std::string &bearer, const std::string &token)
{
	if (bearer == "PB-GATT")
	{
		return BLUETOOTH_ERROR_UNSUPPORTED;
	}
	else if (bearer == "PB-ADV")
	{
		return mMeshAdv->attachToken(token);
	}

	return BLUETOOTH_ERROR_PARAM_INVALID;
}

void Bluez5ProfileMesh::getMeshInfo(const std::string &bearer, BleMeshInfoCallback callback)
{
	if (bearer == "PB-GATT")
	{
		//Not supported
	}
	else if (bearer == "PB-ADV")
	{
		mMeshAdv->getMeshInfo(callback);
	}
}

BluetoothError Bluez5ProfileMesh::scanUnprovisionedDevices(const std::string &bearer,
															const uint16_t scanTimeout)
{
	if (bearer == "PB-GATT")
	{
		return BLUETOOTH_ERROR_UNSUPPORTED;
	}
	else if (bearer == "PB-ADV")
	{
		return mMeshAdv->scanUnprovisionedDevices(scanTimeout);
	}

	return BLUETOOTH_ERROR_PARAM_INVALID;
}

BluetoothError Bluez5ProfileMesh::unprovisionedScanCancel(const std::string &bearer)
{
	if (bearer == "PB-GATT")
	{
		return BLUETOOTH_ERROR_UNSUPPORTED;
	}
	else if (bearer == "PB-ADV")
	{
		return mMeshAdv->unprovisionedScanCancel();
	}

	return BLUETOOTH_ERROR_PARAM_INVALID;
}

BluetoothError Bluez5ProfileMesh::provision(const std::string &bearer, const std::string &uuid,
											const uint16_t timeout)
{
	if (bearer == "PB-GATT")
	{
		return BLUETOOTH_ERROR_UNSUPPORTED;
	}
	else if (bearer == "PB-ADV")
	{
		return mMeshAdv->provision(uuid, timeout);
	}

	return BLUETOOTH_ERROR_PARAM_INVALID;
}

BluetoothError Bluez5ProfileMesh::supplyProvisioningNumeric(const std::string &bearer,
															uint32_t number)
{
	if (bearer == "PB-GATT")
	{
		return BLUETOOTH_ERROR_UNSUPPORTED;
	}
	else if (bearer == "PB-ADV")
	{
		return mMeshAdv->supplyNumeric(number);
	}

	return BLUETOOTH_ERROR_PARAM_INVALID;
}

BluetoothError Bluez5ProfileMesh::supplyProvisioningOob(const std::string &bearer,
														const std::string &oobData)
{
	if (bearer == "PB-GATT")
	{
		return BLUETOOTH_ERROR_UNSUPPORTED;
	}
	else if (bearer == "PB-ADV")
	{
		return mMeshAdv->supplyStatic(oobData);
	}

	return BLUETOOTH_ERROR_PARAM_INVALID;
}

void Bluez5ProfileMesh::getCompositionData(const std::string &bearer, uint16_t destAddress,
										   BleMeshCompositionDataCallback callback)
{
}

BluetoothError Bluez5ProfileMesh::createAppKey(const std::string &bearer,
										uint16_t netKeyIndex,
										uint16_t appKeyIndex)
{
	if (bearer == "PB-GATT")
	{
		return BLUETOOTH_ERROR_UNSUPPORTED;
	}
	else if (bearer == "PB-ADV")
	{
		return mMeshAdv->createAppKey(netKeyIndex, appKeyIndex);
	}
	return BLUETOOTH_ERROR_PARAM_INVALID;;
}

BluetoothError Bluez5ProfileMesh::modelSend(const std::string &bearer, uint16_t srcAddress,
									 uint16_t destAddress, uint16_t appKeyIndex,
									 const std::string &command,
									 BleMeshPayload payload)
{
	if (bearer == "PB-GATT")
	{
		return BLUETOOTH_ERROR_UNSUPPORTED;
	}
	else if (bearer == "PB-ADV")
	{
		return mMeshAdv->modelSend(srcAddress, destAddress, appKeyIndex, command, payload);
	}

	return BLUETOOTH_ERROR_PARAM_INVALID;
}

BluetoothError Bluez5ProfileMesh::setOnOff(const std::string &bearer,
									 uint16_t destAddress, uint16_t appKeyIndex, bool onoff)
{
	if (bearer == "PB-GATT")
	{
		return BLUETOOTH_ERROR_UNSUPPORTED;
	}
	else if (bearer == "PB-ADV")
	{
		return mMeshAdv->setOnOff(destAddress, appKeyIndex, onoff);
	}

	return BLUETOOTH_ERROR_PARAM_INVALID;
}

void Bluez5ProfileMesh::configGet(BleMeshGetConfigCallback callback,
										const std::string &bearer,
										uint16_t destAddress,
										const std::string &config,
										uint16_t netKeyIndex)
{
}

BluetoothError Bluez5ProfileMesh::configSet(const std::string &bearer,
											uint16_t destAddress, const std::string &config,
											uint8_t gattProxyState, uint16_t netKeyIndex,
											uint16_t appKeyIndex, uint32_t modelId,
											uint8_t ttl, BleMeshRelayStatus *relayStatus)
{
	return BLUETOOTH_ERROR_UNSUPPORTED;
}

void Bluez5ProfileMesh::getProperties(const std::string &address,
									BluetoothPropertiesResultCallback callback)
{
}

void Bluez5ProfileMesh::getProperty(const std::string &address,
								BluetoothProperty::Type type,
								BluetoothPropertyResultCallback callback)
{
}
