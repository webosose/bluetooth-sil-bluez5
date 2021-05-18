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

#ifndef BLUEZ5MESHMODEL_H
#define BLUEZ5MESHMODEL_H

#include <bluetooth-sil-api.h>
#include <cstdint>

extern "C"
{
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5ProfileMesh;
class Bluez5MeshAdv;
class Bluez5Adapter;

class Bluez5MeshModel
{
public:
	Bluez5MeshModel(uint32_t modelId, Bluez5ProfileMesh *meshProfile, Bluez5MeshAdv *meshAdv,
						Bluez5Adapter *adapter):
	mModelId(modelId),
	mMeshProfile(meshProfile),
	mMeshAdv(meshAdv),
	mAdapter(adapter) {}
	virtual ~Bluez5MeshModel() {}
	virtual BluetoothError sendData(uint16_t srcAddress, uint16_t destAddress,
									uint16_t appIndex, uint8_t data[])
									{ return BLUETOOTH_ERROR_UNSUPPORTED; }
	virtual bool recvData(uint16_t srcAddress, uint16_t destAddress, uint16_t appIndex,
							uint8_t data[], uint32_t datalen) { return false; }

public:
	uint32_t mModelId;
	Bluez5ProfileMesh *mMeshProfile;
	Bluez5MeshAdv *mMeshAdv;
	Bluez5Adapter *mAdapter;
};

#endif //BLUEZ5MESHMODEL_H