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

#ifndef BLUEZ5MESHCONFIGCLIENT_H
#define BLUEZ5MESHCONFIGCLIENT_H

#include <bluetooth-sil-api.h>
#include <cstdint>
#include <mutex>

extern "C"
{
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5MeshModel;
class Bluez5MeshModelConfigClient;

class BleMeshKeyRefreshData
{
public:
	BleMeshKeyRefreshData():
	netKeyIndex(0),
	appKeyIndex(0),
	waitTime(0),
	numberOfElements(0) {}
	uint16_t netKeyIndex;
	uint16_t appKeyIndex;
	int32_t waitTime;
	int32_t numberOfElements;
};

class BleMeshPendingRequest
{
public:
	BleMeshPendingRequest():
	timer(0),
	req(0),
	resp(0),
	addr(0),
	count(0) {}
	guint timer;
	uint32_t req;
	uint32_t resp;
	uint16_t addr;
	uint8_t count;
	std::string desc;
	BleMeshKeyRefreshData keyRefreshData;
	Bluez5MeshModelConfigClient *configClient;
};

class Bluez5MeshModelConfigClient : public Bluez5MeshModel
{
public:
	std::vector<BleMeshPendingRequest *> pendingRequests;

	Bluez5MeshModelConfigClient(uint32_t modelId, Bluez5ProfileMesh *meshProfile,
								Bluez5MeshAdv *meshAdv, Bluez5Adapter *adapter);
	~Bluez5MeshModelConfigClient();
	BluetoothError sendData(uint16_t srcAddress, uint16_t destAddress,
							uint16_t appIndex, uint8_t data[]);
	bool recvData(uint16_t srcAddress, uint16_t destAddress, uint16_t appIndex,
					uint8_t data[], uint32_t dataLen);
	BluetoothError configGet(uint16_t destAddress,
								const std::string &config,
								uint16_t netKeyIndex);
	BluetoothError configSet(
		uint16_t destAddress, const std::string &config,
		uint8_t gattProxyState, uint16_t netKeyIndex, uint16_t appKeyIndex,
		uint32_t modelId, uint8_t ttl, BleMeshRelayStatus *relayStatus, int32_t waitTime = 0,
		int32_t numberOfElements = 1, uint8_t phase = 3);
	BluetoothError getCompositionData(uint16_t destAddress);
	BluetoothError deleteNode(uint16_t destAddress, uint8_t count);

private:
	const char * sigModelString(uint16_t sigModelId);
	uint32_t printModId(uint8_t *data, bool vendor, const char *offset);
	void compositionReceived(uint8_t *data, uint16_t len, BleMeshCompositionData &compositionData);
	BluetoothError getDefaultTTL(uint16_t destAddress, uint16_t netKeyIndex);
	BluetoothError getGATTProxy(uint16_t destAddress, uint16_t netKeyIndex);
	BluetoothError getRelay(uint16_t destAddress, uint16_t netKeyIndex);
	BluetoothError getAppKeyIndex(uint16_t destAddress, uint16_t netKeyIndex);
	BluetoothError configAppKeyAdd(uint16_t destAddress,
								   uint16_t netKeyIndex, uint16_t appKeyIndex);
	BluetoothError configAppKeyUpdate(uint16_t destAddress,
								   uint16_t netKeyIndex, uint16_t appKeyIndex,
									int32_t waitTime = 0);
	BluetoothError configBindAppKey(uint16_t destAddress,
									uint16_t netKeyIndex, uint16_t appKeyIndex, uint32_t modelId);
	BluetoothError setDefaultTTL(uint16_t destAddress, uint16_t netKeyIndex, uint8_t ttl);
	BluetoothError setGATTProxy(uint16_t destAddress, uint16_t netKeyIndex, uint8_t gattProxyState);
	BluetoothError setRelay(uint16_t destAddress, uint16_t netKeyIndex, BleMeshRelayStatus *relayStatus);
	uint16_t putModelId(uint8_t *buf, uint32_t *args, bool vendor);
	BluetoothError addAppKey(uint16_t destAddress,	uint16_t netKeyIndex,
								uint16_t appKeyIndex, bool update, int32_t waitTime = 0);
	BluetoothError addPendingRequest(uint32_t opcode, uint16_t destAddr, uint8_t count = 0);
	BluetoothError addPendingRequest(uint32_t opcode, uint16_t destAddr, BleMeshKeyRefreshData keyRefreshData);
	bool requestExists(uint32_t opcode, uint16_t destAddr);
	BluetoothError deletePendingRequest(uint32_t opcode, uint16_t destAddr);
	std::vector<BleMeshPendingRequest *>::iterator getRequestFromResponse(uint32_t opcode,
											uint16_t destAddr);
	BluetoothError configAppKeyDelete(uint16_t destAddress, uint16_t netKeyIndex,
										uint16_t appKeyIndex);
	BluetoothError configUnBindAppKey(uint16_t destAddress, uint16_t netKeyIndex,
										uint16_t appKeyIndex, uint32_t modelId);
	BluetoothError configNetKeyUpdate(uint16_t destAddress,
										uint16_t netKeyIndex, int32_t waitTime,
										int32_t numberOfElements);
	BluetoothError configKrPhaseSet(uint16_t destAddress,
			uint16_t netKeyIndex, uint8_t phase);

public:
	/* Mutex used to synchronise the access to pending request Q */
	std::mutex pendingReqMutex;
};

#endif //BLUEZ5MESHCONFIGCLIENT_H