// Copyright (c) 2021 LG Electronics, Inc.
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

#ifndef BLUEZ5MESHADVPROVAGENT_H
#define BLUEZ5MESHADVPROVAGENT_H

#include <bluetooth-sil-api.h>
#include <cstdint>
#include <string>

extern "C"
{
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5Adapter;
class Bluez5ProfileMesh;

class Bluez5MeshAdvProvAgent
{
public:
	Bluez5MeshAdvProvAgent(Bluez5Adapter *adapter, Bluez5ProfileMesh *mesh);
	~Bluez5MeshAdvProvAgent();

	void registerProvAgentInterface(GDBusObjectManagerServer *objectManagerServer);
	BluetoothError supplyNumeric(uint32_t number);
	BluetoothError supplyStatic(const std::string &oobData);
	static gboolean handleDisplayNumeric(BluezMeshProvisionAgent1 *object,
										 GDBusMethodInvocation *invocation,
										 const gchar *argType,
										 gint argNumber,
										 gpointer userData);

	static gboolean handleDisplayString(BluezMeshProvisionAgent1 *object,
										GDBusMethodInvocation *invocation,
										const gchar *argValue,
										gpointer userData);

	static gboolean handlePromptNumeric(BluezMeshProvisionAgent1 *object,
										GDBusMethodInvocation *invocation,
										const gchar *argType,
										gpointer userData);

	static gboolean handlePromptStatic(BluezMeshProvisionAgent1 *object,
									   GDBusMethodInvocation *invocation,
									   const gchar *argType,
									   gpointer userData);

private:
	Bluez5Adapter *mAdapter;
	Bluez5ProfileMesh *mMesh;

	GDBusMethodInvocation *supplyNumericInvokation;
	GDBusMethodInvocation *supplyStaticInvokation;
};

#endif //BLUEZ5MESHADVPROVAGENT_H