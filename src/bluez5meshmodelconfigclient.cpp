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

#include "bluez5meshmodel.h"
#include "utils.h"
#include "logging.h"
#include "asyncutils.h"
#include "bluez5profilemesh.h"
#include "bluez5meshadv.h"
#include "bluez5meshmodelconfigclient.h"
#include "bluez5adapter.h"
#include <cstdint>

#define BLUEZ_MESH_ELEMENT_PATH "/element"

#define ONE_SECOND 1000
#define RESPOND_WAIT_DURATION 2

#define DEFAULT_NET_KEY_INDEX 0x0000

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
#define NO_RESPONSE 0xFFFFFFFF

struct BleMeshConfigCmd {
	uint32_t opcode;
	uint32_t rsp;
	std::string desc;
};

static struct BleMeshConfigCmd cmds[] = {
	{OP_APPKEY_ADD, OP_APPKEY_STATUS, "APPKEY_ADD"},
	{OP_APPKEY_DELETE, OP_APPKEY_STATUS, "APPKEY_DELETE"},
	{OP_APPKEY_GET, OP_APPKEY_LIST, "APPKEYINDEX"},
	{OP_APPKEY_LIST, NO_RESPONSE, ""},
	{OP_APPKEY_STATUS, NO_RESPONSE, ""},
	{OP_APPKEY_UPDATE, OP_APPKEY_STATUS, ""},
	{OP_DEV_COMP_GET, OP_DEV_COMP_STATUS, "COMPOSITION_DATA"},
	{OP_DEV_COMP_STATUS, NO_RESPONSE, ""},
	{OP_CONFIG_BEACON_GET, OP_CONFIG_BEACON_STATUS, ""},
	{OP_CONFIG_BEACON_SET, OP_CONFIG_BEACON_STATUS, ""},
	{OP_CONFIG_BEACON_STATUS, NO_RESPONSE, ""},
	{OP_CONFIG_DEFAULT_TTL_GET, OP_CONFIG_DEFAULT_TTL_STATUS, "DEFAULT_TTL"},
	{OP_CONFIG_DEFAULT_TTL_SET, OP_CONFIG_DEFAULT_TTL_STATUS, "DEFAULT_TTL"},
	{OP_CONFIG_DEFAULT_TTL_STATUS, NO_RESPONSE, ""},
	{OP_CONFIG_FRIEND_GET, OP_CONFIG_FRIEND_STATUS, ""},
	{OP_CONFIG_FRIEND_SET, OP_CONFIG_FRIEND_STATUS, ""},
	{OP_CONFIG_FRIEND_STATUS, NO_RESPONSE, ""},
	{OP_CONFIG_PROXY_GET, OP_CONFIG_PROXY_STATUS, "GATT_PROXY"},
	{OP_CONFIG_PROXY_SET, OP_CONFIG_PROXY_STATUS, "GATT_PROXY"},
	{OP_CONFIG_PROXY_STATUS, NO_RESPONSE, ""},
	{OP_CONFIG_KEY_REFRESH_PHASE_GET, OP_CONFIG_KEY_REFRESH_PHASE_STATUS, ""},
	{OP_CONFIG_KEY_REFRESH_PHASE_SET, OP_CONFIG_KEY_REFRESH_PHASE_STATUS, ""},
	{OP_CONFIG_KEY_REFRESH_PHASE_STATUS, NO_RESPONSE, ""},
	{OP_CONFIG_MODEL_PUB_GET, OP_CONFIG_MODEL_PUB_STATUS, ""},
	{OP_CONFIG_MODEL_PUB_SET, OP_CONFIG_MODEL_PUB_STATUS, ""},
	{OP_CONFIG_MODEL_PUB_STATUS, NO_RESPONSE, ""},
	{OP_CONFIG_MODEL_PUB_VIRT_SET, OP_CONFIG_MODEL_PUB_STATUS, ""},
	{OP_CONFIG_MODEL_SUB_ADD, OP_CONFIG_MODEL_SUB_STATUS, ""},
	{OP_CONFIG_MODEL_SUB_DELETE, OP_CONFIG_MODEL_SUB_STATUS, ""},
	{OP_CONFIG_MODEL_SUB_DELETE_ALL, OP_CONFIG_MODEL_SUB_STATUS, ""},
	{OP_CONFIG_MODEL_SUB_OVERWRITE, OP_CONFIG_MODEL_SUB_STATUS, ""},
	{OP_CONFIG_MODEL_SUB_STATUS, NO_RESPONSE, ""},
	{OP_CONFIG_MODEL_SUB_VIRT_ADD, OP_CONFIG_MODEL_SUB_STATUS, ""},
	{OP_CONFIG_MODEL_SUB_VIRT_DELETE, OP_CONFIG_MODEL_SUB_STATUS, ""},
	{OP_CONFIG_MODEL_SUB_VIRT_OVERWRITE, OP_CONFIG_MODEL_SUB_STATUS, ""},
	{OP_CONFIG_NETWORK_TRANSMIT_GET, OP_CONFIG_NETWORK_TRANSMIT_STATUS, ""},
	{OP_CONFIG_NETWORK_TRANSMIT_SET, OP_CONFIG_NETWORK_TRANSMIT_STATUS, ""},
	{OP_CONFIG_NETWORK_TRANSMIT_STATUS, NO_RESPONSE, ""},
	{OP_CONFIG_RELAY_GET, OP_CONFIG_RELAY_STATUS, "RELAY"},
	{OP_CONFIG_RELAY_SET, OP_CONFIG_RELAY_STATUS, "RELAY"},
	{OP_CONFIG_RELAY_STATUS, NO_RESPONSE, ""},
	{OP_CONFIG_MODEL_SUB_GET, OP_CONFIG_MODEL_SUB_LIST, ""},
	{OP_CONFIG_MODEL_SUB_LIST, NO_RESPONSE, ""},
	{OP_CONFIG_VEND_MODEL_SUB_GET, OP_CONFIG_VEND_MODEL_SUB_LIST, ""},
	{OP_CONFIG_VEND_MODEL_SUB_LIST, NO_RESPONSE, ""},
	{OP_CONFIG_POLL_TIMEOUT_LIST, OP_CONFIG_POLL_TIMEOUT_STATUS, ""},
	{OP_CONFIG_POLL_TIMEOUT_STATUS, NO_RESPONSE, ""},
	{OP_CONFIG_HEARTBEAT_PUB_GET, OP_CONFIG_HEARTBEAT_PUB_STATUS, ""},
	{OP_CONFIG_HEARTBEAT_PUB_SET, OP_CONFIG_HEARTBEAT_PUB_STATUS, ""},
	{OP_CONFIG_HEARTBEAT_PUB_STATUS, NO_RESPONSE, ""},
	{OP_CONFIG_HEARTBEAT_SUB_GET, OP_CONFIG_HEARTBEAT_SUB_STATUS, ""},
	{OP_CONFIG_HEARTBEAT_SUB_SET, OP_CONFIG_HEARTBEAT_SUB_STATUS, ""},
	{OP_CONFIG_HEARTBEAT_SUB_STATUS, NO_RESPONSE, ""},
	{OP_MODEL_APP_BIND, OP_MODEL_APP_STATUS, "APPKEY_BIND"},
	{OP_MODEL_APP_STATUS, NO_RESPONSE, ""},
	{OP_MODEL_APP_UNBIND, OP_MODEL_APP_STATUS, "APPKEY_UNBIND"},
	{OP_NETKEY_ADD, OP_NETKEY_STATUS, ""},
	{OP_NETKEY_DELETE, OP_NETKEY_STATUS, ""},
	{OP_NETKEY_GET, OP_NETKEY_LIST, ""},
	{OP_NETKEY_LIST, NO_RESPONSE, ""},
	{OP_NETKEY_STATUS, NO_RESPONSE, ""},
	{OP_NETKEY_UPDATE, OP_NETKEY_STATUS, ""},
	{OP_NODE_IDENTITY_GET, OP_NODE_IDENTITY_STATUS, ""},
	{OP_NODE_IDENTITY_SET, OP_NODE_IDENTITY_STATUS, ""},
	{OP_NODE_IDENTITY_STATUS, NO_RESPONSE, ""},
	{OP_NODE_RESET, OP_NODE_RESET_STATUS, ""},
	{OP_NODE_RESET_STATUS, NO_RESPONSE, ""},
	{OP_MODEL_APP_GET, OP_MODEL_APP_LIST, ""},
	{OP_MODEL_APP_LIST, NO_RESPONSE, ""},
	{OP_VEND_MODEL_APP_GET, OP_VEND_MODEL_APP_LIST, ""},
	{OP_VEND_MODEL_APP_LIST, NO_RESPONSE, ""}};

Bluez5MeshModelConfigClient::Bluez5MeshModelConfigClient(uint32_t modelId,
								Bluez5ProfileMesh *meshProfile, Bluez5MeshAdv *meshAdv,
								Bluez5Adapter *adapter):
Bluez5MeshModel(modelId, meshProfile, meshAdv, adapter)
{
}

Bluez5MeshModelConfigClient::~Bluez5MeshModelConfigClient()
{
}

BluetoothError Bluez5MeshModelConfigClient::sendData(uint16_t srcAddress, uint16_t destAddress,
										 uint16_t appIndex, uint8_t data[])
{
	return BLUETOOTH_ERROR_UNSUPPORTED;
}

std::vector<BleMeshPendingRequest>::iterator Bluez5MeshModelConfigClient::getRequestFromResponse(uint32_t opcode,
											   uint16_t destAddr)
{
	DEBUG("%s::%d, destAddr: %d", __FUNCTION__, __LINE__ , destAddr);
	std::lock_guard<std::mutex> guard(pendingReqMutex);
	for (auto req = pendingRequests.begin();
			req != pendingRequests.end(); ++req)
	{
		DEBUG("resp: %d, opcode: %d, addr: %d", req->resp, opcode, req->addr);
		if (req->resp == opcode && req->addr == destAddr)
		{
			return req;
		}
	}
	return pendingRequests.end();
}

static gboolean pendingRequestTimerExpired(gpointer userdata)
{
	BleMeshPendingRequest *pendingReq = (BleMeshPendingRequest *)userdata;
	BleMeshConfiguration meshConfig;
	meshConfig.setConfig(pendingReq->desc);

	DEBUG("No response for: %s, destAddress: %d\n", pendingReq->desc.c_str(), pendingReq->addr);
	pendingReq->configClient->mMeshAdv->mMesh->getMeshObserver()->modelConfigResult(
		convertAddressToLowerCase(pendingReq->configClient->mMeshAdv->mAdapter->getAddress()),
		meshConfig, BLUETOOTH_ERROR_MESH_NO_RESPONSE_FROM_NODE);

	std::lock_guard<std::mutex> guard(pendingReq->configClient->pendingReqMutex);
	for (auto req = pendingReq->configClient->pendingRequests.begin();
			req != pendingReq->configClient->pendingRequests.end(); ++req)
	{
		if (req->req == pendingReq->req)
		{
			DEBUG("Erasing request");
			pendingReq->configClient->pendingRequests.erase(req);
			break;
		}
	}
	return false;
}

bool Bluez5MeshModelConfigClient::requestExists(uint32_t opcode,
											   uint16_t destAddr)
{
	std::lock_guard<std::mutex> guard(pendingReqMutex);
	for (auto req = pendingRequests.begin();
			req != pendingRequests.end(); ++req)
	{
		if ((*req).req == opcode && (*req).addr == destAddr)
		{
			return true;
		}
	}
	return false;
}

BluetoothError Bluez5MeshModelConfigClient::deletePendingRequest(uint32_t opcode,
											   uint16_t destAddr)
{
	DEBUG("%s::%d, destAddr: %d", __FUNCTION__, __LINE__ , destAddr);
	std::lock_guard<std::mutex> guard(pendingReqMutex);
	for (auto req = pendingRequests.begin();
			req != pendingRequests.end(); )
	{

		DEBUG("resp: %d, opcode: %d, addr: %d", req->resp, opcode, req->addr);
		if (req->resp == opcode && req->addr == destAddr)
		{
			DEBUG("Found the request in queue, deleting it");
			g_source_remove(req->timer);
			req = pendingRequests.erase(req);
			/* We continue checking because get and set may have been called
			 * on the same config and the response for both is same. */
		}
		else
		{
			++req;
		}
	}
	return BLUETOOTH_ERROR_NONE;
}

BluetoothError Bluez5MeshModelConfigClient::addPendingRequest(uint32_t opcode,
											   uint16_t destAddr)
{
	if (requestExists(opcode, destAddr))
	{
		return BLUETOOTH_ERROR_BUSY;
	}
	BleMeshPendingRequest pendingReq;
	for (int i = 0; i < (sizeof(cmds)/sizeof(cmds[0])); ++i)
	{
		if (opcode == cmds[i].opcode && NO_RESPONSE != cmds[i].rsp)
		{
			pendingReq.req = opcode;
			pendingReq.resp = cmds[i].rsp;
			pendingReq.desc = cmds[i].desc;
			pendingReq.addr = destAddr;
			pendingReq.configClient = this;
			pendingRequests.push_back(pendingReq);

			pendingRequests.back().timer =
				g_timeout_add(RESPOND_WAIT_DURATION * ONE_SECOND,
							  pendingRequestTimerExpired, &pendingRequests.back());

		}
	}

	return BLUETOOTH_ERROR_NONE;
}

uint32_t Bluez5MeshModelConfigClient::printModId(uint8_t *data, bool vendor, const char *offset)
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

const char * Bluez5MeshModelConfigClient::sigModelString(uint16_t sigModelId)
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

void Bluez5MeshModelConfigClient::compositionReceived(uint8_t *data, uint16_t len,
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

bool Bluez5MeshModelConfigClient::recvData(uint16_t srcAddress, uint16_t destAddress,
									uint16_t appIndex, uint8_t data[], uint32_t dataLen)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	int32_t n = 0;
	uint32_t opcode;
	bool opCodeHandled = false;
	BleMeshConfiguration configuration;

	if (meshOpcodeGet(data, dataLen, &opcode, &n))
	{
		dataLen -= n;
		data += n;
	}
	else
	{
		return false;
	}
	DEBUG("Opcode received: %x", opcode);
	auto req = getRequestFromResponse(opcode, srcAddress);
	if (req == pendingRequests.end())
	{
		return false;
	}
	configuration.setConfig(req->desc);
	deletePendingRequest(opcode, srcAddress);

	switch (opcode & ~OP_UNRELIABLE)
	{
		case OP_APPKEY_STATUS:
		{
			if (dataLen != 4)
				break;

			DEBUG("Node %4.4x AppKey status %x\n", srcAddress, data[0]);
			uint16_t net_idx = get_le16(data + 1) & 0xfff;
			uint16_t app_idx = get_le16(data + 2) >> 4;

			DEBUG("NetKey\t%3.3x\n", net_idx);
			DEBUG("AppKey\t%3.3x\n", app_idx);
			opCodeHandled = true;
			break;
		}
		case OP_APPKEY_LIST:
		{
			std::vector<uint16_t> appKeyList;
			if (dataLen < 3)
				break;

			DEBUG("AppKey List (node %4.4x) Status %d\n",
				srcAddress, data[0]);
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

			configuration.setAppKeyIndexes(appKeyList);
			opCodeHandled = true;
			break;
		}
		case OP_MODEL_APP_STATUS:
		{
			if (dataLen != 7 && dataLen != 9)
				break;

			DEBUG("Node %4.4x: Model App status %d\n", srcAddress, data[0]);
			uint16_t addr = get_le16(data + 1);
			uint16_t app_idx = get_le16(data + 3);

			DEBUG("Element Addr\t%4.4x\n", addr);

			uint32_t mod_id = get_le16(data + 5);
			DEBUG("Model ID\t %4.4x\n",mod_id);

			DEBUG("AppIdx\t\t%3.3x\n ", app_idx);
			opCodeHandled = true;
			break;
		}
		case OP_CONFIG_DEFAULT_TTL_STATUS:
		{
			if (dataLen != 1)
				break;

			DEBUG("Node %4.4x  Default TTL %d\n", srcAddress, data[0]);
			configuration.setTTL(data[0]);
			DEBUG("Config:%s\n",configuration.getConfig());
			opCodeHandled = true;
			break;
		}
		case OP_CONFIG_PROXY_STATUS:
		{
			if (dataLen != 1)
				break;
			configuration.setGattProxyState(data[0]);
			DEBUG("Node %4.4x  Proxy state 0x%02x\n", srcAddress, data[0]);
			opCodeHandled = true;
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

			configuration.setRelayStatus(relayStatus);

			DEBUG("Node %4.4x: Relay 0x%02x, cnt %d, steps %d\n",
				srcAddress, data[0], data[1] & 0x7, data[1] >> 3);
			opCodeHandled = true;
			break;
		}
		case OP_DEV_COMP_STATUS:
		{
			DEBUG("OP_DEV_COMP_STATUS");
			BleMeshCompositionData compositionData;
			compositionReceived(data, dataLen, compositionData);
			configuration.setCompositionData(compositionData);
			opCodeHandled = true;
			break;
		}
		default:
			DEBUG("Op code not handled");
	}

	if (opCodeHandled)
	{
		mMeshProfile->getMeshObserver()->modelConfigResult(convertAddressToLowerCase(mAdapter->getAddress()), configuration, BLUETOOTH_ERROR_NONE);
		return true;
	}
	return false;
}

BluetoothError Bluez5MeshModelConfigClient::getDefaultTTL(uint16_t destAddress, uint16_t netKeyIndex)
{
	uint8_t msg[32];
	uint16_t n;

	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	int32_t status = addPendingRequest(OP_CONFIG_DEFAULT_TTL_GET, destAddress);
	if (BLUETOOTH_ERROR_NONE != status)
	{
		return (BluetoothError)status;
	}

	n = meshOpcodeSet(OP_CONFIG_DEFAULT_TTL_GET, msg);

	return mMeshAdv->devKeySend(destAddress, netKeyIndex, msg, n);
}

BluetoothError Bluez5MeshModelConfigClient::getGATTProxy(uint16_t destAddress, uint16_t netKeyIndex)
{
	uint8_t msg[32];
	uint16_t n;

	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	int32_t status = addPendingRequest(OP_CONFIG_PROXY_GET, destAddress);
	if (BLUETOOTH_ERROR_NONE != status)
	{
		return (BluetoothError)status;
	}

	n = meshOpcodeSet(OP_CONFIG_PROXY_GET, msg);

	return mMeshAdv->devKeySend(destAddress, netKeyIndex, msg, n);
}

BluetoothError Bluez5MeshModelConfigClient::getRelay(uint16_t destAddress, uint16_t netKeyIndex)
{
	uint8_t msg[32];
	uint16_t n;

	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	int32_t status = addPendingRequest(OP_CONFIG_RELAY_GET, destAddress);
	if (BLUETOOTH_ERROR_NONE != status)
	{
		return (BluetoothError)status;
	}

	n = meshOpcodeSet(OP_CONFIG_RELAY_GET, msg);

	return mMeshAdv->devKeySend(destAddress, netKeyIndex, msg, n);
}

BluetoothError Bluez5MeshModelConfigClient::getAppKeyIndex(uint16_t destAddress, uint16_t netKeyIndex)
{
	uint8_t msg[32];
	uint16_t n;

	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	int32_t status = addPendingRequest(OP_APPKEY_GET, destAddress);
	if (BLUETOOTH_ERROR_NONE != status)
	{
		return (BluetoothError)status;
	}
	n = meshOpcodeSet(OP_APPKEY_GET, msg);

	put_le16(netKeyIndex, msg + n);
	n += 2;

	return mMeshAdv->devKeySend(destAddress, netKeyIndex, msg, n);
}

BluetoothError Bluez5MeshModelConfigClient::configGet(uint16_t destAddress,
								const std::string &config,
								uint16_t netKeyIndex)
{
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

	return BLUETOOTH_ERROR_PARAM_INVALID;
}

uint16_t Bluez5MeshModelConfigClient::putModelId(uint8_t *buf, uint32_t *args, bool vendor)
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

BluetoothError Bluez5MeshModelConfigClient::configBindAppKey(uint16_t destAddress,
												uint16_t netKeyIndex, uint16_t appKeyIndex, uint32_t modelId)
{
	uint16_t n;
	uint8_t msg[32];

	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	int32_t status = addPendingRequest(OP_MODEL_APP_BIND, destAddress);
	if (BLUETOOTH_ERROR_NONE != status)
	{
		return (BluetoothError)status;
	}
	n = meshOpcodeSet(OP_MODEL_APP_BIND, msg);

	put_le16(destAddress, msg + n);
	n += 2;
	put_le16(appKeyIndex, msg + n);
	n += 2;

	n += putModelId(msg + n, &modelId, false);

	return mMeshAdv->devKeySend(destAddress, netKeyIndex, msg, n);
}

BluetoothError Bluez5MeshModelConfigClient::setDefaultTTL(uint16_t destAddress, uint16_t netKeyIndex, uint8_t ttl)
{
	uint8_t msg[32];
	uint16_t n;

	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	if (ttl > TTL_MASK)
		return BLUETOOTH_ERROR_PARAM_INVALID;

	int32_t status = addPendingRequest(OP_CONFIG_DEFAULT_TTL_SET, destAddress);
	if (BLUETOOTH_ERROR_NONE != status)
	{
		return (BluetoothError)status;
	}
	n = meshOpcodeSet(OP_CONFIG_DEFAULT_TTL_SET, msg);
	msg[n++] = ttl;

	return mMeshAdv->devKeySend(destAddress, netKeyIndex, msg, n);
}

BluetoothError Bluez5MeshModelConfigClient::setGATTProxy(uint16_t destAddress, uint16_t netKeyIndex, uint8_t gattProxyState)
{
	uint8_t msg[2 + 1];
	uint16_t n;

	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	int32_t status = addPendingRequest(OP_CONFIG_PROXY_SET, destAddress);
	if (BLUETOOTH_ERROR_NONE != status)
	{
		return (BluetoothError)status;
	}
	n = meshOpcodeSet(OP_CONFIG_PROXY_SET, msg);
	msg[n++] = gattProxyState;

	return mMeshAdv->devKeySend(destAddress, netKeyIndex, msg, n);
}

BluetoothError Bluez5MeshModelConfigClient::setRelay(uint16_t destAddress, uint16_t netKeyIndex, BleMeshRelayStatus *relayStatus)
{
	uint8_t msg[2 + 2 + 4];
	uint16_t n;

	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	DEBUG("%d::%d::%d",relayStatus->getRelay(),relayStatus->getrelayRetransmitCount(), relayStatus->getRelayRetransmitIntervalSteps());
	int32_t status = addPendingRequest(OP_CONFIG_RELAY_SET, destAddress);
	if (BLUETOOTH_ERROR_NONE != status)
	{
		return (BluetoothError)status;
	}
	n = meshOpcodeSet(OP_CONFIG_RELAY_SET, msg);

	msg[n++] = relayStatus->getRelay();
	msg[n++] = relayStatus->getrelayRetransmitCount() | (relayStatus->getRelayRetransmitIntervalSteps() << 3);

	return mMeshAdv->devKeySend(destAddress, netKeyIndex, msg, n);
}

BluetoothError Bluez5MeshModelConfigClient::addAppKey(uint16_t destAddress,
											uint16_t netKeyIndex, uint16_t appKeyIndex, bool update)
{
	GError *error = 0;
	BluetoothError silError = BLUETOOTH_ERROR_FAIL;
	int32_t status;
	if (update)
	{
		status = addPendingRequest(OP_APPKEY_UPDATE, destAddress);
	}
	else
	{
		status = addPendingRequest(OP_APPKEY_ADD, destAddress);
	}
	if (BLUETOOTH_ERROR_NONE != status)
	{
		return (BluetoothError)status;
	}

	bluez_mesh_node1_call_add_app_key_sync(mMeshAdv->getBluezNodeInterface(), BLUEZ_MESH_ELEMENT_PATH,
										destAddress, appKeyIndex, netKeyIndex, update, NULL, &error);
	if (error)
	{
		ERROR(MSGID_MESH_PROFILE_ERROR, 0, "ConfigAppKey failed: %s", error->message);

		if (strstr(error->message, "AppKey not found"))
		{
			silError = BLUETOOTH_ERROR_MESH_APP_KEY_INDEX_DOES_NOT_EXIST;
		}
		else if (strstr(error->message, "Cannot update"))
		{
			silError = BLUETOOTH_ERROR_MESH_CANNOT_UPDATE_APPKEY;
		}
		g_error_free(error);
		mMeshAdv->stopReqTimer();
		return silError;
	}
	return BLUETOOTH_ERROR_NONE;
}

BluetoothError Bluez5MeshModelConfigClient::configAppKeyAdd(uint16_t destAddress,
												uint16_t netKeyIndex, uint16_t appKeyIndex)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	return addAppKey(destAddress, netKeyIndex, appKeyIndex, false);
}

BluetoothError Bluez5MeshModelConfigClient::configAppKeyUpdate(uint16_t destAddress,
												uint16_t netKeyIndex, uint16_t appKeyIndex)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	return addAppKey(destAddress, netKeyIndex, appKeyIndex, true);
}

BluetoothError Bluez5MeshModelConfigClient::configSet(
		uint16_t destAddress, const std::string &config,
		uint8_t gattProxyState, uint16_t netKeyIndex, uint16_t appKeyIndex,
		uint32_t modelId, uint8_t ttl, BleMeshRelayStatus *relayStatus)
{
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
	return BLUETOOTH_ERROR_PARAM_INVALID;
}

BluetoothError Bluez5MeshModelConfigClient::getCompositionData(uint16_t destAddress)
{
	uint16_t n;
	uint8_t msg[32];

	int32_t status = addPendingRequest(OP_DEV_COMP_GET, destAddress);
	if (BLUETOOTH_ERROR_NONE != status)
	{
		return (BluetoothError)status;
	}
	n = meshOpcodeSet(OP_DEV_COMP_GET, msg);

	/* By default, use page 0 */
	msg[n++] = 0;
	return mMeshAdv->devKeySend(destAddress, DEFAULT_NET_KEY_INDEX, msg, n);
}