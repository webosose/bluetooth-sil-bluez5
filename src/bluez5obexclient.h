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

#ifndef BLUEZ5OBEXCLIENT_H
#define BLUEZ5OBEXCLIENT_H

#include <string>
#include <functional>

#include "bluez5obexsession.h"
#include "dbusutils.h"

extern "C" {
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5ObexSession;

typedef std::function<void(Bluez5ObexSession *session)> Bluez5ObexSessionCreateCallback;

class Bluez5ObexClient
{
public:
	Bluez5ObexClient();
	~Bluez5ObexClient();

	Bluez5ObexClient(const Bluez5ObexClient&) = delete;
	Bluez5ObexClient& operator = (const Bluez5ObexClient&) = delete;

	void createSession(Bluez5ObexSession::Type type, const std::string &deviceAddress, Bluez5ObexSessionCreateCallback callback);
	void destroySession(const std::string &objectPath);

	static void handleObexServiceStarted(GDBusConnection *conn, const gchar *name,
	                                      const gchar *nameOwner, gpointer user_data);
	static void handleObexServiceStopped(GDBusConnection *conn, const gchar *name,
	                                     gpointer user_data);

private:
	BluezObexClient1 *mClientProxy;
	DBusUtils::NameWatch mNameWatch;

	void connectWithObex();

	void createProxy();
	void destroyProxy();
};

#endif // BLUEZ5OBEXCLIENT_H
