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
#include "bluez5meshmodelonoffclient.h"
#include "bluez5profilemesh.h"
#include "bluez5meshadv.h"
#include "bluez5adapter.h"
#include <cstdint>

/* Generic onoff model opration status */
#define OP_GENERIC_ONOFF_GET				0x8201
#define OP_GENERIC_ONOFF_SET				0x8202
#define OP_GENERIC_ONOFF_SET_UNACK			0x8203
#define OP_GENERIC_ONOFF_STATUS				0x8204

Bluez5MeshModelOnOffClient::Bluez5MeshModelOnOffClient(uint32_t modelId,
                    Bluez5ProfileMesh *meshProfile, Bluez5MeshAdv *meshAdv,
                    Bluez5Adapter *adapter):
Bluez5MeshModel(modelId, meshProfile, meshAdv, adapter)
{
}

Bluez5MeshModelOnOffClient::~Bluez5MeshModelOnOffClient()
{
}

BluetoothError Bluez5MeshModelOnOffClient::sendData(uint16_t srcAddress, uint16_t destAddress,
                                         uint16_t appIndex, uint8_t data[])
{
	return BLUETOOTH_ERROR_UNSUPPORTED;
}

bool Bluez5MeshModelOnOffClient::recvData(uint16_t srcAddress, uint16_t destAddress,
                                            uint16_t appIndex, uint8_t data[], uint32_t dataLen)
{
	uint32_t opcode = 0;
	bool isMsgHandled = false;
	int32_t n = 0;
	bool onoff = false;

    if (meshOpcodeGet(data, dataLen, &opcode, &n))
	{
		dataLen -= n;
		data += n;
	}
	else
	{
		return true;
	}
	DEBUG("Opcode received: %x", opcode);
    switch (opcode & ~OP_UNRELIABLE)
	{
	    case OP_GENERIC_ONOFF_STATUS:
	    {
			if (dataLen != 1 && dataLen != 3)
				break;

			DEBUG("Node %4.4x: Off Status present = %s",
						srcAddress, data[0] ? "ON" : "OFF");
			onoff = data[0];

            isMsgHandled = true;
		    break;
	    }
	    default:
	    {
		    DEBUG("Op code not handled");
	    }
	}
	//Todo: check why not getting any msgCB from bluez/nodeDevice
	if(isMsgHandled)
	{
		//if onOFF cmd Status ack msg received
		mMeshProfile->getMeshObserver()->modelSetOnOffResult(
			convertAddressToLowerCase(mAdapter->getAddress()),
			onoff, BLUETOOTH_ERROR_NONE);
		return true;
	}
	return false;
}

BluetoothError Bluez5MeshModelOnOffClient::setOnOff(uint16_t destAddress, uint16_t appIndex, bool onoff)
{
	uint8_t msg[32];
	uint16_t n;
	n = meshOpcodeSet(OP_GENERIC_ONOFF_SET, msg);
	msg[n++] = onoff;
	msg[n++] = mMeshAdv->getTransactionId();
	return mMeshAdv->send(destAddress, appIndex, msg, n);
}