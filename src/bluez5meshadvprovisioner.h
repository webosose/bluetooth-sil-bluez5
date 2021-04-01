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

#ifndef BLUEZ5MESHADVPROV_H
#define BLUEZ5MESHADVPROV_H

#include <cstdint>

extern "C"
{
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5Adapter;
class Bluez5ProfileMesh;

class Bluez5MeshAdvProvisioner
{
public:
	Bluez5MeshAdvProvisioner(Bluez5Adapter *adapter, Bluez5ProfileMesh *mesh);
	~Bluez5MeshAdvProvisioner();

	void registerProvisionerInterface(GDBusObjectManagerServer *objectManagerServer,
									  GDBusObjectSkeleton *provSkeleton);

	static gboolean handleScanResult(BluezMeshProvisioner1 *interface,
						GDBusMethodInvocation *invocation,
						gint16 argRssi,
						GVariant *argData,
						gpointer userData);
	static gboolean handleAddNodeComplete(BluezMeshProvisioner1 *interface,
							GDBusMethodInvocation *invocation,
							GVariant *uuid,
							guint16 unicast,
							guchar count,
							gpointer userData);
	static gboolean handleAddNodeFailed(BluezMeshProvisioner1 *interface,
							GDBusMethodInvocation *invocation,
							GVariant *uuid,
							const gchar *reason,
							gpointer userData);
	static gboolean handleRequestProvData(BluezMeshProvisioner1 *interface,
							GDBusMethodInvocation *invocation,
							guchar count,
							gpointer userData);

private:
	Bluez5Adapter *mAdapter;
	Bluez5ProfileMesh *mMesh;

	//TODO : We need to store the information about all the nodes in the network.
	/* Loop through the available nodes and number of elements in the node to decide
	* the next available unicast address. It should also consider the number of elements
	* in the device that is currently being provisioned.
	*/
	uint16_t mUnicastAddressAvailable;
};

#endif //BLUEZ5MESHADVPROV_H