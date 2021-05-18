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

#ifndef BLUEZ5MESHAPPLICATION_H
#define BLUEZ5MESHAPPLICATION_H

#include <bluetooth-sil-api.h>

extern "C"
{
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5Adapter;
class Bluez5ProfileMesh;
class Bluez5MeshAdv;

class Bluez5MeshApplication
{
public:
	Bluez5MeshApplication(Bluez5Adapter *adapter, Bluez5ProfileMesh *mesh);
	~Bluez5MeshApplication();

	void registerApplicationInterface(GDBusObjectManagerServer *objectManagerServer, GDBusObjectSkeleton *meshAppSkeleton, Bluez5MeshAdv *meshAdv);

	static gboolean handleJoinComplete(BluezMeshApplication1 *object,
										GDBusMethodInvocation *invocation,
										guint64 argToken, gpointer userData);

	static gboolean handleJoinFailed(BluezMeshApplication1 *object,
										GDBusMethodInvocation *invocation,
										const gchar *argReason, gpointer userData);
private:
	Bluez5Adapter *mAdapter;
	Bluez5ProfileMesh *mMesh;
};

#endif //BLUEZ5MESHAPPLICATION_H