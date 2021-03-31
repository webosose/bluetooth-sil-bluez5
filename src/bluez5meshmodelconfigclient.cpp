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
#include "bluez5meshmodelconfigclient.h"
#include <cstdint>

Bluez5MeshModelConfigClient::Bluez5MeshModelConfigClient(uint32_t modelId):
Bluez5MeshModel(modelId)
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

void Bluez5MeshModelConfigClient::recvData(uint16_t srcAddress, uint16_t destAddress, uint16_t appIndex, uint8_t data)
{
}