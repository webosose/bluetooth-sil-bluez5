// Copyright (c) 2014-2018 LG Electronics, Inc.
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

#include "bluez5profilebase.h"
#include "bluez5device.h"
#include "bluez5adapter.h"
#include "logging.h"

Bluez5ProfileBase::Bluez5ProfileBase(Bluez5Adapter *adapter, const std::string &uuid) :
	mAdapter(adapter),
	mUuid(uuid)
{
}

Bluez5ProfileBase::~Bluez5ProfileBase()
{
}

void Bluez5ProfileBase::connect(const std::string &address, BluetoothResultCallback callback)
{
	Bluez5Device *device = mAdapter->findDevice(address);
	if (!device)
	{
		callback(BLUETOOTH_ERROR_PARAM_INVALID);
		return;
	}

	device->connect(mUuid, callback);
}

void Bluez5ProfileBase::disconnect(const std::string &address, BluetoothResultCallback callback)
{
	Bluez5Device *device = mAdapter->findDevice(address);
	if (!device)
	{
		DEBUG("Could not find device with address %s while trying to disconnect", address.c_str());
		callback(BLUETOOTH_ERROR_PARAM_INVALID);
		return;
	}

	device->disconnect(mUuid, callback);
}

void Bluez5ProfileBase::updateConnectionStatus(const std::string &address, bool isConnected, const std::string &uuid)
{
	DEBUG("Bluez5ProfileBase::updateConnectionStatus address %s state %d uuid %s", address.c_str(), isConnected, uuid.c_str());
}
