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

#ifndef BLUEZ5GATTREMOTEATTRIBUTE_H
#define BLUEZ5GATTREMOTEATTRIBUTE_H

#include <gio/gio.h>
#include <string>
#include <vector>

extern "C" {
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5ProfileGatt;

class GattRemoteDescriptor
{
public:
	GattRemoteDescriptor(BluezGattDescriptor1 *interface)
		: mInterface(interface) {
	}
	std::vector<unsigned char> readValue(uint16_t offset = 0);
	bool writeValue(const std::vector<unsigned char> &descriptorValue, uint16_t offset = 0);

	static const std::map <BluetoothGattPermission, std::string> descriptorPermissionMap;
	std::string parentObjectPath;
	std::string objectPath;
	BluetoothGattDescriptor descriptor;
	BluezGattDescriptor1 *mInterface;
};

class GattRemoteCharacteristic
{
public:
	static void onCharacteristicPropertiesChanged(GDBusProxy *proxy, GVariant *changed_properties,
                                                  GStrv invalidated_properties, gpointer userdata);
	GattRemoteCharacteristic(BluezGattCharacteristic1 *interface, Bluez5ProfileGatt *gattProfile)
		: mInterface(interface), mGattProfile(gattProfile) {
		g_signal_connect(G_DBUS_PROXY(interface), "g-properties-changed",
						 G_CALLBACK(GattRemoteCharacteristic::onCharacteristicPropertiesChanged), this);
	}
	bool startNotify();
	bool stopNotify();
	std::vector<unsigned char> readValue(uint16_t offset = 0);
	bool writeValue(const std::vector<unsigned char> &characteristicValue, uint16_t offset = 0);
	BluetoothGattCharacteristicProperties readProperties();

	static const std::map <std::string, BluetoothGattCharacteristic::Property> characteristicPropertyMap;
	std::string parentObjectPath;
	std::string objectPath;
	BluetoothGattCharacteristic characteristic;
	BluezGattCharacteristic1 *mInterface;
	Bluez5ProfileGatt *mGattProfile;
	std::vector<GattRemoteDescriptor*> gattRemoteDescriptors;
};

class GattRemoteService
{
public:
	GattRemoteService(BluezGattService1 *interface)
		: mInterface(interface) {
	}
	std::string parentObjectPath;
	std::string objectPath;
	BluetoothGattService service;
	BluezGattService1 *mInterface;
	std::vector<GattRemoteCharacteristic*> gattRemoteCharacteristics;
};

#endif
