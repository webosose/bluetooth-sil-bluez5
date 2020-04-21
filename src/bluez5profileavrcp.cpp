// Copyright (c) 2018-2020 LG Electronics, Inc.
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
#include "bluez5mprisplayer.h"
#include "bluez5profilea2dp.h"
#include <string>


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
Bluez5ProfileBase(adapter, BLUETOOTH_PROFILE_AVRCP_REMOTE_UUID),
mMetaDataRequestId(0),
mMediaPlayStatusRequestId(0),
mConnected(false),
mConnectedDevice(nullptr)
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

	if (!mConnected)
	{
		updateAvrcpUuid(address, connectCallback);
		Bluez5ProfileBase::connect(address, connectCallback);
	}
	else
		connectCallback(BLUETOOTH_ERROR_DEVICE_ALREADY_CONNECTED);
}

void Bluez5ProfileAvcrp::updateAvrcpUuid(const std::string &address, BluetoothResultCallback callback)
{
	Bluez5Device *device = mAdapter->findDevice(address);
	if (!device)
	{
		callback(BLUETOOTH_ERROR_UNKNOWN_DEVICE_ADDR);
		return;
	}

	for(auto tempUuid : device->getUuids())
	{
		if ((tempUuid == BLUETOOTH_PROFILE_AVRCP_TARGET_UUID) ||
				(tempUuid == BLUETOOTH_PROFILE_AVRCP_REMOTE_UUID))
		{
			DEBUG("AVRCP UID supported by device: %s", tempUuid.c_str());
			mUuid = tempUuid;
			break;
		}
	}
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
        updateAvrcpUuid(address, disConnectCallback);
	Bluez5ProfileBase::disconnect(address, disConnectCallback);
}

void Bluez5ProfileAvcrp::enable(const std::string &uuid, BluetoothResultCallback callback)
{
	mAdapter->notifyAvrcpRoleChange(uuid);
	callback(BLUETOOTH_ERROR_NONE);
}

void Bluez5ProfileAvcrp::disable(const std::string &uuid, BluetoothResultCallback callback)
{
	callback(BLUETOOTH_ERROR_UNSUPPORTED);
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

	bool connected = (device == mConnectedDevice);
	prop.setValue<bool>(connected);
	callback(BLUETOOTH_ERROR_NONE, prop);
}

void Bluez5ProfileAvcrp::supplyMediaMetaData(BluetoothAvrcpRequestId requestId, const BluetoothMediaMetaData &metaData, BluetoothResultCallback callback)
{
	mAdapter->getPlayer()->setMediaMetaData(metaData);
	callback(BLUETOOTH_ERROR_NONE);
}

void Bluez5ProfileAvcrp::supplyMediaPlayStatus(BluetoothAvrcpRequestId requestId, const BluetoothMediaPlayStatus &playStatus, BluetoothResultCallback callback)
{
	mAdapter->getPlayer()->setMediaPlayStatus(playStatus);
	if (playStatus.getStatus() == BluetoothMediaPlayStatus::MEDIA_PLAYSTATUS_STOPPED)
		mAdapter->getPlayer()->setMediaPosition(0);
	else
		mAdapter->getPlayer()->setMediaPosition(playStatus.getPosition());
	mAdapter->getPlayer()->setMediaDuration(playStatus.getDuration());
	callback(BLUETOOTH_ERROR_NONE);
}

void Bluez5ProfileAvcrp::mediaPlayStatusRequested(const std::string &address)
{
	getAvrcpObserver()->mediaPlayStatusRequested(generateMediaPlayStatusRequestId(), address);
}

void Bluez5ProfileAvcrp::mediaMetaDataRequested(const std::string &address)
{
	getAvrcpObserver()->mediaMetaDataRequested(generateMetaDataRequestId(), address);
}

void Bluez5ProfileAvcrp::updateConnectionStatus(const std::string &address, bool status)
{
	DEBUG("AVRCP isConnected %d", status);
	BluetoothPropertiesList properties;
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::CONNECTED, status));
	getObserver()->propertiesChanged(convertAddressToLowerCase(mAdapter->getAddress()), convertAddressToLowerCase(address), properties);

	mConnected = status;

	if (status)
	{
		Bluez5Device *device = mAdapter->findDevice(address);
		if (!device)
		{
			return;
		}

		mConnectedDevice = device;
	}
	else
	{
		mConnectedDevice = nullptr;
	}

}

void Bluez5ProfileAvcrp::updateRemoteFeatures(uint8_t features, const std::string &role, const std::string &address)
{
	Bluez5Device *device = mAdapter->findDevice(address);
	if (!device)
	{
		return;
	}

	BluetoothAvrcpRemoteFeatures remoteFeatures = FEATURE_NONE;
	if(features & REMOTE_DEVICE_AVRCP_FEATURE_BROWSE)
		remoteFeatures = FEATURE_BROWSE;
	else if(features & REMOTE_DEVICE_AVRCP_FEATURE_ABSOLUTE_VOLUME)
		remoteFeatures = FEATURE_ABSOLUTE_VOLUME;
	else if(features & REMOTE_DEVICE_AVRCP_FEATURE_METADATA)
		remoteFeatures = FEATURE_METADATA;

	if (mConnected && mConnectedDevice && mConnectedDevice->getAddress() == address)
		getAvrcpObserver()->remoteFeaturesReceived(remoteFeatures, convertAddressToLowerCase(mAdapter->getAddress()),convertAddressToLowerCase(address), role);
}

void Bluez5ProfileAvcrp::updateVolume(const std::string &address, int volume)
{
	DEBUG("updateVolume %d", volume);

	Bluez5Device *device = mAdapter->findDevice(address);
	if (!device)
	{
		DEBUG("Bluez5ProfileAvcrp::updateVolume not handled");
		return;
	}

	if (mConnected)
		getAvrcpObserver()->volumeChanged(volume, convertAddressToLowerCase(mAdapter->getAddress()), convertAddressToLowerCase(address));
}

void Bluez5ProfileAvcrp::recievePassThroughCommand(std::string address, std::string key, std::string state)
{
	DEBUG("Bluez5ProfileAvcrp::recievePassThroughCommand %s %s", key.c_str(), state.c_str());
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
	if (mConnected)
		getAvrcpObserver()->passThroughCommandReceived(keyCode, keyStatus, convertAddressToLowerCase(mAdapter->getAddress()), convertAddressToLowerCase(address));
}

BluetoothError Bluez5ProfileAvcrp::setAbsoluteVolume(const std::string &address, int volume)
{
	Bluez5ProfileA2dp *a2dp = dynamic_cast<Bluez5ProfileA2dp*> (mAdapter->getProfile(BLUETOOTH_PROFILE_ID_A2DP));
	if (a2dp)
		bluez_media_transport1_set_volume(a2dp->getMediaTransport(), (uint8_t)volume);
	return BLUETOOTH_ERROR_NONE;
}
