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
	Bluez5MeshElement(uint8_t elementIndex, Bluez5Adapter *adapter, Bluez5ProfileMesh *mesh);
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

	void registerElementInterface(GDBusObjectManagerServer *objectManagerServer, Bluez5MeshAdv *meshAdv);
	BluetoothError addModel(uint32_t modelId);

private:
	bool meshOpcodeGet(const uint8_t *buf, uint16_t sz, uint32_t *opcode, int *n);
	const char * sigModelString(uint16_t sigModelId);
	uint32_t printModId(uint8_t *data, bool vendor, const char *offset);
	void compositionReceived(uint8_t *data, uint16_t len, BleMeshCompositionData &compositionData);

private:
	uint8_t mElementIndex;
	std::vector<Bluez5MeshModel *> mModels;
	Bluez5Adapter *mAdapter;
	Bluez5ProfileMesh *mMeshProfile;
	Bluez5MeshAdv *mMeshAdv;
};

#endif //BLUEZ5MESHELEMENT_H