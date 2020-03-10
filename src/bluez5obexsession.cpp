// Copyright (c) 2014-2020 LG Electronics, Inc.
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

#include "bluez5obexsession.h"
#include "bluez5obexclient.h"
#include "dbusutils.h"
#include "asyncutils.h"
#include "logging.h"
#include "bluez5busconfig.h"

Bluez5ObexSession::Bluez5ObexSession(Bluez5ObexClient *client, Type type, const std::string &objectPath, const std::string &deviceAddress) :
	mClient(client),
	mType(type),
	mObjectPath(objectPath),
	mDeviceAddress(deviceAddress),
	mSessionProxy(0),
	mFileTransferProxy(0),
	mObjectPushProxy(0),
	mPhonebookAccessProxy(nullptr),
	mLostRemote(false),
	mObjectWatch(new DBusUtils::ObjectWatch(BLUEZ5_OBEX_DBUS_BUS_TYPE, "org.bluez.obex", objectPath))
{
	GError *error = 0;

	mSessionProxy = bluez_obex_session1_proxy_new_for_bus_sync(BLUEZ5_OBEX_DBUS_BUS_TYPE, G_DBUS_PROXY_FLAGS_NONE,
	                                                           "org.bluez.obex", mObjectPath.c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_FAILED_TO_CREATE_OBEX_SESSION_PROXY, 0,
			  "Failed to create dbus proxy for session client on path %s",
			  mObjectPath.c_str());
		g_error_free(error);
		return;
	}

	mFileTransferProxy = bluez_obex_file_transfer1_proxy_new_for_bus_sync(BLUEZ5_OBEX_DBUS_BUS_TYPE, G_DBUS_PROXY_FLAGS_NONE,
																		"org.bluez.obex", mObjectPath.c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_FAILED_TO_CREATE_OBEX_FILE_TRANSFER_PROXY, 0,
			  "Failed to create dbus proxy for file transfer on path %s",
			  mObjectPath.c_str());
		g_error_free(error);
		return;
	}

	mObjectPushProxy = bluez_obex_object_push1_proxy_new_for_bus_sync(BLUEZ5_OBEX_DBUS_BUS_TYPE, G_DBUS_PROXY_FLAGS_NONE,
																		"org.bluez.obex", mObjectPath.c_str(), NULL, &error);

	if (error)
	{
		ERROR(MSGID_FAILED_TO_CREATE_OBEX_PUSH_PROXY, 0,
			  "Failed to create dbus proxy for obex push on path %s",
			  mObjectPath.c_str());
		g_error_free(error);
		return;
	}

	mPhonebookAccessProxy = bluez_obex_phonebook_access1_proxy_new_for_bus_sync(BLUEZ5_OBEX_DBUS_BUS_TYPE, G_DBUS_PROXY_FLAGS_NONE,
																		"org.bluez.obex", mObjectPath.c_str(), NULL, &error);

	if (error)
	{
		ERROR(MSGID_FAILED_TO_CREATE_OBEX_PHONEBOOK_PROXY, 0,
			  "Failed to create dbus proxy for obex phonebook on path %s",
			  mObjectPath.c_str());
		g_error_free(error);
		return;
	}

	mObjectWatch->watchInterfaceRemoved([this](const std::string &name) {
		if (name != "org.bluez.obex.Session1" && name != "all")
			return;

		DEBUG("Session interface was removed for %s", mObjectPath.c_str());
		mLostRemote = true;

		if (mStatusCallback)
			mStatusCallback(true);
	});
}

Bluez5ObexSession::~Bluez5ObexSession()
{
	if (!mLostRemote)
		mClient->destroySession(mObjectPath);

	g_object_unref(mFileTransferProxy);
	g_object_unref(mSessionProxy);
	g_object_unref(mPhonebookAccessProxy);

	delete mObjectWatch;
}

void Bluez5ObexSession::watch(Bluez5ObexSessionStatusCallback callback)
{
	mStatusCallback = callback;
}
