// Copyright (c) 2019 LG Electronics, Inc.
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

#include "bluez5profileavrcp.h"
#include "bluez5adapter.h"
#include "logging.h"
#include "utils.h"
#include "bluez5mediacontrol.h"
#include <string>
#include <iostream>

const std::string BLUETOOTH_PROFILE_AVRCP_TARGET_UUID = "0000110c-0000-1000-8000-00805f9b34fb";
const std::string BLUETOOTH_PROFILE_AVRCP_REMOTE_UUID = "0000110e-0000-1000-8000-00805f9b34fb";

const std::map <std::string, BluetoothAvrcpPassThroughKeyCode> Bluez5ProfileAvcrp::mKeyMap = {
	{ "POWER",		KEY_CODE_POWER },
	{ "VOLUME UP",	KEY_CODE_VOLUME_UP },
	{ "VOLUME DOWN",KEY_CODE_VOLUME_DOWN },
	{ "MUTE",		KEY_CODE_MUTE },
	{ "PLAY",		KEY_CODE_PLAY},
	{ "STOP",		KEY_CODE_STOP },
	{ "PAUSE",		KEY_CODE_PAUSE},
	{ "FORWARD",	KEY_CODE_NEXT},
	{ "BACKWARD",	KEY_CODE_PREVIOUS },
	{ "REWIND",		KEY_CODE_REWIND},
	{ "FAST FORWARD",KEY_CODE_FAST_FORWARD}
};

Bluez5ProfileAvcrp::Bluez5ProfileAvcrp(Bluez5Adapter* adapter):
Bluez5ProfileBase(adapter, BLUETOOTH_PROFILE_AVRCP_REMOTE_UUID)
{
}

Bluez5ProfileAvcrp::~Bluez5ProfileAvcrp()
{
}

void Bluez5ProfileAvcrp::connect(const std::string& address, BluetoothResultCallback callback)
{
		auto connectCallback = [this, address, callback](BluetoothError error) {
		if (error != BLUETOOTH_ERROR_NONE)
		{
			DEBUG("AVRCP connect failed");
			callback(error);
			return;
		}
		DEBUG("AVRCP connected succesfully");
		callback(BLUETOOTH_ERROR_NONE);
	};

	Bluez5ProfileBase::connect(address, connectCallback);
}

void Bluez5ProfileAvcrp::disconnect(const std::string& address, BluetoothResultCallback callback)
{
	auto disConnectCallback = [this, address, callback](BluetoothError error) {
		if (error != BLUETOOTH_ERROR_NONE)
		{
			DEBUG("AVRCP disconnect failed");
			callback(error);
			return;
		}
		DEBUG("AVRCP disconnected succesfully");
		callback(BLUETOOTH_ERROR_NONE);
	};
	Bluez5ProfileBase::disconnect(address, disConnectCallback);
}

void Bluez5ProfileAvcrp::enable(const std::string &uuid, BluetoothResultCallback callback)
{
}

void Bluez5ProfileAvcrp::disable(const std::string &uuid, BluetoothResultCallback callback)
{
}

void Bluez5ProfileAvcrp::getProperties(const std::string &address, BluetoothPropertiesResultCallback  callback)
{
}

void Bluez5ProfileAvcrp::getProperty(const std::string &address, BluetoothProperty::Type type, BluetoothPropertyResultCallback callback)
{
	BluetoothProperty prop(type);
	Bluez5Device *device = mAdapter->findDevice(address);
	if (!device)
	{
		callback(BLUETOOTH_ERROR_PARAM_INVALID, prop);
		return;
	}

	bool isConnected = device->isUUIDConnected(BLUETOOTH_PROFILE_AVRCP_REMOTE_UUID);
	DEBUG("AVRCP isConnected %d", isConnected);

	prop.setValue<bool>(isConnected);
	callback(BLUETOOTH_ERROR_NONE, prop);
}

void Bluez5ProfileAvcrp::supplyMediaMetaData(BluetoothAvrcpRequestId requestId, const BluetoothMediaMetaData &metaData, BluetoothResultCallback callback)
{
}

void Bluez5ProfileAvcrp::supplyMediaPlayStatus(BluetoothAvrcpRequestId requestId, const BluetoothMediaPlayStatus &playStatus, BluetoothResultCallback callback)
{
}

void Bluez5ProfileAvcrp::updateConnectionStatus(const std::string &address, bool status)
{
	DEBUG("AVRCP isConnected %d", status);
	BluetoothPropertiesList properties;
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::CONNECTED, status));
	getObserver()->propertiesChanged(convertAddressToLowerCase(address), properties);
}

void Bluez5ProfileAvcrp::recievePassThroughCommand(std::string address, std::string key, std::string state)
{
	DEBUG("Bluez5ProfileAvcrp::recievePassThroughCommand %s %s", key, state);
	BluetoothAvrcpPassThroughKeyStatus keyStatus = KEY_STATUS_UNKNOWN;
	BluetoothAvrcpPassThroughKeyCode keyCode = KEY_CODE_UNKNOWN;

	if (state == "pressed")
		keyStatus = KEY_STATUS_PRESSED;
	else if (state == "released")
		keyStatus = KEY_STATUS_RELEASED;

	auto keyCodeIt = mKeyMap.find(key);
	if (keyCodeIt != mKeyMap.end())
		keyCode = keyCodeIt->second;

	Bluez5Device *device = mAdapter->findDevice(address);
	if (!device)
	{
		DEBUG("Bluez5ProfileAvcrp::recievePassThroughCommand not handled");
		return;
	}
	if (device->isUUIDConnected(BLUETOOTH_PROFILE_AVRCP_REMOTE_UUID))
		getAvrcpObserver()->passThroughCommandReceived(keyCode, keyStatus, convertAddressToLowerCase(address));
}
