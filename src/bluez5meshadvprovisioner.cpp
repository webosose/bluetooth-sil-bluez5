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

#include "bluez5meshadvprovisioner.h"
#include "logging.h"
#include "utils.h"
#include "bluez5profilemesh.h"
#include "bluez5adapter.h"
#include <cstdint>
#include <string>

#define BLUEZ_MESH_PROV_PATH "/"
#define DEFAULT_NET_INDEX 0x0000

Bluez5MeshAdvProvisioner::Bluez5MeshAdvProvisioner(Bluez5Adapter *adapter, Bluez5ProfileMesh *mesh) :
mAdapter(adapter),
mMesh(mesh),
mUnicastAddressAvailable(0x00aa)
{
}

Bluez5MeshAdvProvisioner::~Bluez5MeshAdvProvisioner()
{
}

void Bluez5MeshAdvProvisioner::registerProvisionerInterface(GDBusObjectManagerServer *objectManagerServer,
															GDBusObjectSkeleton *provSkeleton)
{
	BluezMeshProvisioner1 *provInterface = bluez_mesh_provisioner1_skeleton_new();

	g_signal_connect(provInterface, "handle_scan_result", G_CALLBACK(handleScanResult), this);
	g_signal_connect(provInterface, "handle_add_node_complete", G_CALLBACK(handleAddNodeComplete), this);
	g_signal_connect(provInterface, "handle_add_node_failed", G_CALLBACK(handleAddNodeFailed), this);
	g_signal_connect(provInterface, "handle_request_prov_data", G_CALLBACK(handleRequestProvData), this);

	g_dbus_object_skeleton_add_interface(provSkeleton, G_DBUS_INTERFACE_SKELETON(provInterface));

}

gboolean Bluez5MeshAdvProvisioner::handleScanResult(BluezMeshProvisioner1 *interface,
													GDBusMethodInvocation *invocation,
													gint16 argRssi,
													GVariant *argData,
													GVariant *argOptions,
													gpointer userData)
{
	Bluez5MeshAdvProvisioner *meshAdvProvisioner = (Bluez5MeshAdvProvisioner *)userData;
	std::vector<unsigned char> result;
	char deviceUuid[16 * 2 + 1];

	result = convertArrayByteGVariantToVector(argData);
	for (int i = 0; i < 16; ++i)
	{
			sprintf(deviceUuid + (i * 2), "%2.2x", result[i]);
	}

	std::string deviceUuidS(deviceUuid);
	DEBUG("Device discovered : %s", deviceUuidS.c_str());
	meshAdvProvisioner->mMesh->getMeshObserver()->scanResult(
		convertAddressToLowerCase(meshAdvProvisioner->mAdapter->getAddress()),
		argRssi, deviceUuidS);

	return true;
}
gboolean Bluez5MeshAdvProvisioner::handleAddNodeComplete(BluezMeshProvisioner1 *interface,
														 GDBusMethodInvocation *invocation,
														 GVariant *uuid,
														 guint16 unicast,
														 guchar count,
														 gpointer userData)
{
	DEBUG("handleAddNodeComplete: count: %d", count);
	std::vector<unsigned char> result;
	char deviceUuid[16 * 2 + 1];

	result = convertArrayByteGVariantToVector(uuid);
	for (int i = 0; i < 16; ++i)
	{
		sprintf(deviceUuid + (i * 2), "%2.2x", result[i]);
	}

	std::string deviceUuidS(deviceUuid);
	Bluez5MeshAdvProvisioner *meshAdvProvisioner = (Bluez5MeshAdvProvisioner *)userData;
	meshAdvProvisioner->mMesh->getMeshObserver()->provisionResult(BLUETOOTH_ERROR_NONE,
																	convertAddressToLowerCase(
																		meshAdvProvisioner->mAdapter->getAddress()),
																	"endProvision", "", 0, "", "",
																	unicast, count, deviceUuidS);

	return true;
}
gboolean Bluez5MeshAdvProvisioner::handleAddNodeFailed(BluezMeshProvisioner1 *interface,
													   GDBusMethodInvocation *invocation,
													   GVariant *uuid,
													   const gchar *reason,
													   gpointer userData)
{
	DEBUG("handleAddNodeFailed: %s", reason);
	std::vector<unsigned char> result;
	char deviceUuid[16 * 2 + 1];

	result = convertArrayByteGVariantToVector(uuid);
	for (int i = 0; i < 16; ++i)
	{
			sprintf(deviceUuid + (i * 2), "%2.2x", result[i]);
	}

	std::string deviceUuidS(deviceUuid);
	BluetoothError error = BLUETOOTH_ERROR_NONE;

	if(strcmp(reason, "bad-pdu") == 0)
		error = BLUETOOTH_ERROR_MESH_BAD_PDU;

	Bluez5MeshAdvProvisioner *meshAdvProvisioner = (Bluez5MeshAdvProvisioner *)userData;
	meshAdvProvisioner->mMesh->getMeshObserver()->provisionResult(error,
																	convertAddressToLowerCase(
																		meshAdvProvisioner->mAdapter->getAddress()),
																	"endProvision", "", 0, "", "",
																	0, 0, deviceUuidS);

	return true;
}
gboolean Bluez5MeshAdvProvisioner::handleRequestProvData(BluezMeshProvisioner1 *interface,
														 GDBusMethodInvocation *invocation,
														 guchar count,
														 gpointer userData)
{
	DEBUG("handleRequestProvData: %d", count);
	Bluez5MeshAdvProvisioner *meshAdvProvisioner = (Bluez5MeshAdvProvisioner *)userData;
	g_dbus_method_invocation_return_value(invocation,
										g_variant_new("(qq)", DEFAULT_NET_INDEX,
										meshAdvProvisioner->mUnicastAddressAvailable));
	for (int i = 0; i < count; ++i)
	{
		meshAdvProvisioner->mUnicastAddresses.push_back(meshAdvProvisioner->mUnicastAddressAvailable + i);
	}
	meshAdvProvisioner->mUnicastAddressAvailable += count;
	DEBUG("Next available unicast address: %x", meshAdvProvisioner->mUnicastAddressAvailable);

	return true;
}

BluetoothError Bluez5MeshAdvProvisioner::updateNodeInfo(std::vector<uint16_t> &unicastAddresses)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	mUnicastAddresses = unicastAddresses;
	sort(mUnicastAddresses.begin(), mUnicastAddresses.end());
	int n = mUnicastAddresses.size();
	mUnicastAddressAvailable = mUnicastAddresses[n-1] + 1;
	return BLUETOOTH_ERROR_NONE;
}