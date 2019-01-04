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

#ifndef BLUEZ5MEDIACONTROL_H
#define BLUEZ5MEDIACONTROL_H

#include <string>

extern "C" {
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5Adapter;

class Bluez5MediaControl
{
public:
	Bluez5MediaControl(Bluez5Adapter *adapter, const std::string &objectPath);
	~Bluez5MediaControl();
	static void handlePropertiesChanged(BluezMediaControl1 *controlInterface, gchar *interface,  GVariant *changedProperties,
												   GVariant *invalidatedProperties, gpointer userData);

	std::string getObjectPath() const;
	bool getConnectionStatus() const;
	bool fastForwad();
	bool next();
	bool pause();
	bool play();
	bool previous();
	bool rewind();
	bool volumeDown();
	bool volumeUp();

private:
	Bluez5Adapter *mAdapter;
	std::string mObjectPath;
	BluezMediaControl1 *mInterface;
	bool mState;
};

#endif // BLUEZ5MEDIACONTROL_H
