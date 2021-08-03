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

#include "bluez5meshelement.h"
#include "bluez5meshmodel.h"
#include "bluez5profilemesh.h"
#include "bluez5adapter.h"
#include "utils.h"
#include "logging.h"
#include "asyncutils.h"
#include "bluez5meshmodelconfigclient.h"
#include "bluez5meshmodelonoffclient.h"
#include "bluez5meshadv.h"
#include "bluez5profilemesh.h"
#include "bluez5adapter.h"
#include <cstdint>
#include <vector>

#define CONFIG_CLIENT_MODEL_ID 0x0001
#define GENERIC_ONOFF_CLIENT_MODEL_ID 0x1001
#define BLUEZ_MESH_ELEMENT_PATH "/element"

/* Generic onoff model opration status */
#define OP_GENERIC_ONOFF_STATUS			0x8204

Bluez5MeshElement::Bluez5MeshElement(uint8_t elementIndex, Bluez5Adapter *adapter,
									Bluez5ProfileMesh *mesh, Bluez5MeshAdv *meshAdv) :
mElementIndex(elementIndex),
mAdapter(adapter),
mMeshProfile(mesh),
mMeshAdv(meshAdv)
{

}

Bluez5MeshElement::~Bluez5MeshElement()
{
	mModels.clear();
}

void Bluez5MeshElement::registerElementInterface(GDBusObjectManagerServer *objectManagerServer)
{
	GDBusObjectSkeleton *elementSkeleton = g_dbus_object_skeleton_new(BLUEZ_MESH_ELEMENT_PATH);
	BluezMeshElement1 *elementInterface = bluez_mesh_element1_skeleton_new();
	g_signal_connect(elementInterface, "handle_dev_key_message_received", G_CALLBACK(handleDevKeyMessageReceived), this);
	g_signal_connect(elementInterface, "handle_message_received", G_CALLBACK(handleMessageReceived), this);

	bluez_mesh_element1_set_index(elementInterface, mElementIndex);

	GVariantBuilder profileBuilder;
	GVariantBuilder prop;
	g_variant_builder_init(&profileBuilder, G_VARIANT_TYPE("a(qa{sv})"));

	for (auto model : mModels)
	{
		g_variant_builder_init(&prop, G_VARIANT_TYPE("a{sv}"));
		g_variant_builder_add (&prop, "{sv}", "Subscribe",g_variant_new("b", false));
		g_variant_builder_add (&prop, "{sv}", "Publish",g_variant_new("b", false));
		g_variant_builder_add (&profileBuilder, "(qa{sv})", (guint16)(model.first), &prop);
	}
	bluez_mesh_element1_set_models(elementInterface, g_variant_new ("a(qa{sv})", &profileBuilder));

	g_dbus_object_skeleton_add_interface(elementSkeleton, G_DBUS_INTERFACE_SKELETON(elementInterface));
	g_dbus_object_manager_server_export(objectManagerServer, elementSkeleton);
}

gboolean Bluez5MeshElement::handleDevKeyMessageReceived(BluezMeshElement1 *object,
														GDBusMethodInvocation *invocation,
														guint16 argSource,
														gboolean argRemote,
														guint16 argNetIndex,
														GVariant *argData,
														gpointer userData)
{
	DEBUG("handleDevKeyMessageReceived: src: %d, remote: %d, netIndex: %d",
		  argSource, argRemote, argNetIndex);
	Bluez5MeshElement *meshElement = (Bluez5MeshElement *)userData;
	uint8_t *data;
	gsize dataLen = 0;

	data = (uint8_t *)g_variant_get_fixed_array(argData, &dataLen, sizeof(guint8));

	DEBUG("Received msg with length: %d", dataLen);

	for ( auto model : meshElement->mModels)
	{
		if (model.second.get()->recvData(argSource, 0, 0, data, dataLen))
		{
			break;
		}
	}
	return true;
}

gboolean Bluez5MeshElement::handleMessageReceived(BluezMeshElement1 *object,
												  GDBusMethodInvocation *invocation,
												  guint16 argSource,
												  guint16 argKeyIndex,
												  GVariant *argDestination,
												  GVariant *argData,
												  gpointer userData)
{
	DEBUG("handleMessageReceived: src: %d, appkeyIndex: %d",
		  argSource, argKeyIndex);
	Bluez5MeshElement *meshElement = (Bluez5MeshElement *)userData;
	uint8_t *data = nullptr;
	gsize dataLen = 0;
	uint16_t destAddr = 0;

	meshElement->mMeshAdv->stopReqTimer();

	data = (uint8_t *)g_variant_get_fixed_array(argData, &dataLen, sizeof(guint8));
	GVariant *destAddrVar = g_variant_get_variant(argDestination);
	destAddr = g_variant_get_uint16(destAddrVar);

	DEBUG("Received msg with length: %d", dataLen);

	for ( auto model : meshElement->mModels)
	{
		if (model.second.get()->recvData(argSource, destAddr, argKeyIndex, data, dataLen))
		{
			break;
		}
	}
	return true;
}

BluetoothError Bluez5MeshElement::addModel(uint32_t modelId)
{
	Bluez5MeshModel *model = nullptr;
	if (CONFIG_CLIENT_MODEL_ID == modelId)
	{
		model = new Bluez5MeshModelConfigClient(modelId, mMeshProfile, mMeshAdv, mAdapter);
	}
	else if (GENERIC_ONOFF_CLIENT_MODEL_ID == modelId)
	{
		model = new Bluez5MeshModelOnOffClient(modelId, mMeshProfile, mMeshAdv, mAdapter);
	}

	std::shared_ptr <Bluez5MeshModel> p(model);
	mModels.insert(std::pair<uint32_t, std::shared_ptr<Bluez5MeshModel>>(modelId, p));

	return BLUETOOTH_ERROR_NONE;
}

BluetoothError Bluez5MeshElement::configGet(uint16_t destAddress,
										const std::string &config,
										uint16_t netKeyIndex)
{
	auto model = mModels.find(CONFIG_CLIENT_MODEL_ID);
	Bluez5MeshModelConfigClient *configClient = (Bluez5MeshModelConfigClient *)(model->second).get();
	return configClient->configGet(destAddress, config, netKeyIndex);
}

BluetoothError Bluez5MeshElement::configSet(
		uint16_t destAddress, const std::string &config,
		uint8_t gattProxyState, uint16_t netKeyIndex, uint16_t appKeyIndex,
		uint32_t modelId, uint8_t ttl, BleMeshRelayStatus *relayStatus)
{
	auto model = mModels.find(CONFIG_CLIENT_MODEL_ID);
	Bluez5MeshModelConfigClient *configClient = (Bluez5MeshModelConfigClient *)(model->second).get();
	return configClient->configSet(destAddress, config, gattProxyState,
									netKeyIndex, appKeyIndex, modelId,
									ttl, relayStatus);
}

BluetoothError Bluez5MeshElement::deleteNode(uint16_t destAddress, uint8_t count)
{
	auto model = mModels.find(CONFIG_CLIENT_MODEL_ID);
	Bluez5MeshModelConfigClient *configClient = (Bluez5MeshModelConfigClient *)(model->second).get();
	return configClient->deleteNode(destAddress, count);
}

BluetoothError Bluez5MeshElement::getCompositionData(uint16_t destAddress)
{
	auto model = mModels.find(CONFIG_CLIENT_MODEL_ID);
	Bluez5MeshModelConfigClient *configClient = (Bluez5MeshModelConfigClient *)(model->second).get();
	return configClient->getCompositionData(destAddress);
}

BluetoothError Bluez5MeshElement::setOnOff(uint16_t destAddress, uint16_t appIndex, bool onoff)
{
	auto model = mModels.find(GENERIC_ONOFF_CLIENT_MODEL_ID);
	Bluez5MeshModelOnOffClient *onoffClient = (Bluez5MeshModelOnOffClient *)(model->second).get();
	return onoffClient->setOnOff(destAddress, appIndex, onoff);
}
