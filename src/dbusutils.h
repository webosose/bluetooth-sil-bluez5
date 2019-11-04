// Copyright (c) 2014-2019 LG Electronics, Inc.
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

#ifndef DBUSCONNECTION_H
#define DBUSCONNECTION_H

#include <functional>
#include <string>

#include <glib.h>
#include <gio/gio.h>



namespace DBusUtils
{
	typedef std::function<void(bool)> StatusCallback;

	void checkBus(GBusType busType, StatusCallback callback);
	void waitForBus(GBusType busType, StatusCallback callback);

	class NameWatch
	{
	public:
		NameWatch(GBusType busType, const std::string &busName);
		~NameWatch();

		void watch(StatusCallback callback);

	private:
		uint32_t mWatch;
		StatusCallback mCallback;

		static void handleNameAppeared(GDBusConnection *conn, const gchar *name, const gchar *nameOwner, gpointer user_data);
		static void handleNameDisappeared(GDBusConnection *conn, const gchar *name, gpointer user_data);
	};

	typedef std::function<void (const std::string &)> InterfaceStatusCallback;

	class ObjectWatch
	{
	public:
		ObjectWatch(GBusType busType, const std::string &busName, const std::string &path);
		~ObjectWatch();

		ObjectWatch(const ObjectWatch&) = delete;
		ObjectWatch& operator = (const ObjectWatch&) = delete;

		void watchInterfaceRemoved(InterfaceStatusCallback callback);

	private:
		std::string mPath;
		GDBusObjectManager *mObjectManager;
		InterfaceStatusCallback mInterfaceRemovedCallback;

		static void handleInterfaceRemoved(GDBusObjectManager *manager, GDBusObject *object, GDBusInterface *interface,
		                                   gpointer user_data);
		static void handleObjectRemoved(GDBusObjectManager *manager, GDBusObject *object, gpointer user_data);
	};
}

#endif // DBUSCONNECTION_H
