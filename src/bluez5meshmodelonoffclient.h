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

#ifndef BLUEZ5MESHONOFFCLIENT_H
#define BLUEZ5MESHONOFFCLIENT_H

#include <bluetooth-sil-api.h>
#include <cstdint>

extern "C"
{
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5MeshModel;

class Bluez5MeshModelOnOffClient : public Bluez5MeshModel
{
public:
	Bluez5MeshModelOnOffClient(uint32_t modelId, Bluez5ProfileMesh *meshProfile,
								Bluez5MeshAdv *meshAdv, Bluez5Adapter *adapter);
	~Bluez5MeshModelOnOffClient();
	BluetoothError sendData(uint16_t srcAddress, uint16_t destAddress,
							uint16_t appIndex, uint8_t data[]);
    bool recvData(uint16_t srcAddress, uint16_t destAddress, uint16_t appIndex,
					uint8_t data[], uint32_t dataLen);
	BluetoothError setOnOff(uint16_t destAddress, uint16_t appIndex, bool onoff);

};
#endif //BLUEZ5MESHONOFFCLIENT_H