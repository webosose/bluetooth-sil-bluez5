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

class Bluez5MeshModel
{
public:
	Bluez5MeshModel(uint32_t modelId) { mModelId = modelId; }
	~Bluez5MeshModel() {}
	virtual BluetoothError sendData(uint16_t srcAddress, uint16_t destAddress,
									uint16_t appIndex, uint8_t data[])
									{ return BLUETOOTH_ERROR_UNSUPPORTED; }
	virtual void recvData(uint16_t srcAddress, uint16_t destAddress, uint16_t appIndex, uint8_t data) {}

public:
	uint32_t mModelId;
};

#endif //BLUEZ5MESHMODEL_H