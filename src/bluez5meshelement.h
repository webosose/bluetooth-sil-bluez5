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

#ifndef BLUEZ5MESHELEMENT_H
#define BLUEZ5MESHELEMENT_H

#include <bluetooth-sil-api.h>
#include <cstdint>
#include <vector>

extern "C"
{
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5MeshModel;
class Bluez5Adapter;
class Bluez5ProfileMesh;
class Bluez5MeshAdv;

class Bluez5MeshElement
{
public:
	Bluez5MeshElement(uint8_t elementIndex, Bluez5Adapter *adapter,
						Bluez5ProfileMesh *mesh, Bluez5MeshAdv *meshAdv);
	~Bluez5MeshElement();

	static gboolean handleDevKeyMessageReceived(BluezMeshElement1 *object,
												GDBusMethodInvocation *invocation,
												guint16 argSource,
												gboolean argRemote,
												guint16 argNetIndex,
												GVariant *argData,
												gpointer userData);

	static gboolean handleMessageReceived(BluezMeshElement1 *object,
										  GDBusMethodInvocation *invocation,
										  guint16 argSource,
										  guint16 argKeyIndex,
										  GVariant *argDestination,
										  GVariant *argData,
										  gpointer userData);

	void registerElementInterface(GDBusObjectManagerServer *objectManagerServer);
	BluetoothError addModel(uint32_t modelId);

	/* Config client model's APIs */
	BluetoothError configGet(uint16_t destAddress,
								const std::string &config,
								uint16_t netKeyIndex);
	BluetoothError configSet(
		uint16_t destAddress, const std::string &config,
		uint8_t gattProxyState, uint16_t netKeyIndex = 0, uint16_t appKeyIndex = 0,
		uint32_t modelId = 0, uint8_t ttl = 0, BleMeshRelayStatus *relayStatus = NULL,
		int32_t waitTime = 0, int32_t numberOfElements = 1, uint8_t phase = 3);
	BluetoothError getCompositionData(uint16_t destAddress);
	BluetoothError deleteNode(uint16_t destAddress, uint8_t count);

	/* Generic onoff client model's APIs */
	BluetoothError setOnOff(uint16_t destAddress, uint16_t appIndex, bool onoff);

private:
	uint8_t mElementIndex;
	std::map<uint32_t, std::shared_ptr<Bluez5MeshModel>> mModels;
	Bluez5Adapter *mAdapter;
	Bluez5ProfileMesh *mMeshProfile;
	Bluez5MeshAdv *mMeshAdv;
};

#endif //BLUEZ5MESHELEMENT_H
