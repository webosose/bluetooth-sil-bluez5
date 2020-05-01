// Copyright (c) 2020 LG Electronics, Inc.
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

#include "bluez5profilehfp.h"
#include "bluez5adapter.h"
#include "logging.h"
#include "utils.h"
#include <glib-2.0/glib/gvariant.h>

const std::string BLUETOOTH_PROFILE_REMOTE_HFP_HF_UUID = "0000111e-0000-1000-8000-00805f9b34fb";
const std::string BLUETOOTH_PROFILE_REMOTE_HFP_AG_UUID = "0000111f-0000-1000-8000-00805f9b34fb";

Bluez5ProfileHfp::Bluez5ProfileHfp(Bluez5Adapter *adapter) :
	Bluez5ProfileBase(adapter, BLUETOOTH_PROFILE_REMOTE_HFP_AG_UUID)
{
}

Bluez5ProfileHfp::~Bluez5ProfileHfp()
{

}

void Bluez5ProfileHfp::getProperty(const std::string &address, BluetoothProperty::Type type,
	                         BluetoothPropertyResultCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	BluetoothProperty prop(type);
	Bluez5Device *device = mAdapter->findDevice(address);

	if (!device)
	{
		callback(BLUETOOTH_ERROR_PARAM_INVALID, prop);
		return;
	}

	auto it = std::find(mConnectedDevices.begin(), mConnectedDevices.end(), convertAddressToLowerCase(address));
	if (it != mConnectedDevices.end())
	{
		prop.setValue<bool>(true);
	}
	else
	{
		prop.setValue<bool>(false);
	}

	callback(BLUETOOTH_ERROR_NONE, prop);
}

void Bluez5ProfileHfp::connect(const std::string &address, BluetoothResultCallback callback)
{
	auto connectCallback = [this, address, callback](BluetoothError error) {
		if (error != BLUETOOTH_ERROR_NONE)
		{
			callback(error);
			return;
		}
		DEBUG("HFP connected succesfully");
		callback(BLUETOOTH_ERROR_NONE);
	};

	auto it = std::find(mConnectedDevices.begin(), mConnectedDevices.end(), convertAddressToLowerCase(address));
	if (it == mConnectedDevices.end())
		Bluez5ProfileBase::connect(address, connectCallback);
	else
		callback(BLUETOOTH_ERROR_DEVICE_ALREADY_CONNECTED);
}

void Bluez5ProfileHfp::disconnect(const std::string &address, BluetoothResultCallback callback)
{
	auto disConnectCallback = [this, address, callback](BluetoothError error) {
		if (error != BLUETOOTH_ERROR_NONE)
		{
			callback(error);
			return;
		}
		DEBUG("HFP disconnected succesfully");
		callback(BLUETOOTH_ERROR_NONE);
	};

	Bluez5ProfileBase::disconnect(address, disConnectCallback);
}

void Bluez5ProfileHfp::updateConnectionStatus(const std::string &address, bool isConnected, const std::string &uuid)
{
	BluetoothPropertiesList properties;
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::CONNECTED, isConnected));

	if (isConnected)
	{
		auto it = std::find(mConnectedDevices.begin(), mConnectedDevices.end(), convertAddressToLowerCase(address));
		if (it == mConnectedDevices.end())
			mConnectedDevices.push_back(convertAddressToLowerCase(address));
		else
		{
			DEBUG("Current status already connected, no need update");
			return;
		}
	}
	else
	{
		auto it = std::find(mConnectedDevices.begin(), mConnectedDevices.end(), convertAddressToLowerCase(address));
		if (it != mConnectedDevices.end())
			mConnectedDevices.erase(it);
		else
		{
			DEBUG("Current status already disconnected, no need to update");
			return;
		}
	}

	getObserver()->propertiesChanged(convertAddressToLowerCase(mAdapter->getAddress()), convertAddressToLowerCase(address), properties);
}
