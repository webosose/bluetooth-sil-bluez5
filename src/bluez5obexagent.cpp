// Copyright (c) 2018 LG Electronics, Inc.
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

#include "bluez5obexagent.h"
#include "dbusutils.h"
#include "asyncutils.h"
#include "logging.h"
#include "bluez5adapter.h"
#include "bluez5profileopp.h"
#include "bluez5busconfig.h"

const std::string OBEX_AGENT_PATH = "/obex/agent";

Bluez5ObexAgent::Bluez5ObexAgent(Bluez5Adapter *adapter) :
	mAgentManagerProxy(nullptr),
	mAgentInterface(nullptr),
	mNameWatch(BLUEZ5_OBEX_DBUS_BUS_TYPE, "org.bluez.obex"),
	mAdapter(adapter)
{
	DBusUtils::waitForBus(BLUEZ5_OBEX_DBUS_BUS_TYPE, [this](bool available) {
		if (!available) {
			// register an empty name watch handler
			mNameWatch.watch([](bool available) { });
			return;
		}

		DEBUG("DBus session bus is available now");

		connectWithObex();
	});
}

Bluez5ObexAgent::~Bluez5ObexAgent()
{
}

void Bluez5ObexAgent::connectWithObex()
{
	DEBUG("Waiting for OBEX service to be available on the bus");

	mNameWatch.watch([this](bool available) {
		if (available)
		{
			createObexAgentManagerProxy();
		}
		else
		{
			deleteObexAgentManagerProxy();
		}
	});
}

void Bluez5ObexAgent::createObexAgentManagerProxy()
{
	if (mAgentManagerProxy)
	{
		WARNING(MSGID_PROXY_ALREADY_EXISTS, 0, "Proxy for OBEX agent mgr already exists. Removing it first");
		deleteObexAgentManagerProxy();
	}

	auto createProxyCallback = [this](GAsyncResult *result) {
		GError *error = 0;

		mAgentManagerProxy = bluez_obex_agent_manager1_proxy_new_for_bus_finish(result, &error);
		if (error)
		{
			ERROR(MSGID_FAILED_TO_CREATE_OBEX_AGENT_MGR_PROXY, 0,
			      "Failed to create dbus proxy for OBEX agent mgr: %s", error->message);
			g_error_free(error);
			return;
		}

		DEBUG("Successfully created agent manager for OBEX client");
		registerAgent();
	};

	bluez_obex_agent_manager1_proxy_new_for_bus(BLUEZ5_OBEX_DBUS_BUS_TYPE, G_DBUS_PROXY_FLAGS_NONE,
	                                     "org.bluez.obex", "/org/bluez/obex", NULL,
	                                     glibAsyncMethodWrapper,
	                                     new GlibAsyncFunctionWrapper(createProxyCallback));
}

void Bluez5ObexAgent::deleteObexAgentManagerProxy()
{
	DEBUG("Destroying proxy for OBEX agent manager");

	if (mAgentManagerProxy)
	{
		g_object_unref(mAgentManagerProxy);
		mAgentManagerProxy = nullptr;
	}

	if (mAgentInterface)
	{
		g_object_unref(mAgentInterface);
		mAgentInterface = nullptr;
	}
}

void Bluez5ObexAgent::registerAgent()
{
	DEBUG("registerAgent with OBEX agent manager");

	std::string objectPath(OBEX_AGENT_PATH);

	auto registerCallback = [this, objectPath](GAsyncResult *result) {
		GError *error = 0;
		gboolean ret;

		ret = bluez_obex_agent_manager1_call_register_agent_finish(mAgentManagerProxy, result, &error);
		if (error)
		{
			ERROR(MSGID_FAILED_TO_CREATE_OBEX_AGENT_MGR_PROXY, 0, "Failed to register obex agent on path %s",
				  objectPath.c_str());
			g_error_free(error);
			return;
		}

		createAgentInterface(objectPath);
	};

	bluez_obex_agent_manager1_call_register_agent(mAgentManagerProxy, objectPath.c_str(), NULL,
	                                       glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(registerCallback));
}

void Bluez5ObexAgent::createAgentInterface(const std::string &objectPath)
{
	DEBUG("creating interface OBEX agent");

	GError *error = nullptr;
	GDBusConnection *mConn = g_bus_get_sync(BLUEZ5_OBEX_DBUS_BUS_TYPE, NULL, &error);
	if (error)
	{
		ERROR(MSGID_FAILED_TO_CREATE_OBEX_AGENT_MGR_PROXY, 0, "Failed to create obex agent on path");
		g_error_free(error);
		return;
	}

	if (mAgentInterface)
	{
		g_object_unref(mAgentInterface);
		mAgentInterface = nullptr;
	}

	mAgentInterface = bluez_obex_agent1_skeleton_new();

	g_signal_connect(mAgentInterface,
		"handle_authorize_push",
		G_CALLBACK (onHandleAuthorizePush),
		this);

	g_signal_connect(mAgentInterface,
		"handle_cancel",
		G_CALLBACK (onHandleCancel),
		this);

	g_signal_connect(mAgentInterface,
		"handle_release",
		G_CALLBACK (onHandleRelease),
		this);

	g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (mAgentInterface),
									  mConn,
									  objectPath.c_str(),
									  &error);
}

gboolean Bluez5ObexAgent::handleAuthorizePush(BluezObexAgent1 *object, GDBusMethodInvocation *invocation,
	                                      const gchar *arg_path)
{
	Bluez5ProfileOpp *oppProfile = dynamic_cast<Bluez5ProfileOpp*> (mAdapter->getProfile(BLUETOOTH_PROFILE_ID_OPP));
	oppProfile->agentTransferConfirmationRequested(object, invocation, arg_path);
	return TRUE;
}

gboolean Bluez5ObexAgent::handleCancel(BluezObexAgent1 *object, GDBusMethodInvocation *invocation)
{
	g_dbus_method_invocation_return_dbus_error(invocation, "org.bluez.Error.Canceled", "User cancelled confirmation");
	return TRUE;
}

gboolean Bluez5ObexAgent::handleRelease(BluezObexAgent1 *object,GDBusMethodInvocation *invocation)
{
	return TRUE;
}
