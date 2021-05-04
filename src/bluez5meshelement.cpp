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

#include "bluez5meshelement.h"
#include "bluez5meshmodel.h"
#include "bluez5profilemesh.h"
#include "bluez5adapter.h"
#include "utils.h"
#include "logging.h"
#include "asyncutils.h"
#include "bluez5meshmodelconfigclient.h"
#include "bluez5meshmodelonoffclient.h"
#include "utils_mesh.h"
#include "bluez5meshadv.h"
#include "bluez5profilemesh.h"
#include "bluez5adapter.h"
#include <cstdint>
#include <vector>

#define CONFIG_CLIENT_MODEL_ID 0001
#define GENERIC_ONOFF_CLIENT_MODEL_ID 1001
#define BLUEZ_MESH_ELEMENT_PATH "/element"

//TODO: Move the following status opcodes to appropriate location. And handle the
//message received appropriately.
/* Config model operation status */

#define OP_APPKEY_ADD				0x00
#define OP_APPKEY_DELETE			0x8000
#define OP_APPKEY_GET				0x8001
#define OP_APPKEY_LIST				0x8002
#define OP_APPKEY_STATUS			0x8003
#define OP_APPKEY_UPDATE			0x01
#define OP_DEV_COMP_GET				0x8008
#define OP_DEV_COMP_STATUS			0x02
#define OP_CONFIG_BEACON_GET			0x8009
#define OP_CONFIG_BEACON_SET			0x800A
#define OP_CONFIG_BEACON_STATUS			0x800B
#define OP_CONFIG_DEFAULT_TTL_GET		0x800C
#define OP_CONFIG_DEFAULT_TTL_SET		0x800D
#define OP_CONFIG_DEFAULT_TTL_STATUS		0x800E
#define OP_CONFIG_FRIEND_GET			0x800F
#define OP_CONFIG_FRIEND_SET			0x8010
#define OP_CONFIG_FRIEND_STATUS			0x8011
#define OP_CONFIG_PROXY_GET			0x8012
#define OP_CONFIG_PROXY_SET			0x8013
#define OP_CONFIG_PROXY_STATUS			0x8014
#define OP_CONFIG_KEY_REFRESH_PHASE_GET		0x8015
#define OP_CONFIG_KEY_REFRESH_PHASE_SET		0x8016
#define OP_CONFIG_KEY_REFRESH_PHASE_STATUS	0x8017
#define OP_CONFIG_MODEL_PUB_GET			0x8018
#define OP_CONFIG_MODEL_PUB_SET			0x03
#define OP_CONFIG_MODEL_PUB_STATUS		0x8019
#define OP_CONFIG_MODEL_PUB_VIRT_SET		0x801A
#define OP_CONFIG_MODEL_SUB_ADD			0x801B
#define OP_CONFIG_MODEL_SUB_DELETE		0x801C
#define OP_CONFIG_MODEL_SUB_DELETE_ALL		0x801D
#define OP_CONFIG_MODEL_SUB_OVERWRITE		0x801E
#define OP_CONFIG_MODEL_SUB_STATUS		0x801F
#define OP_CONFIG_MODEL_SUB_VIRT_ADD		0x8020
#define OP_CONFIG_MODEL_SUB_VIRT_DELETE		0x8021
#define OP_CONFIG_MODEL_SUB_VIRT_OVERWRITE	0x8022
#define OP_CONFIG_NETWORK_TRANSMIT_GET		0x8023
#define OP_CONFIG_NETWORK_TRANSMIT_SET		0x8024
#define OP_CONFIG_NETWORK_TRANSMIT_STATUS	0x8025
#define OP_CONFIG_RELAY_GET			0x8026
#define OP_CONFIG_RELAY_SET			0x8027
#define OP_CONFIG_RELAY_STATUS			0x8028
#define OP_CONFIG_MODEL_SUB_GET			0x8029
#define OP_CONFIG_MODEL_SUB_LIST		0x802A
#define OP_CONFIG_VEND_MODEL_SUB_GET		0x802B
#define OP_CONFIG_VEND_MODEL_SUB_LIST		0x802C
#define OP_CONFIG_POLL_TIMEOUT_LIST		0x802D
#define OP_CONFIG_POLL_TIMEOUT_STATUS		0x802E
#define OP_CONFIG_HEARTBEAT_PUB_GET		0x8038
#define OP_CONFIG_HEARTBEAT_PUB_SET		0x8039
#define OP_CONFIG_HEARTBEAT_PUB_STATUS		0x06
#define OP_CONFIG_HEARTBEAT_SUB_GET		0x803A
#define OP_CONFIG_HEARTBEAT_SUB_SET		0x803B
#define OP_CONFIG_HEARTBEAT_SUB_STATUS		0x803C
#define OP_MODEL_APP_BIND			0x803D
#define OP_MODEL_APP_STATUS			0x803E
#define OP_MODEL_APP_UNBIND			0x803F
#define OP_NETKEY_ADD				0x8040
#define OP_NETKEY_DELETE			0x8041
#define OP_NETKEY_GET				0x8042
#define OP_NETKEY_LIST				0x8043
#define OP_NETKEY_STATUS			0x8044
#define OP_NETKEY_UPDATE			0x8045
#define OP_NODE_IDENTITY_GET			0x8046
#define OP_NODE_IDENTITY_SET			0x8047
#define OP_NODE_IDENTITY_STATUS			0x8048
#define OP_NODE_RESET				0x8049
#define OP_NODE_RESET_STATUS			0x804A
#define OP_MODEL_APP_GET			0x804B
#define OP_MODEL_APP_LIST			0x804C
#define OP_VEND_MODEL_APP_GET			0x804D
#define OP_VEND_MODEL_APP_LIST			0x804E

/* Generic onoff model opration status */
#define OP_GENERIC_ONOFF_STATUS			0x8204

#define FEATURE_RELAY	1
#define FEATURE_PROXY	2
#define FEATURE_FRIEND	4
#define FEATURE_LPN	8

Bluez5MeshElement::Bluez5MeshElement(uint8_t elementIndex, Bluez5Adapter *adapter, Bluez5ProfileMesh *mesh) :
mElementIndex(elementIndex),
mAdapter(adapter),
mMeshProfile(mesh)
{

}

Bluez5MeshElement::~Bluez5MeshElement()
{
}

void Bluez5MeshElement::registerElementInterface(GDBusObjectManagerServer *objectManagerServer, Bluez5MeshAdv *meshAdv)
{
	GDBusObjectSkeleton *elementSkeleton = g_dbus_object_skeleton_new(BLUEZ_MESH_ELEMENT_PATH);
	BluezMeshElement1 *elementInterface = bluez_mesh_element1_skeleton_new();
	mMeshAdv = meshAdv;
	g_signal_connect(elementInterface, "handle_dev_key_message_received", G_CALLBACK(handleDevKeyMessageReceived), this);
	g_signal_connect(elementInterface, "handle_message_received", G_CALLBACK(handleMessageReceived), this);

	bluez_mesh_element1_set_index(elementInterface, mElementIndex);

	GVariantBuilder profileBuilder;
	GVariantBuilder prop;
	g_variant_builder_init(&profileBuilder, G_VARIANT_TYPE("a(qa{sv})"));

	for (int i = 0; i < mModels.size(); i++)
	{
		g_variant_builder_init(&prop, G_VARIANT_TYPE("a{sv}"));
		g_variant_builder_add (&prop, "{sv}", "Subscribe",g_variant_new("b", false));
		g_variant_builder_add (&prop, "{sv}", "Publish",g_variant_new("b", false));
		g_variant_builder_add (&profileBuilder, "(qa{sv})", (guint16)mModels[i]->mModelId, &prop);
	}
	bluez_mesh_element1_set_models(elementInterface, g_variant_new ("a(qa{sv})", &profileBuilder));

	g_dbus_object_skeleton_add_interface(elementSkeleton, G_DBUS_INTERFACE_SKELETON(elementInterface));
	g_dbus_object_manager_server_export(objectManagerServer, elementSkeleton);
}

bool Bluez5MeshElement::meshOpcodeGet(const uint8_t *buf, uint16_t sz, uint32_t *opcode, int *n)
{
	if (!n || !opcode || sz < 1)
		return false;

	switch (buf[0] & 0xc0) {
	case 0x00:
	case 0x40:
		/* RFU */
		if (buf[0] == 0x7f)
			return false;

		*n = 1;
		*opcode = buf[0];
		break;

	case 0x80:
		if (sz < 2)
			return false;

		*n = 2;
		*opcode = get_be16(buf);
		break;

	case 0xc0:
		if (sz < 3)
			return false;

		*n = 3;
		*opcode = get_be16(buf + 1);
		*opcode |= buf[0] << 16;
		break;

	default:
		DEBUG("Bad opcode");
		return false;
	}

	return true;
}

uint32_t Bluez5MeshElement::printModId(uint8_t *data, bool vendor, const char *offset)
{
	uint32_t mod_id;

	if (!vendor) {
		mod_id = get_le16(data);
		DEBUG("%sModel ID\t%4.4x \"%s\"\n",
				offset, mod_id, sigModelString(mod_id));
		mod_id = VENDOR_ID_MASK | mod_id;
	} else {
		mod_id = get_le16(data + 2);
		DEBUG("%sModel ID\t%4.4x %4.4x\n", offset,
							get_le16(data), mod_id);
		mod_id = get_le16(data) << 16 | mod_id;
	}

	return mod_id;
}

const char * Bluez5MeshElement::sigModelString(uint16_t sigModelId)
{
	switch (sigModelId) {
	case 0x0000: return "Configuration Server";
	case 0x0001: return "Configuration Client";
	case 0x0002: return "Health Server";
	case 0x0003: return "Health Client";
	case 0x1000: return "Generic OnOff Server";
	case 0x1001: return "Generic OnOff Client";
	case 0x1002: return "Generic Level Server";
	case 0x1003: return "Generic Level Client";
	case 0x1004: return "Generic Default Transition Time Server";
	case 0x1005: return "Generic Default Transition Time Client";
	case 0x1006: return "Generic Power OnOff Server";
	case 0x1007: return "Generic Power OnOff Setup Server";
	case 0x1008: return "Generic Power OnOff Client";
	case 0x1009: return "Generic Power Level Server";
	case 0x100A: return "Generic Power Level Setup Server";
	case 0x100B: return "Generic Power Level Client";
	case 0x100C: return "Generic Battery Server";
	case 0x100D: return "Generic Battery Client";
	case 0x100E: return "Generic Location Server";
	case 0x100F: return "Generic Location Setup Server";
	case 0x1010: return "Generic Location Client";
	case 0x1011: return "Generic Admin Property Server";
	case 0x1012: return "Generic Manufacturer Property Server";
	case 0x1013: return "Generic User Property Server";
	case 0x1014: return "Generic Client Property Server";
	case 0x1015: return "Generic Property Client";
	case 0x1100: return "Sensor Server";
	case 0x1101: return "Sensor Setup Server";
	case 0x1102: return "Sensor Client";
	case 0x1200: return "Time Server";
	case 0x1201: return "Time Setup Server";
	case 0x1202: return "Time Client";
	case 0x1203: return "Scene Server";
	case 0x1204: return "Scene Setup Server";
	case 0x1205: return "Scene Client";
	case 0x1206: return "Scheduler Server";
	case 0x1207: return "Scheduler Setup Server";
	case 0x1208: return "Scheduler Client";
	case 0x1300: return "Light Lightness Server";
	case 0x1301: return "Light Lightness Setup Server";
	case 0x1302: return "Light Lightness Client";
	case 0x1303: return "Light CTL Server";
	case 0x1304: return "Light CTL Setup Server";
	case 0x1305: return "Light CTL Client";
	case 0x1306: return "Light CTL Temperature Server";
	case 0x1307: return "Light HSL Server";
	case 0x1308: return "Light HSL Setup Server";
	case 0x1309: return "Light HSL Client";
	case 0x130A: return "Light HSL Hue Server";
	case 0x130B: return "Light HSL Saturation Server";
	case 0x130C: return "Light xyL Server";
	case 0x130D: return "Light xyL Setup Server";
	case 0x130E: return "Light xyL Client";
	case 0x130F: return "Light LC Server";
	case 0x1310: return "Light LC Setup Server";
	case 0x1311: return "Light LC Client";

	default: return "Unknown";
	}
}

void Bluez5MeshElement::compositionReceived(uint8_t *data, uint16_t len,
											BleMeshCompositionData &compositionData)
{
	uint16_t features;
	int i = 0;

	DEBUG("Received composion:\n");

	/* skip page -- We only support Page Zero */
	data++;
	len--;

	DEBUG("\tCID: %4.4x", get_le16(&data[0]));
	DEBUG("\tPID: %4.4x", get_le16(&data[2]));
	DEBUG("\tVID: %4.4x", get_le16(&data[4]));
	DEBUG("\tCRPL: %4.4x", get_le16(&data[6]));

	compositionData.setCompanyId(get_le16(&data[0]));
	compositionData.setProductId(get_le16(&data[2]));
	compositionData.setVersionId(get_le16(&data[4]));
	compositionData.setNumRplEnteries(get_le16(&data[6]));

	features = get_le16(&data[8]);
	data += 10;
	len -= 10;

	DEBUG("\tFeature support:\n");
	DEBUG("\t\trelay: %s\n", (features & FEATURE_RELAY) ?
								"yes" : "no");
	DEBUG("\t\tproxy: %s\n", (features & FEATURE_PROXY) ?
								"yes" : "no");
	DEBUG("\t\tfriend: %s\n", (features & FEATURE_FRIEND) ?
								"yes" : "no");
	DEBUG("\t\tlpn: %s\n", (features & FEATURE_LPN) ?
								"yes" : "no");

	BleMeshFeature meshFeatures(features & FEATURE_RELAY, features & FEATURE_PROXY,
								features & FEATURE_FRIEND, features & FEATURE_LPN);

	compositionData.setFeatures(meshFeatures);
	std::vector<BleMeshElement> elements;

	while (len) {
		BleMeshElement element;
		uint8_t m, v;

		DEBUG("\t Element %d:\n", i);
		DEBUG("\t\tlocation: %4.4x\n", get_le16(data));
		element.setLoc(get_le16(data));
		data += 2;
		len -= 2;

		m = *data++;
		v = *data++;
		len -= 2;
		element.setNumS(m);
		element.setNumV(v);
		std::vector<uint32_t> sigModId;
		std::vector<uint32_t> vendorModId;

		if (m)
			DEBUG("\t\tSIG defined models:\n");

		while (len >= 2 && m--) {
			printModId(data, false, "\t\t  ");
			uint32_t modId = get_le16(data);
			sigModId.push_back(modId);
			data += 2;
			len -= 2;
		}
		element.setSigModelIds(sigModId);

		if (v)
			DEBUG("\t\t Vendor defined models:\n");

		while (len >= 4 && v--) {
			printModId(data, true, "\t\t  ");
			uint32_t modId = get_le16(data);
			modId = get_le16(data) << 16 | modId;
			vendorModId.push_back(modId);
			data += 4;
			len -= 4;
		}
		element.setVendorModelIds(vendorModId);
		elements.push_back(element);
		i++;
	}
	DEBUG("Elements size: %d", elements.size());
	compositionData.setElements(elements);
}

gboolean Bluez5MeshElement::handleDevKeyMessageReceived(BluezMeshElement1 *object,
														GDBusMethodInvocation *invocation,
														guint16 argSource,
														gboolean argRemote,
														guint16 argNetIndex,
														GVariant *argData,
														gpointer userData)
{
	DEBUG("handleDevKeyMessageReceived: src: %d, remote: %d, netIndex: %d",
		  argSource, argRemote, argNetIndex);
	Bluez5MeshElement *meshElement = (Bluez5MeshElement *)userData;
	uint8_t *data;
	gsize dataLen = 0;
	int32_t n = 0;
	uint32_t opcode;

	BleMeshConfiguration configuration;
	meshElement->mMeshAdv->stopReqTimer();
	data = (uint8_t *)g_variant_get_fixed_array(argData, &dataLen, sizeof(guint8));

	DEBUG("Received msg with length: %d", dataLen);

	if (meshElement->meshOpcodeGet(data, dataLen, &opcode, &n))
	{
		dataLen -= n;
		data += n;
	}
	else
	{
		return true;
	}
	DEBUG("Opcode received: %x", opcode);

	switch (opcode & ~OP_UNRELIABLE)
	{
		case OP_APPKEY_STATUS:
		{
			if (dataLen != 4)
				break;

			DEBUG("Node %4.4x AppKey status %x\n", argSource, data[0]);
			uint16_t net_idx = get_le16(data + 1) & 0xfff;
			uint16_t app_idx = get_le16(data + 2) >> 4;

			DEBUG("NetKey\t%3.3x\n", net_idx);
			DEBUG("AppKey\t%3.3x\n", app_idx);
			configuration.setConfig("APPKEY_ADD");
			break;
		}
		case OP_APPKEY_LIST:
		{
			std::vector<uint16_t> appKeyList;
			if (dataLen < 3)
				break;

			DEBUG("AppKey List (node %4.4x) Status %d\n",
				argSource, data[0]);
			DEBUG("NetKey %3.3x\n", get_le16(&data[1]));
			dataLen -= 3;

			if (data[0] != MESH_STATUS_SUCCESS)
				break;

			DEBUG("AppKeys:\n");
			data += 3;

			while (dataLen >= 3)
			{
				DEBUG("\t%3.3x\n", get_le16(data) & 0xfff);
				DEBUG("\t%3.3x\n", get_le16(data + 1) >> 4);
				appKeyList.push_back(get_le16(data) & 0xfff);
				appKeyList.push_back(get_le16(data + 1) >> 4);
				dataLen -= 3;
				data += 3;
			}

			if (dataLen == 2)
			{
				DEBUG("\t%3.3x\n", get_le16(data));
				appKeyList.push_back(get_le16(data));
			}

			configuration.setConfig("APPKEYINDEX");
			configuration.setAppKeyIndexes(appKeyList);
			break;
		}
		case OP_MODEL_APP_STATUS:
		{
			if (dataLen != 7 && dataLen != 9)
				break;

			DEBUG("Node %4.4x: Model App status %d\n", argSource, data[0]);
			uint16_t addr = get_le16(data + 1);
			uint16_t app_idx = get_le16(data + 3);

			DEBUG("Element Addr\t%4.4x\n", addr);

			uint32_t mod_id = get_le16(data + 5);
			DEBUG("Model ID\t %4.4x\n",mod_id);

			DEBUG("AppIdx\t\t%3.3x\n ", app_idx);
			configuration.setConfig("APPKEY_BIND");
			break;
		}
		case OP_CONFIG_DEFAULT_TTL_STATUS:
		{
			if (dataLen != 1)
				break;

			DEBUG("Node %4.4x  Default TTL %d\n", argSource, data[0]);
			configuration.setConfig("DEFAULT_TTL");
			configuration.setTTL(data[0]);
			DEBUG("Config:%s\n",configuration.getConfig());
			break;
		}
		case OP_CONFIG_PROXY_STATUS:
		{
			if (dataLen != 1)
				break;
			configuration.setConfig("GATT_PROXY");
			configuration.setGattProxyState(data[0]);
			DEBUG("Node %4.4x  Proxy state 0x%02x\n", argSource, data[0]);
			break;
		}
		case OP_CONFIG_RELAY_STATUS:
		{
			if (dataLen != 2)
				break;

			BleMeshRelayStatus relayStatus;
			relayStatus.setRelay(data[0]);
			relayStatus.setRelayRetransmitCount(data[1] & 0x7);
			relayStatus.setRelayRetransmitIntervalSteps(data[1] >> 3);

			configuration.setConfig("RELAY");
			configuration.setRelayStatus(relayStatus);

			DEBUG("Node %4.4x: Relay 0x%02x, cnt %d, steps %d\n",
				argSource, data[0], data[1] & 0x7, data[1] >> 3);
			break;
		}
		case OP_DEV_COMP_STATUS:
		{
			DEBUG("OP_DEV_COMP_STATUS");
			BleMeshCompositionData compositionData;
			meshElement->compositionReceived(data, dataLen, compositionData);
			configuration.setConfig("COMPOSITION_DATA");
			configuration.setCompositionData(compositionData);
			break;
		}
		case OP_GENERIC_ONOFF_STATUS:
		{
			if (dataLen != 1 && dataLen != 3)
				break;

			DEBUG("Node %4.4x: Off Status present = %s",
						argSource, data[0] ? "ON" : "OFF");
			configuration.setOnOffState(data[0]);

			//if onOFF cmd Status ack msg received
			meshElement->mMeshProfile->getMeshObserver()->modelSetOnOffResult(
				convertAddressToLowerCase(meshElement->mAdapter->getAddress()), configuration.getOnOffState(), BLUETOOTH_ERROR_NONE);
			break;
		}
		default:
			DEBUG("Op code not handled");
	}

	meshElement->mMeshProfile->getMeshObserver()->modelConfigResult(convertAddressToLowerCase(meshElement->mAdapter->getAddress()), configuration, BLUETOOTH_ERROR_NONE);
	//Todo:
	meshElement->mMeshProfile->getMeshObserver()->modelDataReceived(convertAddressToLowerCase(meshElement->mAdapter->getAddress()),
				argSource, 0, 0xffff, data, dataLen);
	return true;
}

gboolean Bluez5MeshElement::handleMessageReceived(BluezMeshElement1 *object,
												  GDBusMethodInvocation *invocation,
												  guint16 argSource,
												  guint16 argKeyIndex,
												  GVariant *argDestination,
												  GVariant *argData,
												  gpointer userData)
{
	DEBUG("handleDevKeyMessageReceived: src: %d, appkeyIndex: %d",
		  argSource, argKeyIndex);
	Bluez5MeshElement *meshElement = (Bluez5MeshElement *)userData;
	uint8_t *data;
	gsize dataLen = 0;
	int32_t n = 0;
	uint32_t opcode;
	uint16_t destAddr = 0;

	data = (uint8_t *)g_variant_get_fixed_array(argData, &dataLen, sizeof(guint8));
	destAddr = g_variant_get_uint16 (argDestination);

	DEBUG("Received msg with length: %d", dataLen);

	if (meshElement->meshOpcodeGet(data, dataLen, &opcode, &n))
	{
		dataLen -= n;
		data += n;
	}
	else
	{
		return true;
	}
	DEBUG("Opcode received: %x", opcode);

	switch (opcode & ~OP_UNRELIABLE)
	{

	case OP_GENERIC_ONOFF_STATUS:
	{
		if (dataLen != 1 && dataLen != 3)
			break;

		DEBUG("Node %4.4x: Off Status present = %s",
			  argSource, data[0] ? "ON" : "OFF");

		if (dataLen == 3)
		{
			DEBUG(", target = %s",
				  data[1] ? "ON" : "OFF");
		}
		break;
	}
	default:
	{
		DEBUG("Op code not handled");
	}
	}
	//Todo: check why not getting any msgCB from bluez/nodeDevice
	meshElement->mMeshProfile->getMeshObserver()->modelDataReceived(convertAddressToLowerCase(meshElement->mAdapter->getAddress()),
				argSource, destAddr, argKeyIndex, data, dataLen);
	return true;
}

BluetoothError Bluez5MeshElement::addModel(uint32_t modelId)
{
	Bluez5MeshModel *model = nullptr;
	if (CONFIG_CLIENT_MODEL_ID == modelId)
	{
		model = new Bluez5MeshModelConfigClient(modelId);
	}
	else if (GENERIC_ONOFF_CLIENT_MODEL_ID == modelId)
	{
		model = new Bluez5MeshModelOnOffClient(modelId);
	}

	mModels.push_back(model);

	return BLUETOOTH_ERROR_NONE;
}
