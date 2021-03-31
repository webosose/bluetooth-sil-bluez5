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

//#include <dbus/dbus.h>
#include "bluez5meshadv.h"
#include "bluez5profilemesh.h"
#include "bluez5adapter.h"
#include "utils.h"
#include "logging.h"
#include "asyncutils.h"
#include "bluez5meshelement.h"
#include "bluez5meshadvprovisioner.h"
#include "bluez5meshadvprovagent.h"
#include <openssl/rand.h>
#include "utils_mesh.h"
#include <cstdint>
#include <string>
#include <vector>

#define BLUEZ_MESH_NAME "org.bluez.mesh"
#define BLUEZ_MESH_APP_PATH "/" //"/silbluez/mesh"
#define BLUEZ_MESH_ELEMENT_PATH "/element"

#define DEFAULT_NET_INDEX 0x0000

/* Generic OnOff model opcodes */
#define OP_GENERIC_ONOFF_GET					0x8201
#define OP_GENERIC_ONOFF_SET					0x8202
#define OP_GENERIC_ONOFF_SET_UNACK			  0x8203
#define OP_GENERIC_ONOFF_STATUS				 0x8204

/* Config model opcodes */
#define OP_MODEL_APP_BIND					   0x803D

Bluez5MeshAdv::Bluez5MeshAdv(Bluez5ProfileMesh *mesh, Bluez5Adapter *adapter):
mMesh(mesh),
mAdapter(adapter),
mNetworkInterface(nullptr),
mObjectManager(nullptr),
mDbusConn(nullptr),
mToken(0)
{
	GError *error = 0;

	mDbusConn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (error)
	{
		ERROR(MSGID_MESH_PROFILE_ERROR, 0, "Failed to setup dbus: %s", error->message);
		g_error_free(error);
		return;
	}

	mMeshAdvProv = new Bluez5MeshAdvProvisioner(mAdapter, mMesh);
	mMeshAdvProvAgent = new Bluez5MeshAdvProvAgent(mAdapter, mMesh);

	g_bus_watch_name(G_BUS_TYPE_SYSTEM, BLUEZ_MESH_NAME, G_BUS_NAME_WATCHER_FLAGS_NONE,
					 handleBluezMeshServiceStarted, handleBluezMeshServiceStopped, this, NULL);
}

Bluez5MeshAdv::~Bluez5MeshAdv()
{
}

void Bluez5MeshAdv::getRandomBytes(unsigned char *buf, int size)
{
	RAND_bytes(buf, size);
}

gboolean Bluez5MeshAdv::handleJoinComplete(BluezMeshApplication1 *object,
										   GDBusMethodInvocation *invocation,
										   guint64 argToken, gpointer userData)
{
	Bluez5MeshAdv *meshAdv = static_cast<Bluez5MeshAdv *>(userData);
	meshAdv->mToken = argToken;
	DEBUG("handleJoinCompletem mToken: %lu", meshAdv->mToken);

	meshAdv->attach();
	return true;
}

gboolean Bluez5MeshAdv::handleJoinFailed(BluezMeshApplication1 *object,
										 GDBusMethodInvocation *invocation,
										 const gchar *argReason, gpointer userData)
{
	DEBUG("handleJoinFailed, reason: %s", argReason);
	return true;
}

void Bluez5MeshAdv::attach()
{
	auto attachCallback = [this](GAsyncResult *result) {
		GError *error = 0;
		gchar *node = 0;
		GVariant *configuration = 0;
		bluez_mesh_network1_call_attach_finish(mNetworkInterface, &node, &configuration, result, &error);
		if (error)
		{
			ERROR(MSGID_MESH_PROFILE_ERROR, 0, "Atttach failed: %s", error->message);
		}
		else
		{
			DEBUG("node: %s", node);
		}
	};

	bluez_mesh_network1_call_attach(mNetworkInterface, BLUEZ_MESH_APP_PATH, mToken, NULL,
									glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(attachCallback));
}

void Bluez5MeshAdv::createNetwork(BleMeshNetworkIdCallback callback)
{
	GError *error = 0;
	getRandomBytes(mUuid, 16);

	GBytes *bytes = g_bytes_new(mUuid, 16);

	GVariant *uuidVar = g_variant_new_from_bytes(G_VARIANT_TYPE_BYTESTRING, bytes, true);
	auto createNetworkCallback = [this](GAsyncResult *result) {

		GError *error = 0;
		bluez_mesh_network1_call_create_network_finish(mNetworkInterface, result, &error);
		if (error)
		{
			ERROR(MSGID_MESH_PROFILE_ERROR, 0, "CreateNetwork failed: %s", error->message);
			g_error_free(error);
		}
		else
		{
			DEBUG("Mesh CreateNetwork success");
		}
		return;
	};

	bluez_mesh_network1_call_create_network(mNetworkInterface, BLUEZ_MESH_APP_PATH, uuidVar, NULL, glibAsyncMethodWrapper,
											new GlibAsyncFunctionWrapper(createNetworkCallback));
	return;
}

void Bluez5MeshAdv::getMeshInfo(BleMeshInfoCallback callback)
{

}

void Bluez5MeshAdv::handleBluezMeshServiceStarted(GDBusConnection *conn, const gchar *name,
												  const gchar *nameOwner, gpointer userData)
{
	GError *error = 0;
	Bluez5MeshAdv *meshAdv = static_cast<Bluez5MeshAdv *>(userData);
	GDBusObjectManagerServer * objectManagerServer = g_dbus_object_manager_server_new("/");

	GDBusObjectSkeleton *meshAppSkeleton = g_dbus_object_skeleton_new(BLUEZ_MESH_APP_PATH);
	BluezMeshApplication1 *appInterface = bluez_mesh_application1_skeleton_new();
	uint16_t cid = 0x05f1;
	uint16_t pid = 0x0002;
	uint16_t vid = 0x0001;
	bluez_mesh_application1_set_company_id(appInterface, cid);
	bluez_mesh_application1_set_product_id(appInterface, pid);
	bluez_mesh_application1_set_version_id(appInterface, vid);
	bluez_mesh_application1_set_crpl(appInterface, 10);

	g_signal_connect(appInterface, "handle_join_complete", G_CALLBACK(handleJoinComplete), meshAdv);
	g_signal_connect(appInterface, "handle_join_failed", G_CALLBACK(handleJoinFailed), meshAdv);

	g_dbus_object_skeleton_add_interface(meshAppSkeleton, G_DBUS_INTERFACE_SKELETON(appInterface));
	meshAdv->mMeshAdvProv->registerProvisionerInterface(objectManagerServer, meshAppSkeleton);
	meshAdv->mMeshAdvProvAgent->registerProvAgentInterface(objectManagerServer);
	g_dbus_object_manager_server_export(objectManagerServer, meshAppSkeleton);

	for (auto element = meshAdv->mElements.begin(); element != meshAdv->mElements.end(); element++)
	{
		(*element)->registerElementInterface(objectManagerServer);
	}

	g_dbus_object_manager_server_set_connection(objectManagerServer, conn);

	meshAdv->mObjectManager = g_dbus_object_manager_client_new_sync(
		conn, G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
		BLUEZ_MESH_NAME, "/", NULL, NULL, NULL, NULL, &error);

	if (error)
	{
		ERROR(MSGID_MESH_PROFILE_ERROR, 0,
			  "Failed to create object manager: %s", error->message);
		g_error_free(error);
		return;
	}

	g_signal_connect(meshAdv->mObjectManager, "object-added", G_CALLBACK(handleObjectAdded), meshAdv);
	g_signal_connect(meshAdv->mObjectManager, "object-removed", G_CALLBACK(handleObjectRemoved), meshAdv);

	GList* objects = g_dbus_object_manager_get_objects(meshAdv->mObjectManager);
	DEBUG("Mesh object length: %d", g_list_length(objects));
	for (int n = 0; n < g_list_length(objects); n++)
	{
		auto object = static_cast<GDBusObject*>(g_list_nth(objects, n)->data);
		std::string objectPath = g_dbus_object_get_object_path(object);
		DEBUG("Object path: %s", objectPath.c_str());

		auto networkInterface = g_dbus_object_get_interface(object, "org.bluez.mesh.Network1");
		if (networkInterface)
		{
			DEBUG("org.bluez.mesh.Network1 interface added");
			meshAdv->mNetworkInterface = bluez_mesh_network1_proxy_new_for_bus_sync(
				G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
				"org.bluez.mesh", objectPath.c_str(), NULL, &error);
			if (error)
			{
				ERROR(MSGID_MESH_PROFILE_ERROR, 0, "Not able to get Mesh Network interface");
				g_error_free(error);
				return;
			}
			g_object_unref(networkInterface);
		}
		else
		{
			 DEBUG("org.bluez.mesh.Network1 interface add failed!!");
		}
		g_object_unref(object);
	}
	g_list_free(objects);
}

void Bluez5MeshAdv::handleBluezMeshServiceStopped(GDBusConnection *conn, const gchar *name,
												  gpointer userData)
{
}

void Bluez5MeshAdv::handleObjectAdded(GDBusObjectManager *objectManager,
										  GDBusObject *object, void *userData)
{
	DEBUG("Bluez5MeshAdv::handleObjectAdded");
	GError *error = 0;
	Bluez5MeshAdv *meshAdv = static_cast<Bluez5MeshAdv *>(userData);
	std::string objectPath = g_dbus_object_get_object_path(object);

	//TODO: Compare the application paths
	if (!meshAdv->mNetworkInterface)
	{
		auto networkInterface = g_dbus_object_get_interface(object, "org.bluez.mesh.Network1");
		if (networkInterface)
		{
			DEBUG("org.bluez.mesh.Network1 interface added");
			meshAdv->mNetworkInterface = bluez_mesh_network1_proxy_new_for_bus_sync(
				G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
				"org.bluez.mesh", objectPath.c_str(), NULL, &error);
		}
	}

	auto meshManagementInterface = g_dbus_object_get_interface(object, "org.bluez.mesh.Management1");
	if (meshManagementInterface)
	{
		DEBUG("org.bluez.mesh.Management1 interface added");
		meshAdv->mMgmtInterface = bluez_mesh_management1_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
			"org.bluez.mesh", objectPath.c_str(), NULL, &error);
	}

	auto meshNodeInterface = g_dbus_object_get_interface(object, "org.bluez.mesh.Node1");
	if (meshNodeInterface)
	{
		DEBUG("org.bluez.mesh.Node1 interface added");
		meshAdv->mNodeInterface = bluez_mesh_node1_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
			"org.bluez.mesh", objectPath.c_str(), NULL, &error);
	}
}

void Bluez5MeshAdv::handleObjectRemoved(GDBusObjectManager *objectManager,
											GDBusObject *object, void *userData)
{
}



BluetoothError Bluez5MeshAdv::scanUnprovisionedDevices(const uint16_t scanTimeout)
{
	GError *error = 0;
	bluez_mesh_management1_call_unprovisioned_scan_sync(mMgmtInterface, 0, NULL, &error);
	if (error)
	{
		ERROR(MSGID_MESH_PROFILE_ERROR, 0, "UnProvisionedScan failed: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_FAIL;
	}
	return BLUETOOTH_ERROR_NONE;
}

BluetoothError Bluez5MeshAdv::unprovisionedScanCancel()
{
	GError *error = 0;
	bluez_mesh_management1_call_unprovisioned_scan_cancel_sync(mMgmtInterface, NULL, &error);
	if (error)
	{
		ERROR(MSGID_MESH_PROFILE_ERROR, 0, "UnprovisionedScanCancel failed: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_FAIL;
	}
	return BLUETOOTH_ERROR_NONE;
}

static unsigned char gethex(const char *s, char **endptr) {
  return strtoul(s, endptr, 16);
}

BluetoothError Bluez5MeshAdv::provision(const std::string &uuid, const uint16_t timeout)
{
	GError *error = 0;
	const char *uuidStr = uuid.c_str();
	uint8_t uuidByte[17];
	int j = 0;

	for (int i = 0; i < 16; ++i)
	{
			sscanf(&uuidStr[i * 2], "%02hhx", &uuidByte[i]);
			//sscanf(&uuidStr[i], "%02x", &uuidByte[j]);
			DEBUG("%2.2x - %d", uuidByte[i], j);
			++j;
	}

	GBytes *bytes = g_bytes_new(uuidByte, 16);
	GVariant *uuidVar = g_variant_new_from_bytes(G_VARIANT_TYPE_BYTESTRING, bytes, true);

	bluez_mesh_management1_call_add_node_sync(mMgmtInterface, uuidVar,
											  NULL, &error);
	if (error)
	{
		ERROR(MSGID_MESH_PROFILE_ERROR, 0, "provision failed: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_FAIL;
	}
	return BLUETOOTH_ERROR_NONE;
}

BluetoothError Bluez5MeshAdv::supplyNumeric(uint32_t number)
{
	return mMeshAdvProvAgent->supplyNumeric(number);
}

BluetoothError Bluez5MeshAdv::supplyStatic(const std::string &oobData)
{
	return mMeshAdvProvAgent->supplyStatic(oobData);
}

void Bluez5MeshAdv::getCompositionData(uint16_t destAddress,
										   BleMeshCompositionDataCallback callback)
{
}

BluetoothError Bluez5MeshAdv::createAppKey(uint16_t netKeyIndex, uint16_t appKeyIndex)
{
	GError *error = 0;
	bluez_mesh_management1_call_create_app_key_sync(mMgmtInterface,
													netKeyIndex, appKeyIndex, NULL, &error);
	if (error)
	{
		ERROR(MSGID_MESH_PROFILE_ERROR, 0, "createAppKey failed: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_FAIL;
	}
	return BLUETOOTH_ERROR_NONE;
}

uint16_t Bluez5MeshAdv::putModelId(uint8_t *buf, uint32_t *args, bool vendor)
{
		uint16_t n = 2;

		if (vendor) {
				put_le16(args[1], buf);
				buf += 2;
				n = 4;
		}

		put_le16(args[0], buf);

		return n;
}

BluetoothError Bluez5MeshAdv::modelSend(uint16_t srcAddress, uint16_t destAddress,
										uint16_t appKeyIndex,
										const std::string &command,
										BleMeshPayload payload)
{
	return BLUETOOTH_ERROR_UNSUPPORTED;
}

uint16_t Bluez5MeshAdv::meshOpcodeSet(uint32_t opcode, uint8_t *buf)
{
		if (opcode <= 0x7e) {
				buf[0] = opcode;
				return 1;
		} else if (opcode >= 0x8000 && opcode <= 0xbfff) {
				put_be16(opcode, buf);
				return 2;
		} else if (opcode >= 0xc00000 && opcode <= 0xffffff) {
				buf[0] = (opcode >> 16) & 0xff;
				put_be16(opcode, buf + 1);
				return 3;
		} else {
				DEBUG("Illegal Opcode %x", opcode);
				return 0;
		}
}

BluetoothError Bluez5MeshAdv::setOnOff(uint16_t destAddress, uint16_t appIndex, bool onoff)
{
	uint8_t msg[32];
	uint16_t n;
	GError *error = 0;

	DEBUG("onoff:%d", onoff);

	n = meshOpcodeSet(OP_GENERIC_ONOFF_SET, msg);
	msg[n++] = onoff;
	msg[n++] = mTransacId++;
	GBytes *bytes = g_bytes_new(msg, n);
	GVariant *dataToSend = g_variant_new_from_bytes(G_VARIANT_TYPE_BYTESTRING, bytes, true);

	bluez_mesh_node1_call_send_sync(mNodeInterface, BLUEZ_MESH_ELEMENT_PATH,
											destAddress, appIndex,
											dataToSend, NULL, &error);
	if (error)
	{
		ERROR(MSGID_MESH_PROFILE_ERROR, 0, "model send failed: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_FAIL;
	}

	return BLUETOOTH_ERROR_NONE;
}

void Bluez5MeshAdv::configGet(BleMeshGetConfigCallback callback,
										uint16_t destAddress,
										const std::string &config,
										uint16_t netKeyIndex)
{
}

BluetoothError Bluez5MeshAdv::configSet(
		uint16_t destAddress, const std::string &config,
		uint8_t gattProxyState, uint16_t netKeyIndex, uint16_t appKeyIndex,
		uint32_t modelId, uint8_t ttl, BleMeshRelayStatus *relayStatus)
{
	return BLUETOOTH_ERROR_UNSUPPORTED;
}

BluetoothError Bluez5MeshAdv::registerElement(uint8_t index,
										   std::vector<uint32_t> &sigModelIds,
										   std::vector<uint32_t> &vendorModelIds)
{
	Bluez5MeshElement *element = new Bluez5MeshElement(index);

	for (auto sigModelId = sigModelIds.begin(); sigModelId != sigModelIds.end();
		 sigModelId++)
	{
		element->addModel(*sigModelId);
	}

	for (auto vendorModelId = vendorModelIds.begin(); vendorModelId != vendorModelIds.end();
		 vendorModelId++)
	{
		element->addModel(*vendorModelId);
	}

	mElements.push_back(element);

	return BLUETOOTH_ERROR_NONE;
}
