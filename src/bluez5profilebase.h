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

#ifndef BLUEZ5PROFILEBASE_H
#define BLUEZ5PROFILEBASE_H

#include <bluetooth-sil-api.h>

class Bluez5Adapter;
class Bluez5Device;

class Bluez5ProfileBase : virtual public BluetoothProfile
{
public:
	Bluez5ProfileBase(Bluez5Adapter *adapter, const std::string &uuid);
	~Bluez5ProfileBase();

	virtual void connect(const std::string& address, BluetoothResultCallback callback);
	virtual void disconnect(const std::string& address, BluetoothResultCallback callback);

protected:
	Bluez5Adapter *mAdapter;
	std::string mUuid;
};

#endif // BLUEZ5PROFILEBASE_H
