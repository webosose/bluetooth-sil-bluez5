// Copyright (c) 2018 LG Electronics, Inc.
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

#ifndef BLUEZ5PROFILEOPP_H
#define BLUEZ5PROFILEOPP_H

#include <string>

#include <bluetooth-sil-api.h>
#include <bluez5obexprofilebase.h>
#include "bluez5profilebase.h"

class Bluez5Adapter;
class Bluez5Device;
class Bluez5ObexSession;
class Bluez5ObexTransfer;

class Bluez5ProfileOpp : public Bluez5ObexProfileBase,
                         public BluetoothOppProfile
{
public:
	Bluez5ProfileOpp(Bluez5Adapter *adapter);
	~Bluez5ProfileOpp();

	BluetoothOppTransferId pushFile(const std::string &address, const std::string &sourcePath, BluetoothOppTransferResultCallback callback);
	void cancelTransfer(BluetoothOppTransferId id, BluetoothResultCallback callback);
	void supplyTransferConfirmation(BluetoothOppTransferId transferId, bool accept, BluetoothResultCallback callback){}
	inline uint64_t nextTransferId() { return ++mTranfserIdCounter; }

private:
	uint64_t mTranfserIdCounter;
};

#endif
