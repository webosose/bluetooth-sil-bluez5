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
#include "bluez5meshapplication.h"
#include <openssl/rand.h>
#include "utils_mesh.h"
#include <cstdint>
#include <string>
#include <vector>

#define BLUEZ_MESH_NAME "org.bluez.mesh"
#define BLUEZ_MESH_APP_PATH "/" //"/silbluez/mesh"
#define BLUEZ_MESH_ELEMENT_PATH "/element"
#define ONE_SECOND				1000
#define RESPOND_WAIT_DURATION	2

#define DEFAULT_NET_KEY_INDEX 0x0000

/* Generic OnOff model opcodes */
#define OP_GENERIC_ONOFF_GET				0x8201
#define OP_GENERIC_ONOFF_SET				0x8202
#define OP_GENERIC_ONOFF_SET_UNACK			0x8203
#define OP_GENERIC_ONOFF_STATUS				0x8204

/* Config model opcodes */
#define OP_MODEL_APP_BIND				0x803D
#define OP_CONFIG_DEFAULT_TTL_GET		0x800C
#define OP_CONFIG_DEFAULT_TTL_SET		0x800D
#define OP_APPKEY_GET				0x8001
#define OP_CONFIG_RELAY_GET			0x8026
#define OP_CONFIG_RELAY_SET			0x8027
#define OP_CONFIG_PROXY_GET			0x8012
#define OP_CONFIG_PROXY_SET			0x8013
#define OP_DEV_COMP_GET				0x8008

#define TTL_MASK	0x7f

Bluez5MeshAdv::Bluez5MeshAdv(Bluez5ProfileMesh *mesh, Bluez5Adapter *adapter):
mMesh(mesh),
mAdapter(adapter),
mNetworkInterface(nullptr),
mObjectManager(nullptr),
mDbusConn(nullptr),
mMeshAdvProv(nullptr),
mMeshAdvProvAgent(nullptr),
mMeshApplication(nullptr),
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
	mMeshApplication = new Bluez5MeshApplication(mAdapter, mMesh);

	g_bus_watch_name(G_BUS_TYPE_SYSTEM, BLUEZ_MESH_NAME, G_BUS_NAME_WATCHER_FLAGS_NONE,
					 handleBluezMeshServiceStarted, handleBluezMeshServiceStopped, this, NULL);
}

Bluez5MeshAdv::~Bluez5MeshAdv()
{
	if (mMeshAdvProv)
	{
		g_object_unref(mMeshAdvProv);
		mMeshAdvProv = 0;
	}

	if (mMeshAdvProvAgent)
	{
		g_object_unref(mMeshAdvProvAgent);
		mMeshAdvProvAgent = 0;
	}

	if (mMeshApplication)
	{
		g_object_unref(mMeshApplication);
		mMeshApplication = 0;
	}

	if (mNetworkInterface)
	{
		g_object_unref(mNetworkInterface);
		mNetworkInterface = 0;
	}

	if (mObjectManager)
	{
		g_object_unref(mObjectManager);
		mObjectManager = 0;
	}

}

void Bluez5MeshAdv::getRandomBytes(unsigned char *buf, int size)
{
	RAND_bytes(buf, size);
}

void Bluez5MeshAdv::updateNetworkId()
{
	DEBUG("updateNetworkId mToken: %llu", mToken);
	mMesh->getMeshObserver()->updateNetworkId(convertAddressToLowerCase(mAdapter->getAddress()),
		mToken);
}

BluetoothError Bluez5MeshAdv::attachToken(const std::string &token)
{
	std::string::size_type sz = 0;
	mToken = std::stoull(token, &sz, 0);
	DEBUG("attachToken mToken: %llu", mToken);
	attach();
	return BLUETOOTH_ERROR_NONE;
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
			g_error_free(error);
			return;
		}
		DEBUG("attached node: %s", node);
	};
	bluez_mesh_network1_call_attach(mNetworkInterface, BLUEZ_MESH_APP_PATH, mToken, NULL,
									glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(attachCallback));
}

BluetoothError Bluez5MeshAdv::createNetwork()
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
			return BLUETOOTH_ERROR_FAIL;
		}
		DEBUG("Mesh CreateNetwork success\n");
		return BLUETOOTH_ERROR_NONE;
	};

	bluez_mesh_network1_call_create_network(mNetworkInterface, BLUEZ_MESH_APP_PATH, uuidVar, NULL, glibAsyncMethodWrapper,
											new GlibAsyncFunctionWrapper(createNetworkCallback));
	return BLUETOOTH_ERROR_NONE;
}

void Bluez5MeshAdv::getMeshInfo(BleMeshInfoCallback callback)
{

}

void Bluez5MeshAdv::handleBluezMeshServiceStarted(GDBusConnection *conn, const gchar *name,
												  const gchar *nameOwner, gpointer userData)
{
	GError *error = 0;
	Bluez5MeshAdv *meshAdv = static_cast<Bluez5MeshAdv *>(userData);
	GDBusObjectManagerServer * objectManagerServer = g_dbus_object_manager_server_new(BLUEZ_MESH_APP_PATH);
	GDBusObjectSkeleton *meshAppSkeleton = g_dbus_object_skeleton_new(BLUEZ_MESH_APP_PATH);

	meshAdv->mMeshApplication->registerApplicationInterface(objectManagerServer, meshAppSkeleton, meshAdv);
	meshAdv->mMeshAdvProv->registerProvisionerInterface(objectManagerServer, meshAppSkeleton);
	meshAdv->mMeshAdvProvAgent->registerProvAgentInterface(objectManagerServer);

	g_dbus_object_manager_server_export(objectManagerServer, meshAppSkeleton);

	for (auto element = meshAdv->mElements.begin(); element != meshAdv->mElements.end(); element++)
	{
		element->registerElementInterface(objectManagerServer, meshAdv);
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
				BLUEZ_MESH_NAME, objectPath.c_str(), NULL, &error);
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
				BLUEZ_MESH_NAME, objectPath.c_str(), NULL, &error);
		}
	}

	auto meshManagementInterface = g_dbus_object_get_interface(object, "org.bluez.mesh.Management1");
	if (meshManagementInterface)
	{
		DEBUG("org.bluez.mesh.Management1 interface added");
		meshAdv->mMgmtInterface = bluez_mesh_management1_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
			BLUEZ_MESH_NAME, objectPath.c_str(), NULL, &error);
	}

	auto meshNodeInterface = g_dbus_object_get_interface(object, "org.bluez.mesh.Node1");
	if (meshNodeInterface)
	{
		DEBUG("org.bluez.mesh.Node1 interface added");
		meshAdv->mNodeInterface = bluez_mesh_node1_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
			BLUEZ_MESH_NAME, objectPath.c_str(), NULL, &error);
	}
}

void Bluez5MeshAdv::handleObjectRemoved(GDBusObjectManager *objectManager,
											GDBusObject *object, void *userData)
{
}

BluetoothError Bluez5MeshAdv::scanUnprovisionedDevices(const uint16_t scanTimeout)
{
	GVariantBuilder *builder = 0;
	GVariant *params = 0;
	builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add (builder, "{sv}","Seconds" ,g_variant_new_uint16(scanTimeout));
	params = g_variant_builder_end(builder);
	g_variant_builder_unref(builder);

	GError *error = 0;
	bluez_mesh_management1_call_unprovisioned_scan_sync(mMgmtInterface, params, NULL, &error);
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
	const char *uuidStr = uuid.c_str();
	uint8_t uuidByte[16] = {0};
	int j = 0;

	auto provisionCallback = [this](GAsyncResult *result) {

		GError *error = 0;
		bluez_mesh_management1_call_add_node_finish(mMgmtInterface, result, &error);
		if (error)
		{
			ERROR(MSGID_MESH_PROFILE_ERROR, 0, "provision failed: %s", error->message);
			g_error_free(error);
			return BLUETOOTH_ERROR_FAIL;
		}
		DEBUG("provision success\n");
		return BLUETOOTH_ERROR_NONE;
	};

	for (int i = 0; i < 16; ++i)
	{
		sscanf(&uuidStr[i * 2], "%02hhx", &uuidByte[i]);
		DEBUG("%2.2x - %d", uuidByte[i], j);
		++j;
	}

	GBytes *bytes = g_bytes_new(uuidByte, 16);
	GVariant *uuidVar = g_variant_new_from_bytes(G_VARIANT_TYPE_BYTESTRING, bytes, true);

	bluez_mesh_management1_call_add_node(mMgmtInterface, uuidVar, createEmptyStringArrayVariant(), NULL, glibAsyncMethodWrapper,
										new GlibAsyncFunctionWrapper(provisionCallback));

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
										BleMeshPayload &payload)
{
	if (command.compare("onOff") == 0)
	{
		DEBUG("modelSend:: onOff");
		BleMeshPayloadOnOff payloadOnOff = payload.getPayloadOnOff();
		return setOnOff(destAddress, appKeyIndex, payloadOnOff.value);
	}
	else if(command.compare("passThrough") == 0)
	{
		BleMeshPayloadPassthrough payloadPassThr = payload.getPayloadPassthrough();
		return sendPassThrough(destAddress, appKeyIndex, payloadPassThr.value);
	}
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

BluetoothError Bluez5MeshAdv::sendPassThrough(uint16_t destAddress, uint16_t appIndex, const std::vector<uint8_t> value)
{
	uint8_t msg[32];
	uint16_t n;
	GError *error = 0;
	GVariantBuilder *builder = 0;
	GVariant *params = 0;

	builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add (builder, "{sv}","ForceSegmented" ,g_variant_new_boolean (false));
	params = g_variant_builder_end(builder);
	g_variant_builder_unref(builder);

	GVariantBuilder *dataBuilder = g_variant_builder_new (G_VARIANT_TYPE ("ay"));
	for (auto &dataIt : value)
	{
		DEBUG("data : %u ", dataIt);
		g_variant_builder_add(dataBuilder, "y", dataIt);
	}

	GVariant *dataToSend = g_variant_builder_end(dataBuilder);
	g_variant_builder_unref(dataBuilder);

	bluez_mesh_node1_call_send_sync(mNodeInterface, BLUEZ_MESH_ELEMENT_PATH,
								destAddress, appIndex,
								params, dataToSend, NULL, &error);
	if (error)
	{
		ERROR(MSGID_MESH_PROFILE_ERROR, 0, "Model sendPassThrough failed: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_FAIL;
	}

	return BLUETOOTH_ERROR_NONE;
}

BluetoothError Bluez5MeshAdv::setOnOff(uint16_t destAddress, uint16_t appIndex, bool onoff)
{
	uint8_t msg[32];
	uint16_t n;
	GError *error = 0;
	static bool createKey = 0;

	GVariantBuilder *builder = 0;
	GVariant *params = 0;
	builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add (builder, "{sv}","ForceSegmented" ,g_variant_new_boolean (false));
	params = g_variant_builder_end(builder);
	g_variant_builder_unref(builder);

	DEBUG("onoff:%d", onoff);

	n = meshOpcodeSet(OP_GENERIC_ONOFF_SET, msg);
	msg[n++] = onoff;
	msg[n++] = mTransacId++;
	GBytes *bytes = g_bytes_new(msg, n);
	GVariant *dataToSend = g_variant_new_from_bytes(G_VARIANT_TYPE_BYTESTRING, bytes, true);
	g_bytes_unref (bytes);

	bluez_mesh_node1_call_send_sync(mNodeInterface, BLUEZ_MESH_ELEMENT_PATH,
								destAddress, appIndex,
								params, dataToSend, NULL, &error);
	if (error)
	{
		ERROR(MSGID_MESH_PROFILE_ERROR, 0, "model send failed: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_FAIL;
	}

	return BLUETOOTH_ERROR_NONE;
}

BluetoothError Bluez5MeshAdv::configGet(uint16_t destAddress,
										const std::string &config,
										uint16_t netKeyIndex)
{
	startTimer(config);

	if (!mNodeInterface)
	{
		return BLUETOOTH_ERROR_NOT_ALLOWED;
	}

	if(config == "APPKEYINDEX")
	{
		return getAppKeyIndex(destAddress, netKeyIndex);
	}
	else if(config == "DEFAULT_TTL")
	{
		return getDefaultTTL(destAddress, netKeyIndex);
	}
	else if(config == "GATT_PROXY")
	{
		return getGATTProxy(destAddress, netKeyIndex);
	}
	else if(config == "RELAY")
	{
		return getRelay(destAddress, netKeyIndex);
	}

	stopReqTimer();

	return BLUETOOTH_ERROR_PARAM_INVALID;
}

BluetoothError Bluez5MeshAdv::configSet(
		uint16_t destAddress, const std::string &config,
		uint8_t gattProxyState, uint16_t netKeyIndex, uint16_t appKeyIndex,
		uint32_t modelId, uint8_t ttl, BleMeshRelayStatus *relayStatus)
{
	if (!mNodeInterface)
	{
		return BLUETOOTH_ERROR_NOT_ALLOWED;
	}

	startTimer(config);

	if(config == "APPKEY_ADD")
	{
		return configAppKeyAdd(destAddress, netKeyIndex, appKeyIndex);
	}
	else if(config == "APPKEY_UPDATE")
	{
		return configAppKeyUpdate(destAddress, netKeyIndex, appKeyIndex);
	}
	else if(config == "APPKEY_BIND")
	{
		return configBindAppKey(destAddress, netKeyIndex, appKeyIndex, modelId);
	}
	else if(config == "DEFAULT_TTL")
	{
		return setDefaultTTL(destAddress, netKeyIndex, ttl);
	}
	else if(config == "GATT_PROXY")
	{
		return setGATTProxy(destAddress, netKeyIndex, gattProxyState);
	}
	else if(config == "RELAY")
	{
		return setRelay(destAddress, netKeyIndex, relayStatus);
	}

	stopReqTimer();

	return BLUETOOTH_ERROR_PARAM_INVALID;
}

GVariant* Bluez5MeshAdv::createEmptyStringArrayVariant()
{
	GVariantBuilder *builder = 0;
	GVariant *params = 0;
	builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
	params = g_variant_builder_end(builder);
	g_variant_builder_unref(builder);
	return params;
}

GVariant* Bluez5MeshAdv::prepareSendDevKeyData(uint8_t *msg, uint16_t n)
{
	GBytes *bytes = g_bytes_new(msg, n);
	GVariant *dataToSend = g_variant_new_from_bytes(G_VARIANT_TYPE_BYTESTRING, bytes, true);
	g_bytes_unref (bytes);
	return dataToSend;
}

BluetoothError Bluez5MeshAdv::devKeySend(uint16_t destAddress, uint16_t netKeyIndex, uint8_t *msg, uint16_t n)
{
	GError *error = 0;
	BluetoothError silError = BLUETOOTH_ERROR_FAIL;

	bluez_mesh_node1_call_dev_key_send_sync(mNodeInterface, BLUEZ_MESH_ELEMENT_PATH,
											destAddress, true, netKeyIndex,
											createEmptyStringArrayVariant(), prepareSendDevKeyData(msg, n), NULL, &error);

	if (error)
	{
		ERROR(MSGID_MESH_PROFILE_ERROR, 0, "devKeySend failed: %s", error->message);
		if (strstr(error->message, "Object not found"))
		{
			silError = BLUETOOTH_ERROR_MESH_NET_KEY_INDEX_DOES_NOT_EXIST;
		}
		g_error_free(error);
		stopReqTimer();
		return silError;
	}

	return BLUETOOTH_ERROR_NONE;
}

BluetoothError Bluez5MeshAdv::setDefaultTTL(uint16_t destAddress, uint16_t netKeyIndex, uint8_t ttl)
{
	uint8_t msg[32];
	uint16_t n;

	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	if (ttl > TTL_MASK)
		return BLUETOOTH_ERROR_PARAM_INVALID;

	n = meshOpcodeSet(OP_CONFIG_DEFAULT_TTL_SET, msg);
	msg[n++] = ttl;

	return devKeySend(destAddress, netKeyIndex, msg, n);
}

BluetoothError Bluez5MeshAdv::getDefaultTTL(uint16_t destAddress, uint16_t netKeyIndex)
{
	uint8_t msg[32];
	uint16_t n;

	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	n = meshOpcodeSet(OP_CONFIG_DEFAULT_TTL_GET, msg);

	return devKeySend(destAddress, netKeyIndex, msg, n);
}

BluetoothError Bluez5MeshAdv::getGATTProxy(uint16_t destAddress, uint16_t netKeyIndex)
{
	uint8_t msg[32];
	uint16_t n;

	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	n = meshOpcodeSet(OP_CONFIG_PROXY_GET, msg);

	return devKeySend(destAddress, netKeyIndex, msg, n);
}

BluetoothError Bluez5MeshAdv::setGATTProxy(uint16_t destAddress, uint16_t netKeyIndex, uint8_t gattProxyState)
{
	uint8_t msg[2 + 1];
	uint16_t n;

	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	n = meshOpcodeSet(OP_CONFIG_PROXY_SET, msg);
	msg[n++] = gattProxyState;

	return devKeySend(destAddress, netKeyIndex, msg, n);
}

BluetoothError Bluez5MeshAdv::getRelay(uint16_t destAddress, uint16_t netKeyIndex)
{
	uint8_t msg[32];
	uint16_t n;

	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	n = meshOpcodeSet(OP_CONFIG_RELAY_GET, msg);

	return devKeySend(destAddress, netKeyIndex, msg, n);
}

BluetoothError Bluez5MeshAdv::setRelay(uint16_t destAddress, uint16_t netKeyIndex, BleMeshRelayStatus *relayStatus)
{
	uint8_t msg[2 + 2 + 4];
	uint16_t n;

	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	DEBUG("%d::%d::%d",relayStatus->getRelay(),relayStatus->getrelayRetransmitCount(), relayStatus->getRelayRetransmitIntervalSteps());
	n = meshOpcodeSet(OP_CONFIG_RELAY_SET, msg);

	msg[n++] = relayStatus->getRelay();
	msg[n++] = relayStatus->getrelayRetransmitCount() | (relayStatus->getRelayRetransmitIntervalSteps() << 3);

	return devKeySend(destAddress, netKeyIndex, msg, n);
}

BluetoothError Bluez5MeshAdv::getAppKeyIndex(uint16_t destAddress, uint16_t netKeyIndex)
{
	uint8_t msg[32];
	uint16_t n;

	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	n = meshOpcodeSet(OP_APPKEY_GET, msg);

	put_le16(netKeyIndex, msg + n);
	n += 2;

	return devKeySend(destAddress, netKeyIndex, msg, n);
}

BluetoothError Bluez5MeshAdv::configBindAppKey(uint16_t destAddress,
												uint16_t netKeyIndex, uint16_t appKeyIndex, uint32_t modelId)
{
	uint16_t n;
	uint8_t msg[32];

	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	n = meshOpcodeSet(OP_MODEL_APP_BIND, msg);

	put_le16(destAddress, msg + n);
	n += 2;
	put_le16(appKeyIndex, msg + n);
	n += 2;

	n += putModelId(msg + n, &modelId, false);

	return devKeySend(destAddress, netKeyIndex, msg, n);
}

BluetoothError Bluez5MeshAdv::addAppKey(uint16_t destAddress,	uint16_t netKeyIndex, uint16_t appKeyIndex, bool update)
{
	GError *error = 0;
	BluetoothError silError = BLUETOOTH_ERROR_FAIL;
	bluez_mesh_node1_call_add_app_key_sync(mNodeInterface, BLUEZ_MESH_ELEMENT_PATH,
										destAddress, appKeyIndex, netKeyIndex, update, NULL, &error);
	if (error)
	{
		ERROR(MSGID_MESH_PROFILE_ERROR, 0, "ConfigAppKey failed: %s", error->message);

		if (strstr(error->message, "AppKey not found"))
		{
			silError = BLUETOOTH_ERROR_MESH_APP_KEY_INDEX_DOES_NOT_EXIST;
		}
		else if (strstr(error->message, "Object not found"))
		{
			silError = BLUETOOTH_ERROR_MESH_NET_KEY_INDEX_DOES_NOT_EXIST;
		}
		else if (strstr(error->message, "Cannot update"))
		{
			silError = BLUETOOTH_ERROR_MESH_CANNOT_UPDATE_APPKEY;
		}
		g_error_free(error);
		stopReqTimer();
		return silError;
	}
	return BLUETOOTH_ERROR_NONE;
}

BluetoothError Bluez5MeshAdv::configAppKeyAdd(uint16_t destAddress,
												uint16_t netKeyIndex, uint16_t appKeyIndex)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	return addAppKey(destAddress, netKeyIndex, appKeyIndex, false);
}

BluetoothError Bluez5MeshAdv::configAppKeyUpdate(uint16_t destAddress,
												uint16_t netKeyIndex, uint16_t appKeyIndex)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	return addAppKey(destAddress, netKeyIndex, appKeyIndex, true);
}

BluetoothError Bluez5MeshAdv::registerElement(uint8_t index,
												std::vector<uint32_t> &sigModelIds,
												std::vector<uint32_t> &vendorModelIds)
{
	Bluez5MeshElement element(index, mAdapter, mMesh);

	for (auto sigModelId = sigModelIds.begin(); sigModelId != sigModelIds.end();
		 sigModelId++)
	{
		element.addModel(*sigModelId);
	}

	for (auto vendorModelId = vendorModelIds.begin(); vendorModelId != vendorModelIds.end();
		 vendorModelId++)
	{
		element.addModel(*vendorModelId);
	}

	mElements.push_back(element);

	return BLUETOOTH_ERROR_NONE;
}

static gboolean atTimeOut(gpointer userdata)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	Bluez5MeshAdv *adv = (Bluez5MeshAdv *)userdata;
	adv->mMesh->getMeshObserver()->modelConfigResult(convertAddressToLowerCase(adv->mAdapter->getAddress()), adv->mConfiguration, BLUETOOTH_ERROR_MESH_NO_RESPONSE_FROM_NODE);
	return false;
}

void Bluez5MeshAdv::startTimer(const std::string config)
{
	stopReqTimer();
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	mConfiguration.setConfig(config);
	mReqExpTimerId = g_timeout_add( RESPOND_WAIT_DURATION * ONE_SECOND, atTimeOut, this);
	DEBUG("Request timer started [%d]",mReqExpTimerId);
}

void Bluez5MeshAdv::stopReqTimer()
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	if (mReqExpTimerId)
	{
		g_source_remove(mReqExpTimerId);
		DEBUG("Request timer stopped [%d]",mReqExpTimerId);
		mReqExpTimerId = 0;
	}
	return;
}

BluetoothError Bluez5MeshAdv::getCompositionData(uint16_t destAddress)
{
	uint16_t n;
	uint8_t msg[32];

	n = meshOpcodeSet(OP_DEV_COMP_GET, msg);

	/* By default, use page 0 */
	msg[n++] = 0;

	startTimer("COMPOSITION_DATA");

	return devKeySend(destAddress, DEFAULT_NET_KEY_INDEX, msg, n);
}

BluetoothError Bluez5MeshAdv::updateNodeInfo(std::vector<uint16_t> &unicastAddresses)
{
	return mMeshAdvProv->updateNodeInfo(unicastAddresses);
}