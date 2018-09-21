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

#ifndef BLUEZ5OBEXAGENT_H
#define BLUEZ5OBEXAGENT_H

#include <string>
#include "dbusutils.h"

extern "C" {
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5ProfileOpp;
class Bluez5Adapter;

class Bluez5ObexAgent
{
public:
	Bluez5ObexAgent(Bluez5Adapter *adapter);
	~Bluez5ObexAgent();

	static gboolean onHandleAuthorizePush(BluezObexAgent1 *object, GDBusMethodInvocation *invocation,
	                                      const gchar *arg_path,
	                                      gpointer user_data)
	{
		Bluez5ObexAgent *pThis = static_cast<Bluez5ObexAgent*>(user_data);
		return pThis->handleAuthorizePush(object, invocation, arg_path);
	}

	static gboolean onHandleCancel(BluezObexAgent1 *object, GDBusMethodInvocation *invocation,
	                               gpointer user_data)
	{
		Bluez5ObexAgent *pThis = static_cast<Bluez5ObexAgent*>(user_data);
		return pThis->handleCancel(object, invocation);
	}

	static gboolean onHandleRelease(BluezObexAgent1 *object,GDBusMethodInvocation *invocation,
	                                gpointer user_data)
	{
		Bluez5ObexAgent *pThis = static_cast<Bluez5ObexAgent*>(user_data);
		return pThis->handleRelease(object, invocation);
	}
private:
	BluezObexAgentManager1 *mAgentManagerProxy;
	BluezObexAgent1* mAgentInterface;
	DBusUtils::NameWatch mNameWatch;
	Bluez5Adapter *mAdapter;

	void connectWithObex();
	void createObexAgentManagerProxy();
	void deleteObexAgentManagerProxy();
	void registerAgent();
	void createAgentInterface(const std::string &objectPath);
	gboolean handleAuthorizePush(BluezObexAgent1 *object, GDBusMethodInvocation *invocation,
								 const gchar *arg_path);
	gboolean handleCancel(BluezObexAgent1 *object, GDBusMethodInvocation *invocation);
	gboolean handleRelease(BluezObexAgent1 *object,GDBusMethodInvocation *invocation);
};

#endif // BLUEZ5OBEXAGENT_H
