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

#ifndef BLUEZ5MESHADV_H
#define BLUEZ5MESHADV_H

#include <bluetooth-sil-api.h>
#include <cstdint>
#include <string>
#include <vector>

extern "C"
{
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5Adapter;
class Bluez5ProfileMesh;
class Bluez5MeshElement;
class Bluez5MeshAdvProvisioner;
class Bluez5MeshAdvProvAgent;
class Bluez5MeshApplication;

class Bluez5MeshAdv
{
public:
	/* Constructor and Destructor */
	Bluez5MeshAdv(Bluez5ProfileMesh *mesh, Bluez5Adapter *adapter);
	~Bluez5MeshAdv();

	/* Dbus callback */
	static void handleBluezMeshServiceStarted(GDBusConnection *conn, const gchar *name, const gchar *nameOwner, gpointer user_data);
	static void handleBluezMeshServiceStopped(GDBusConnection *conn, const gchar *name, gpointer user_data);
	static void handleObjectAdded(GDBusObjectManager *objectManager, GDBusObject *object, void *userData);
	static void handleObjectRemoved(GDBusObjectManager *objectManager, GDBusObject *object, void *userData);
	static gboolean handleJoinComplete(BluezMeshApplication1 *object, GDBusMethodInvocation *invocation, guint64 argToken, gpointer userData);
	static gboolean handleJoinFailed(BluezMeshApplication1 *object, GDBusMethodInvocation *invocation, const gchar *argReason, gpointer userData);

	/* Mesh APIs */
	BluetoothError createNetwork();
	BluetoothError attachToken(const std::string &token);
	void getMeshInfo(BleMeshInfoCallback callback);
	BluetoothError scanUnprovisionedDevices(const uint16_t scanTimeout);
	BluetoothError unprovisionedScanCancel();
	BluetoothError provision(const std::string &uuid, const uint16_t timeout);
	BluetoothError supplyNumeric(uint32_t number);
	BluetoothError supplyStatic(const std::string &oobData);
	void getCompositionData(uint16_t destAddress,
							BleMeshCompositionDataCallback callback);
	BluetoothError createAppKey(uint16_t netKeyIndex, uint16_t appKeyIndex);
	BluetoothError modelSend(uint16_t srcAddress, uint16_t destAddress, uint16_t appKeyIndex,
									 const std::string &command,
									 BleMeshPayload payload);
	BluetoothError configAppKeyAdd(uint16_t srcAddress, uint16_t destAddress,
								   uint16_t netIndex, uint16_t appIndex);
	void configGet(BleMeshGetConfigCallback callback,
										uint16_t destAddress,
										const std::string &config,
										uint16_t netKeyIndex = 0);
	BluetoothError configSet(
		uint16_t destAddress, const std::string &config,
		uint8_t gattProxyState, uint16_t netKeyIndex = 0, uint16_t appKeyIndex = 0,
		uint32_t modelId = 0, uint8_t ttl = 0, BleMeshRelayStatus *relayStatus = NULL);
	BluetoothError setOnOff(uint16_t destAddress, uint16_t appIndex, bool onoff);

	BluetoothError registerElement(uint8_t index,
								std::vector<uint32_t> &sigModelIds,
								std::vector<uint32_t> &vendorModelIds);
	void attach();
	void updateNetworkId();

	private:
	void getRandomBytes(unsigned char *buf, int size);
	uint16_t meshOpcodeSet(uint32_t opcode, uint8_t *buf);
	uint16_t putModelId(uint8_t *buf, uint32_t *args, bool vendor);

private:
	GDBusConnection *mDbusConn;
	GDBusObjectManager *mObjectManager;
	BluezMeshNetwork1 *mNetworkInterface;
	BluezMeshManagement1 *mMgmtInterface;
	BluezMeshNode1 *mNodeInterface;
	Bluez5MeshAdvProvisioner *mMeshAdvProv;
	Bluez5MeshAdvProvAgent *mMeshAdvProvAgent;
	Bluez5MeshApplication *mMeshApplication;

	unsigned char mUuid[16]; //Local node uuid
	Bluez5ProfileMesh *mMesh;
	Bluez5Adapter *mAdapter;
	std::vector<Bluez5MeshElement> mElements;
	uint8_t mTransacId;

public:
	uint64_t mToken;
};

#endif //BLUEZ5MESHADV_H
