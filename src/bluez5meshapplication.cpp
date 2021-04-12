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
#include "bluez5meshapplication.h"
#include "bluez5profilemesh.h"
#include "bluez5meshadv.h"
#include "bluez5adapter.h"
#include "logging.h"
#include "utils.h"

#define BLUEZ_MESH_APP_PATH "/" //"/silbluez/mesh"

Bluez5MeshApplication::Bluez5MeshApplication(Bluez5Adapter *adapter, Bluez5ProfileMesh *mesh) :
mAdapter(adapter),
mMesh(mesh)
{

}

Bluez5MeshApplication::~Bluez5MeshApplication()
{

}

void Bluez5MeshApplication::registerApplicationInterface(GDBusObjectManagerServer *objectManagerServer, GDBusObjectSkeleton *meshAppSkeleton, Bluez5MeshAdv *meshAdv)
{
	BluezMeshApplication1 *appInterface = bluez_mesh_application1_skeleton_new();
	uint16_t cid = 0x05f1;
	uint16_t pid = 0x0002;
	uint16_t vid = 0x0001;
	bluez_mesh_application1_set_company_id(appInterface, cid);
	bluez_mesh_application1_set_product_id(appInterface, pid);
	bluez_mesh_application1_set_version_id(appInterface, vid);
	bluez_mesh_application1_set_crpl(appInterface, 10);

	g_signal_connect(appInterface, "handle_join_complete", G_CALLBACK(handleJoinComplete), meshAdv);
	g_signal_connect(appInterface, "handle_join_failed", G_CALLBACK(handleJoinFailed), meshAdv);

	g_dbus_object_skeleton_add_interface(meshAppSkeleton, G_DBUS_INTERFACE_SKELETON(appInterface));
}

gboolean Bluez5MeshApplication::handleJoinComplete(BluezMeshApplication1 *object,
										GDBusMethodInvocation *invocation,
										guint64 argToken, gpointer userData)
{
	Bluez5MeshAdv *meshAdv = static_cast<Bluez5MeshAdv *>(userData);
	meshAdv->mToken = argToken;
	DEBUG("handleJoinCompletem mToken: %llu", meshAdv->mToken);
	bluez_mesh_application1_complete_join_complete(object, invocation);
	meshAdv->attach();
	meshAdv->updateNetworkId();
	return true;
}

gboolean Bluez5MeshApplication::handleJoinFailed(BluezMeshApplication1 *object,
										GDBusMethodInvocation *invocation,
										const gchar *argReason, gpointer userData)
{
	DEBUG("handleJoinFailed, reason: %s", argReason);
	return true;
}