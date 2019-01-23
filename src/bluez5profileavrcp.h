// Copyright (c) 2018-2019 LG Electronics, Inc.
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

#ifndef BLUEZ5PROFILEAVRCP_H
#define BLUEZ5PROFILEAVRCP_H

#include <bluetooth-sil-api.h>
#include <bluez5profilebase.h>
#include <string>

extern "C" {
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5Adapter;

class Bluez5ProfileAvcrp : public Bluez5ProfileBase,
                      public BluetoothAvrcpProfile
{
public:
	static const std::map <std::string, BluetoothAvrcpPassThroughKeyCode> mKeyMap;
	Bluez5ProfileAvcrp(Bluez5Adapter* adapter);
	~Bluez5ProfileAvcrp();
	void connect(const std::string& address, BluetoothResultCallback callback) override;
	void disconnect(const std::string& address, BluetoothResultCallback callback) override;
	void enable(const std::string &uuid, BluetoothResultCallback callback) override;
	void disable(const std::string &uuid, BluetoothResultCallback callback) override;
	void getProperties(const std::string &address, BluetoothPropertiesResultCallback  callback) override;
	void getProperty(const std::string &address, BluetoothProperty::Type type, BluetoothPropertyResultCallback callback) override;
	void supplyMediaMetaData(BluetoothAvrcpRequestId requestId, const BluetoothMediaMetaData &metaData, BluetoothResultCallback callback) override;
	void supplyMediaPlayStatus(BluetoothAvrcpRequestId requestId, const BluetoothMediaPlayStatus &playStatus, BluetoothResultCallback callback) override;
	void mediaMetaDataRequested(const std::string &address);
	void mediaPlayStatusRequested(const std::string &address);
	void updateConnectionStatus(const std::string &address, bool status);
	void recievePassThroughCommand(std::string address, std::string key, std::string state);

private:
	BluetoothAvrcpRequestId generateMetaDataRequestId() { return ++mMetaDataRequestId; }
	BluetoothAvrcpRequestId generateMediaPlayStatusRequestId() { return ++mMediaPlayStatusRequestId; }

private:
	BluetoothAvrcpRequestId mMetaDataRequestId;
	BluetoothAvrcpRequestId mMediaPlayStatusRequestId;
};

#endif
