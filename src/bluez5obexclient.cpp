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

#include "bluez5obexclient.h"
#include "bluez5obexsession.h"
#include "dbusutils.h"
#include "asyncutils.h"
#include "logging.h"

Bluez5ObexClient::Bluez5ObexClient() :
    mClientProxy(0),
    mNameWatch(G_BUS_TYPE_SESSION, "org.bluez.obex")
{
	DBusUtils::waitForBus(G_BUS_TYPE_SESSION, [this](bool available) {
		if (!available) {
			// register an empty name watch handler
			mNameWatch.watch([](bool available) { });
			return;
		}

		DEBUG("DBus session bus is available now");

		connectWithObex();
	});
}

Bluez5ObexClient::~Bluez5ObexClient()
{
	destroyProxy();
}

void Bluez5ObexClient::connectWithObex()
{
	DEBUG("Waiting for OBEX service to be available on the bus");

	mNameWatch.watch([this](bool available) {
		if (available)
			createProxy();
		else
			destroyProxy();
	});
}

void Bluez5ObexClient::createProxy()
{
	if (mClientProxy)
	{
		WARNING(MSGID_PROXY_ALREADY_EXISTS, 0, "Proxy for OBEX client already exists. Removing it first");
		destroyProxy();
	}

	DEBUG("Creating proxy for OBEX client ..");

	auto createProxyCallback = [this](GAsyncResult *result) {
		GError *error = 0;

		mClientProxy = bluez_obex_client1_proxy_new_for_bus_finish(result, &error);
		if (error)
		{
			ERROR(MSGID_FAILED_TO_CREATE_OBEX_CLIENT_PROXY, 0,
			      "Failed to create dbus proxy for OBEX client: %s", error->message);
			g_error_free(error);
			return;
		}

		DEBUG("Successfully created proxy for OBEX client");
	};

	bluez_obex_client1_proxy_new_for_bus(G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE,
	                                     "org.bluez.obex", "/org/bluez/obex", NULL,
	                                     glibAsyncMethodWrapper,
	                                     new GlibAsyncFunctionWrapper(createProxyCallback));
}

void Bluez5ObexClient::destroyProxy()
{
	DEBUG("Destroying proxy for OBEX client");

	if (mClientProxy)
	{
		g_object_unref(mClientProxy);
		mClientProxy = 0;
	}
}

std::string sessionTypeToString(Bluez5ObexSession::Type type)
{
	std::string result = "";

	switch (type) {
	case Bluez5ObexSession::Type::FTP:
		result = "ftp";
		break;
	case Bluez5ObexSession::Type::MAP:
		result = "map";
		break;
	case Bluez5ObexSession::Type::OPP:
		result = "opp";
		break;
	case Bluez5ObexSession::Type::PBAP:
		result = "pbap";
		break;
	case Bluez5ObexSession::Type::SYNC:
		result = "sync";
		break;
	default:
		break;
	}

	return result;
}

void Bluez5ObexClient::createSession(Bluez5ObexSession::Type type, const std::string &deviceAddress, Bluez5ObexSessionCreateCallback callback)
{
	if (!mClientProxy)
	{
		callback(0);
		return;
	}

	auto createSessionCallback = [this, callback, type, deviceAddress](GAsyncResult *result) {
		GError *error = 0;
		gboolean ret;
		gchar *objectPath = 0;

		ret = bluez_obex_client1_call_create_session_finish(mClientProxy, &objectPath, result, &error);
		if (error)
		{
			g_error_free(error);
			callback(0);
			return;
		}

		Bluez5ObexSession *session = new Bluez5ObexSession(this, type, std::string(objectPath), deviceAddress);

		// The caller has to take care to release the memory for the session when it isn't used anymore
		callback(session);
	};

	GVariantBuilder *builder = 0;
	GVariant *arguments = 0;

	std::string typeStr = sessionTypeToString(type);

	builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add (builder, "{sv}", "Target", g_variant_new_string (typeStr.c_str()));
	arguments = g_variant_builder_end (builder);

	bluez_obex_client1_call_create_session(mClientProxy, deviceAddress.c_str(), arguments, NULL,
	                                       glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(createSessionCallback));
}

void Bluez5ObexClient::destroySession(const std::string &objectPath)
{
	if (!mClientProxy)
		return;

	auto removeSessionCallback = [this, objectPath](GAsyncResult *result) {
		GError *error = 0;
		gboolean ret;

		ret = bluez_obex_client1_call_remove_session_finish(mClientProxy, result, &error);
		if (error)
		{
			ERROR(MSGID_FAILED_TO_REMOVE_OBEX_SESSION_PROXY, 0, "Failed to remove obex session on path %s",
				  objectPath.c_str());
			g_error_free(error);
			return;
		}
	};

	bluez_obex_client1_call_remove_session(mClientProxy, objectPath.c_str(), NULL,
	                                       glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(removeSessionCallback));
}
