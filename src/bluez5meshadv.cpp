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
#include <cstdint>
#include <string>
#include <vector>
#include <thread>

#define BLUEZ_MESH_NAME "org.bluez.mesh"
#define BLUEZ_MESH_APP_PATH "/" //"/silbluez/mesh"
#define BLUEZ_MESH_ELEMENT_PATH "/element"
#define ONE_SECOND				1000
#define RESPOND_WAIT_DURATION	2
#define LOCAL_NODE_ADDRESS 001


/* Generic OnOff model opcodes */
#define OP_GENERIC_ONOFF_GET				0x8201
#define OP_GENERIC_ONOFF_SET				0x8202
#define OP_GENERIC_ONOFF_SET_UNACK			0x8203
#define OP_GENERIC_ONOFF_STATUS				0x8204

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
mMgmtInterface(nullptr),
mNodeInterface(nullptr),
mTransacId(0),
mReqExpTimerId(0),
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

	if (mMgmtInterface)
	{
		g_object_unref(mMgmtInterface);
		mMgmtInterface = 0;
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
	if(!mNetworkInterface)
		return BLUETOOTH_ERROR_NOT_ALLOWED;

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
		element->registerElementInterface(objectManagerServer);
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

BluetoothError Bluez5MeshAdv::setOnOff(uint16_t destAddress, uint16_t appIndex, bool onoff, bool ack)
{
	if (ack)
	{
		startTimer("setOnOff");
		mConfiguration.setOnOffState(onoff);
	}
	return mElements[0].setOnOff(destAddress, appIndex, onoff);
}

BluetoothError Bluez5MeshAdv::configGet(uint16_t destAddress,
										const std::string &config,
										uint16_t netKeyIndex)
{
	return mElements[0].configGet(destAddress, config, netKeyIndex);

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

	return mElements[0].configSet(destAddress, config, gattProxyState, netKeyIndex,
									appKeyIndex, modelId, ttl, relayStatus);
}

BluetoothError Bluez5MeshAdv::deleteNode(uint16_t destAddress, uint8_t count)
{
	if (!mNodeInterface)
	{
		return BLUETOOTH_ERROR_NOT_ALLOWED;
	}

	return mElements[0].deleteNode(destAddress, count);
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

BluetoothError Bluez5MeshAdv::send(uint16_t destAddress, uint16_t appKeyIndex,
									uint8_t *msg, uint16_t msgLen)
{
	BluetoothError silError = BLUETOOTH_ERROR_FAIL;

	GError *error = 0;
	GVariantBuilder *builder = 0;
	GVariant *params = 0;

	builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add (builder, "{sv}","ForceSegmented" ,g_variant_new_boolean (false));
	params = g_variant_builder_end(builder);
	g_variant_builder_unref(builder);

	GBytes *bytes = g_bytes_new(msg, msgLen);
	GVariant *dataToSend = g_variant_new_from_bytes(G_VARIANT_TYPE_BYTESTRING, bytes, true);
	g_bytes_unref (bytes);

	bluez_mesh_node1_call_send_sync(mNodeInterface, BLUEZ_MESH_ELEMENT_PATH,
								destAddress, appKeyIndex,
								params, dataToSend, NULL, &error);
	if (error)
	{
		ERROR(MSGID_MESH_PROFILE_ERROR, 0, "Send failed: %s", error->message);
		if (strstr(error->message, "Object not found"))
		{
			silError = BLUETOOTH_ERROR_MESH_APP_KEY_INDEX_DOES_NOT_EXIST;
		}
		g_error_free(error);
		stopReqTimer();
		return silError;
	}

	return BLUETOOTH_ERROR_NONE;
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
		silError = BLUETOOTH_ERROR_FAIL;

		g_error_free(error);
		error = NULL;
		stopReqTimer();
		return silError;
	}

	return BLUETOOTH_ERROR_NONE;
}

BluetoothError Bluez5MeshAdv::registerElement(uint8_t index,
												std::vector<uint32_t> &sigModelIds,
												std::vector<uint32_t> &vendorModelIds)
{
	Bluez5MeshElement element(index, mAdapter, mMesh, this);

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
	if (adv->mConfiguration.getConfig() == "setOnOff")
	{
		// Ack is not supported buy nrf Device so returning same state
		adv->mMesh->getMeshObserver()->modelSetOnOffResult(convertAddressToLowerCase(adv->mAdapter->getAddress()), adv->mConfiguration.getOnOffState(), BLUETOOTH_ERROR_NONE);
	}
	else
	{
		adv->mMesh->getMeshObserver()->modelConfigResult(convertAddressToLowerCase(adv->mAdapter->getAddress()), adv->mConfiguration, BLUETOOTH_ERROR_MESH_NO_RESPONSE_FROM_NODE);
	}
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
	return mElements[0].getCompositionData(destAddress);
}

BluetoothError Bluez5MeshAdv::updateNodeInfo(std::vector<uint16_t> &unicastAddresses)
{
	return mMeshAdvProv->updateNodeInfo(unicastAddresses);
}

BluetoothError Bluez5MeshAdv::deleteRemoteNodeFromLocalKeyDatabase(uint16_t primaryAddress, uint8_t count)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	if (!mMgmtInterface)
	{
		return BLUETOOTH_ERROR_NOT_ALLOWED;
	}

	GError *error = 0;
	BluetoothError silError = BLUETOOTH_ERROR_FAIL;

	bluez_mesh_management1_call_delete_remote_node_sync(mMgmtInterface,
													primaryAddress, count, NULL, &error);
	if (error)
	{
		ERROR(MSGID_MESH_PROFILE_ERROR, 0, "deleteRemoteNode failed: %s :%d", error->message, primaryAddress);
		g_error_free(error);
		error = NULL;
		return BLUETOOTH_ERROR_FAIL;
	}
	return BLUETOOTH_ERROR_NONE;
}

void Bluez5MeshAdv::distributeKeys(bool refreshAppKeys, std::vector<uint16_t> appKeyIndexesToRefresh,
	std::vector<BleMeshNode> &nodes, uint16_t netKeyIndex, int32_t waitTime)
{
	BluetoothError btError = BLUETOOTH_ERROR_NONE;
	std::string status = "active";
	GError *error = 0;
	for (auto node = nodes.begin(); node != nodes.end();)
	{
		DEBUG("updating netkeyindex to : %d", node->getPrimaryElementAddress());
		btError = mElements[0].configSet(node->getPrimaryElementAddress(),
						"NETKEY_UPDATE", 0, netKeyIndex, 0, 0, 0, NULL,
						waitTime, node->getNumberOfElements());
		if (BLUETOOTH_ERROR_NONE != btError)
		{
			/* Adding update net key failed, delete the node,
			* Call observer API with appropriate info for subscription
			* response for keyRefresh
			*/
			ERROR(MSGID_MESH_PROFILE_ERROR, 0,
					"netKey update to:%d failed", node->getNumberOfElements());
			deleteRemoteNodeFromLocalKeyDatabase(node->getPrimaryElementAddress(),
											node->getNumberOfElements());
			mMesh->getMeshObserver()->keyRefreshResult(BLUETOOTH_ERROR_MESH_NETKEY_UPDATE_FAILED,
										convertAddressToLowerCase(mAdapter->getAddress()),
										netKeyIndex, status, 1, node->getPrimaryElementAddress());
			node = nodes.erase(node);
		}
		else
		{
			++node;
		}
	}
	DEBUG("Distributing netKeyIndex to remote nodes completed");
	if (refreshAppKeys)
	{
		// Refresh All keys is handled in service. service has the info
		// regarding all the appkeys created. So it sends all the app keys in
		// appKeyIndexesToRefresh parameter
		for (auto appKeyIndex : appKeyIndexesToRefresh)
		{
			DEBUG("appKeyIndex: %d", appKeyIndex);
			bluez_mesh_management1_call_update_app_key_sync(
				mMgmtInterface, appKeyIndex, NULL, &error);
			if (error)
			{
				// Generating new key for appIndex failed
				ERROR(MSGID_MESH_PROFILE_ERROR, 0,
					"Generating new key for appIndex:%d failed", appKeyIndex);
				mMesh->getMeshObserver()->keyRefreshResult(BLUETOOTH_ERROR_MESH_CANNOT_UPDATE_APPKEY,
										convertAddressToLowerCase(mAdapter->getAddress()),
										netKeyIndex, status, 1, appKeyIndex);

				g_error_free(error);
				error = NULL;
				continue;
			}
			btError = mElements[0].configSet(LOCAL_NODE_ADDRESS,
								"APPKEY_UPDATE", 0, netKeyIndex, appKeyIndex,
								0, 0, NULL, waitTime);
			if (BLUETOOTH_ERROR_NONE != btError)
			{
				mMesh->getMeshObserver()->keyRefreshResult(BLUETOOTH_ERROR_MESH_CANNOT_UPDATE_APPKEY,
						convertAddressToLowerCase(mAdapter->getAddress()),
						netKeyIndex, status, 1,
						LOCAL_NODE_ADDRESS, appKeyIndex);
			}
			DEBUG("Generating new app key for index : %d completed", appKeyIndex);
			for (auto node : nodes)
			{
				std::vector<uint16_t> appKeyIndexes = node.getAppKeyIndexes();
				if (appKeyIndexes.end() != std::find(appKeyIndexes.begin(),
						appKeyIndexes.end(), appKeyIndex))
				{
					DEBUG("Distributing appKeyIndex: %d to remote node: %d", appKeyIndex,
									node.getPrimaryElementAddress());
					btError = mElements[0].configSet(node.getPrimaryElementAddress(),
								"APPKEY_UPDATE", 0, netKeyIndex, appKeyIndex,
								0, 0, NULL, waitTime);
					if (BLUETOOTH_ERROR_NONE != btError)
					{
						mMesh->getMeshObserver()->keyRefreshResult(BLUETOOTH_ERROR_MESH_CANNOT_UPDATE_APPKEY,
								convertAddressToLowerCase(mAdapter->getAddress()),
								netKeyIndex, status, 1,
								node.getPrimaryElementAddress(), appKeyIndex);
					}
				}
			}
		}
	}
}

void Bluez5MeshAdv::setKeyRefreshPhase(uint16_t netKeyIndex, uint16_t phase, std::vector<BleMeshNode> nodes)
{
	GError *error = nullptr;
	std::string	status = "active";
	BluetoothError btError = BLUETOOTH_ERROR_NONE;

	bluez_mesh_management1_call_set_key_phase_sync(mMgmtInterface, netKeyIndex, phase, NULL, &error);
	if (error)
	{
		ERROR(MSGID_MESH_PROFILE_ERROR, 0, "Set key phase failed: %s", error->message);
		g_error_free(error);
		error = NULL;
		mMesh->getMeshObserver()->keyRefreshResult(BLUETOOTH_ERROR_FAIL,
										convertAddressToLowerCase(mAdapter->getAddress()),
										netKeyIndex, status, phase - 1);
	}
	else
	{
		mMesh->getMeshObserver()->keyRefreshResult(BLUETOOTH_ERROR_NONE,
										convertAddressToLowerCase(mAdapter->getAddress()),
										netKeyIndex, status, phase);
	}
	/* Set key phase in local node */
	mElements[0].configSet(LOCAL_NODE_ADDRESS,
								"KR_PHASE_SET", 0, netKeyIndex, 0,
								0, 0, NULL, 0, 0, phase);
	/* Set key phase in remote nodes */
	for (auto node : nodes)
	{
		btError = mElements[0].configSet(node.getPrimaryElementAddress(),
								"KR_PHASE_SET", 0, netKeyIndex, 0,
								0, 0, NULL, 0, 0, phase);
		if (BLUETOOTH_ERROR_NONE != btError)
		{
			ERROR(MSGID_MESH_PROFILE_ERROR, 0, "Set key phase failed for node: %d",
					node.getPrimaryElementAddress());
		}
	}
}

void Bluez5MeshAdv::keyRefresh(BluetoothResultCallback callback,
	bool refreshAppKeys, std::vector<uint16_t> appKeyIndexesToRefresh,
	std::vector<uint16_t> blackListedNodes,
	std::vector<BleMeshNode> nodes, uint16_t netKeyIndex, int32_t waitTime)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	GError *error = 0;

	std::thread t( [this, callback, refreshAppKeys, appKeyIndexesToRefresh,
									blackListedNodes, nodes, netKeyIndex, waitTime]() mutable
	{
		std::string status = "idle";

		GError *error = 0;

		/* Start of phase 1 */
		/* Update netKey in the provisioner */
		bluez_mesh_management1_call_update_subnet_sync(mMgmtInterface, netKeyIndex,
		NULL, &error);
		if (error)
		{
			ERROR(MSGID_MESH_PROFILE_ERROR, 0, "devKeySend failed: %s", error->message);
			if (strstr(error->message, "Does not exist")) // Wrong net key
			{
				callback(BLUETOOTH_ERROR_MESH_NET_KEY_INDEX_DOES_NOT_EXIST);
			}
			else
				callback(BLUETOOTH_ERROR_FAIL);
			g_error_free(error);
			error = NULL;
			return;
		}
		DEBUG("Updating netKeyIndex in provisioner completed");
		callback(BLUETOOTH_ERROR_NONE);
		mMesh->getMeshObserver()->keyRefreshResult(BLUETOOTH_ERROR_NONE,
										convertAddressToLowerCase(mAdapter->getAddress()),
										netKeyIndex, status, 0);

		status = "active";
		mMesh->getMeshObserver()->keyRefreshResult(BLUETOOTH_ERROR_NONE,
											convertAddressToLowerCase(mAdapter->getAddress()),
											netKeyIndex, status, 1);
		// Update network key in local node
		bluez_mesh_node1_call_add_net_key_sync(
			mNodeInterface, BLUEZ_MESH_ELEMENT_PATH, LOCAL_NODE_ADDRESS, 0, 0, true, NULL, &error);
		if (error)
		{
			// Adding local node to the network failed. this should never happen!!
			mMesh->getMeshObserver()->keyRefreshResult(BLUETOOTH_ERROR_MESH_NETKEY_UPDATE_FAILED,
											convertAddressToLowerCase(mAdapter->getAddress()),
											netKeyIndex, status, 1, LOCAL_NODE_ADDRESS);
			g_error_free(error);
			error = NULL;
		}
		DEBUG("Distributing netKeyIndex to local node completed");

		/* Remove blackilisted node from list of nodes so that keys are distributed to
		* nodes that are not blacklisted
		*/
		for (int i = 0 ; i < blackListedNodes.size(); ++i)
		{
			for (auto node = nodes.begin(); node != nodes.end();)
			{
				if (blackListedNodes[i] == node->getPrimaryElementAddress())
				{
					//delete the blacklisted node
					deleteRemoteNodeFromLocalKeyDatabase(node->getPrimaryElementAddress(), node->getNumberOfElements());
					node = nodes.erase(node);
				}
				else
				{
					++node;
				}
			}
		}

		distributeKeys(refreshAppKeys, appKeyIndexesToRefresh,
						nodes, netKeyIndex, waitTime);
		sleep(waitTime);
		/* Start of Phase 2 */
		setKeyRefreshPhase(netKeyIndex, 2, nodes);
		sleep(waitTime);
		/* Start of Phase 3 */
		setKeyRefreshPhase(netKeyIndex, 3, nodes);
		sleep(waitTime);
		/* Normal Operation */
		status = "completed";
		mMesh->getMeshObserver()->keyRefreshResult(BLUETOOTH_ERROR_NONE,
											convertAddressToLowerCase(mAdapter->getAddress()),
											netKeyIndex, status, 0);
	});
	t.detach();

	return;
}
