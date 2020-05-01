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

#ifndef BLUEZ5PROFILEHFP_H
#define BLUEZ5PROFILEHFP_H

#include <bluetooth-sil-api.h>
#include <bluez5profilebase.h>
#include <gio/gio.h>
#include <string>

extern "C" {
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5ProfileHfp : public Bluez5ProfileBase,
                      public BluetoothHfpProfile
{
public:
	Bluez5ProfileHfp(Bluez5Adapter* adapter);
	~Bluez5ProfileHfp();
	void openSCO(const std::string &address, BluetoothResultCallback callback) { return; }
	void closeSCO(const std::string &address, BluetoothResultCallback callback) { return; }
	BluetoothError sendResultCode(const std::string &address, const std::string &resultCode) { return BLUETOOTH_ERROR_UNSUPPORTED; }
	BluetoothError sendAtCommand(const std::string &address, const BluetoothHfpAtCommand &atCommand) { return BLUETOOTH_ERROR_UNSUPPORTED; }
	void getProperties(const std::string &address, BluetoothPropertiesResultCallback  callback) { return; }
	void getProperty(const std::string &address, BluetoothProperty::Type type, BluetoothPropertyResultCallback callback);
	void connect(const std::string &address, BluetoothResultCallback callback);
	void disconnect(const std::string &address, BluetoothResultCallback callback);
	void updateConnectionStatus(const std::string &address, bool isConnected, const std::string &uuid);

private:
	std::string mConnectedDeviceAddress;
	std::vector<std::string> mConnectedDevices;
};

#endif
