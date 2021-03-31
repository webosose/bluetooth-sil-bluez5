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
#include "bluez5meshadvprovagent.h"
#include "bluez5profilemesh.h"
#include "bluez5adapter.h"
#include "logging.h"
#include "utils.h"
#include <cstdint>
#include <string>

#define BLUEZ_MESH_AGENT_PATH "/agent"

Bluez5MeshAdvProvAgent::Bluez5MeshAdvProvAgent(Bluez5Adapter *adapter, Bluez5ProfileMesh *mesh) :
mAdapter(adapter),
mMesh(mesh),
supplyNumericInvokation(nullptr),
supplyStaticInvokation(nullptr)
{
}

Bluez5MeshAdvProvAgent::~Bluez5MeshAdvProvAgent()
{
}

void Bluez5MeshAdvProvAgent::registerProvAgentInterface(GDBusObjectManagerServer *objectManagerServer)
{
	GDBusObjectSkeleton *agentSkeleton = g_dbus_object_skeleton_new(BLUEZ_MESH_AGENT_PATH);
	BluezMeshProvisionAgent1 *agentInterface = bluez_mesh_provision_agent1_skeleton_new();

	g_signal_connect(agentInterface, "handle_display_numeric", G_CALLBACK(handleDisplayNumeric), this);
	g_signal_connect(agentInterface, "handle_display_string", G_CALLBACK(handleDisplayString), this);
	g_signal_connect(agentInterface, "handle_prompt_numeric", G_CALLBACK(handlePromptNumeric), this);
	g_signal_connect(agentInterface, "handle_prompt_static", G_CALLBACK(handlePromptStatic), this);

	g_dbus_object_skeleton_add_interface(agentSkeleton, G_DBUS_INTERFACE_SKELETON(agentInterface));
	g_dbus_object_manager_server_export(objectManagerServer, agentSkeleton);
}

BluetoothError Bluez5MeshAdvProvAgent::supplyNumeric(uint32_t number)
{
	return BLUETOOTH_ERROR_UNSUPPORTED;
}
BluetoothError Bluez5MeshAdvProvAgent::supplyStatic(const std::string &oobData)
{
	DEBUG("supplyStatic");
	if (!supplyStaticInvokation)
	{
			return BLUETOOTH_ERROR_NOT_ALLOWED;
	}
	uint8_t out[16];
	for (int i = 0; i < 16; i++)
	{
			sscanf(&oobData[i * 2], "%02hhx", &out[i]);
	}
	GBytes *bytes = g_bytes_new(out, 16);
	GVariant *oobVar = g_variant_new_from_bytes(G_VARIANT_TYPE_BYTESTRING, bytes, true);
	GVariant *tuple = g_variant_new_tuple(&oobVar, 1);
	g_dbus_method_invocation_return_value(supplyStaticInvokation, tuple);
	return BLUETOOTH_ERROR_NONE;
}

gboolean Bluez5MeshAdvProvAgent::handleDisplayNumeric(BluezMeshProvisionAgent1 *object,
													  GDBusMethodInvocation *invocation,
													  const gchar *argType,
													  gint argNumber,
													  gpointer userData)
{
	DEBUG("handleDisplayNumeric");
	return true;
}

gboolean Bluez5MeshAdvProvAgent::handleDisplayString(BluezMeshProvisionAgent1 *object,
													 GDBusMethodInvocation *invocation,
													 const gchar *argValue,
													 gpointer userData)
{
	DEBUG("handleDisplayString");
	return true;
}

gboolean Bluez5MeshAdvProvAgent::handlePromptNumeric(BluezMeshProvisionAgent1 *object,
													 GDBusMethodInvocation *invocation,
													 const gchar *argType,
													 gpointer userData)
{
	DEBUG("handlePromptNumeric");
	return true;
}

gboolean Bluez5MeshAdvProvAgent::handlePromptStatic(BluezMeshProvisionAgent1 *object,
													GDBusMethodInvocation *invocation,
													const gchar *argType,
													gpointer userData)
{
	Bluez5MeshAdvProvAgent *provAgent = (Bluez5MeshAdvProvAgent *)userData;
	DEBUG("handlePromptStatic: %s", argType);
	provAgent->supplyStaticInvokation = invocation;
	provAgent->mMesh->getMeshObserver()->provisionResult(BLUETOOTH_ERROR_NONE,
											  convertAddressToLowerCase(
												  provAgent->mAdapter->getAddress()),
											  "promptStatic", "", 0, "", "", 0, "");
	return true;
}