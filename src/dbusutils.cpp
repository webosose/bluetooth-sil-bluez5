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

#include <map>
#include <stdint.h>

#include "dbusutils.h"
#include "asyncutils.h"
#include "logging.h"

namespace DBusUtils
{

void checkBus(GBusType busType, StatusCallback callback)
{
	if (!callback)
		return;

	auto busGetCallback = [callback](GAsyncResult *result) {
		GError *error = 0;

		GDBusConnection *conn = g_bus_get_finish(result, &error);
		if (error) {
			g_error_free(error);
		}

		bool busAvailable = (conn != 0);
		callback(busAvailable);

		if (conn)
			g_object_unref(conn);
	};

	g_bus_get(busType, NULL, glibAsyncMethodWrapper,
	          new GlibAsyncFunctionWrapper(busGetCallback));
}

void waitForBus(GBusType busType, StatusCallback callback)
{
	auto busStatusCallback = [busType, callback](bool available) {
		if (!available)
		{
			auto timeoutCallback = [busType, callback]() {
				waitForBus(busType, callback);
				return false;
			};

			g_timeout_add(250, glibSourceMethodWrapper,
			              new GlibSourceFunctionWrapper(timeoutCallback));
			return;
		}

		callback(true);
	};

	checkBus(busType, busStatusCallback);
}

NameWatch::NameWatch(GBusType busType, const std::string &name) :
    mWatch(0)
{
	mWatch = g_bus_watch_name(busType, name.c_str(), G_BUS_NAME_WATCHER_FLAGS_NONE,
	                          handleNameAppeared, handleNameDisappeared, this, NULL);
}

NameWatch::~NameWatch()
{
	if (mWatch > 0)
		g_bus_unwatch_name(mWatch);
}

void NameWatch::watch(StatusCallback callback)
{
	mCallback = callback;
}

void NameWatch::handleNameAppeared(GDBusConnection *conn, const gchar *name, const gchar *nameOwner, gpointer user_data)
{
	NameWatch *self = static_cast<NameWatch*>(user_data);

	if (self->mCallback)
		self->mCallback(true);
}

void NameWatch::handleNameDisappeared(GDBusConnection *conn, const gchar *name, gpointer user_data)
{
	NameWatch *self = static_cast<NameWatch*>(user_data);

	if (self->mCallback)
		self->mCallback(false);
}

ObjectWatch::ObjectWatch(GBusType busType, const std::string &busName, const std::string &path) :
    mPath(path),
    mObjectManager(0)
{
	GError *error = 0;

	mObjectManager = g_dbus_object_manager_client_new_for_bus_sync(busType, G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
	                                                               busName.c_str(), "/", NULL, NULL, NULL, NULL,
	                                                               &error);
	if (error)
	{
		ERROR(MSGID_OBJECT_MANAGER_CREATION_FAILED, 0, "Failed to create object manager: %s", error->message);
		g_error_free(error);
		return;
	}

	g_signal_connect(G_OBJECT(mObjectManager), "interface-removed", G_CALLBACK(handleInterfaceRemoved), this);
	g_signal_connect(G_OBJECT(mObjectManager), "object-removed", G_CALLBACK(handleObjectRemoved), this);

	DEBUG("Created object watch on bus %s for %s", busName.c_str(), path.c_str());
}

ObjectWatch::~ObjectWatch()
{
	g_object_unref(mObjectManager);
}

void ObjectWatch::watchInterfaceRemoved(InterfaceStatusCallback callback)
{
	mInterfaceRemovedCallback = callback;
}

void ObjectWatch::handleInterfaceRemoved(GDBusObjectManager *manager, GDBusObject *object, GDBusInterface *interface, gpointer user_data)
{
	ObjectWatch *watch = static_cast<ObjectWatch*>(user_data);

	DEBUG("%s", __PRETTY_FUNCTION__);

	if (!watch->mInterfaceRemovedCallback)
		return;

	const gchar *objectPath = g_dbus_object_get_object_path(object);
	if (g_strcmp0(objectPath, watch->mPath.c_str()) != 0)
		return;

	GDBusInterfaceInfo *info = g_dbus_interface_get_info(interface);
	if (!info)
		return;

	watch->mInterfaceRemovedCallback(std::string(info->name));
}

void ObjectWatch::handleObjectRemoved(GDBusObjectManager *manager, GDBusObject *object, gpointer user_data)
{
	ObjectWatch *watch = static_cast<ObjectWatch*>(user_data);

	DEBUG("%s", __PRETTY_FUNCTION__);

	if (!watch->mInterfaceRemovedCallback)
		return;

	const gchar *objectPath = g_dbus_object_get_object_path(object);
	if (g_strcmp0(objectPath, watch->mPath.c_str()) != 0)
		return;

	watch->mInterfaceRemovedCallback("all");
}


} // namespace DBusUtils
