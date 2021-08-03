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

#ifndef BLUEZ5PROFILEMESH_H
#define BLUEZ5PROFILEMESH_H

#include <bluetooth-sil-api.h>
#include <bluez5profilebase.h>
#include <cstdint>
#include <string>

extern "C"
{
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5Adapter;
class Bluez5MeshAdv;

class Bluez5ProfileMesh : public Bluez5ProfileBase,
						  public BluetoothMeshProfile
{
public:
	/* Constructor and Destructor */
	Bluez5ProfileMesh(Bluez5Adapter *adapter);
	~Bluez5ProfileMesh();
	BluetoothMeshObserver* getMeshObserver() { return BluetoothMeshProfile::getMeshObserver(); }

	void getProperties(const std::string &address, BluetoothPropertiesResultCallback callback);
	void getProperty(const std::string &address, BluetoothProperty::Type type, BluetoothPropertyResultCallback callback);

	/* Mesh APIs */
	BluetoothError createNetwork(const std::string &bearer);
	BluetoothError attach(const std::string &bearer, const std::string &token);
	void getMeshInfo(const std::string &bearer, BleMeshInfoCallback callback);
	BluetoothError scanUnprovisionedDevices(const std::string &bearer, const uint16_t scanTimeout);
	BluetoothError unprovisionedScanCancel(const std::string &bearer);
	BluetoothError provision(const std::string &bearer, const std::string &uuid,
								const uint16_t timeout);
	BluetoothError supplyProvisioningNumeric(const std::string &bearer, uint32_t number);
	BluetoothError supplyProvisioningOob(const std::string &bearer, const std::string &oobData);
	BluetoothError getCompositionData(const std::string &bearer, uint16_t destAddress);
	BluetoothError createAppKey(const std::string &bearer, uint16_t netKeyIndex,
										uint16_t appKeyIndex);
	BluetoothError modelSend(const std::string &bearer, uint16_t srcAddress,
									uint16_t destAddress, uint16_t appKeyIndex,
									const std::string &command,
									 BleMeshPayload &payload);
	BluetoothError configGet(const std::string &bearer,
					uint16_t destAddress,
					const std::string &config,
					uint16_t netKeyIndex = 0);
	BluetoothError configSet(const std::string &bearer, uint16_t destAddress,
							 const std::string &config, uint8_t gattProxyState,
							 uint16_t netKeyIndex = 0, uint16_t appKeyIndex = 0,
							 uint32_t modelId = 0, uint8_t ttl = 0,
							 BleMeshRelayStatus *relayStatus = NULL);
	BluetoothError setOnOff(const std::string &bearer,
							uint16_t destAddress, uint16_t appKeyIndex, bool onoff, bool ack = false);
	BluetoothError updateNodeInfo(const std::string &bearer,
								std::vector<uint16_t> &unicastAddresses);
	BluetoothError deleteNode(const std::string &bearer, uint16_t destAddress, uint8_t count);

private:
	GDBusObjectManager *mObjectManager;
	Bluez5MeshAdv *mMeshAdv;
};

#endif //BLUEZ5PROFILEMESH_H