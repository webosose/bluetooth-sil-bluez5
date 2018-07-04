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

#ifndef BLUEZ5AGENT_H
#define BLUEZ5AGENT_H

#include <glib.h>
#include <string>
#include <unordered_map>

extern "C" {
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5AgentPairingInfo;
class Bluez5SIL;

class Bluez5Agent
{
public:
	Bluez5Agent(BluezAgentManager1 *agentManager, Bluez5SIL *sil);
	~Bluez5Agent();

	bool startPairingForDevice(Bluez5Device *device, bool incoming = false);
	void stopPairingForDevice(const std::string &address);
	void stopPairingForDevice(Bluez5Device *device);

	bool supplyPairingConfirmation(const std::string &address, bool accept);
	bool supplyPairingSecret(const std::string &address, const std::string &pin);
	bool supplyPairingSecret(const std::string &address, BluetoothPasskey passkey);
	bool cancelPairing(const std::string address);

	static void handleBusAcquired(GDBusConnection *connection, const gchar *name, gpointer user_data);

	static gboolean handleRequestConfirmation(BluezAgent1 *proxy, GDBusMethodInvocation *invocation,
                                                  const gchar *address, const guint passkey, gpointer user_data);
	static gboolean handleRequestPasskey(BluezAgent1 *proxy, GDBusMethodInvocation *invocation,
                                             const gchar *address, gpointer user_data);
	static gboolean handleRequestPinCode(BluezAgent1 *proxy, GDBusMethodInvocation *invocation,
		                             const gchar *address, gpointer user_data);
	static gboolean handleDisplayPasskey(BluezAgent1 *proxy, GDBusMethodInvocation *invocation,
                                             const gchar *address, const guint passkey, const guint passkey2,
                                             gpointer user_data);
	static gboolean handleDisplayPinCode(BluezAgent1 *proxy, GDBusMethodInvocation *invocation,
                                         const gchar *address, const gchar *pincode, gpointer user_data);
	static gboolean handleRequestAuthorization(BluezAgent1 *proxy, GDBusMethodInvocation *invocation,
                                               const gchar *address, gpointer user_data);
	static gboolean handleAuthorizeService(BluezAgent1 *proxy, GDBusMethodInvocation *invocation,
                                           const gchar *address, const gchar *service, gpointer user_data);
	static gboolean handleCancel(BluezAgent1 *proxy, GDBusMethodInvocation *invocation, gpointer user_data);
	static gboolean handleRelease(BluezAgent1 *proxy, GDBusMethodInvocation *invocation, gpointer user_data);


private:
	Bluez5AgentPairingInfo* findPairingInfoForDevice(const std::string &objectPath);
	Bluez5AgentPairingInfo* findPairingInfoForAddress(const std::string &address);
	Bluez5AgentPairingInfo* initiatePairing(GDBusMethodInvocation *invocation, const gchar *objectPath);
	void createInterface(GDBusConnection *connection);
	std::string convertPairingIOCapability(BluetoothPairingIOCapability capability);

private:
	guint mBusId;
	BluezAgent1 *mInterface;
	BluezAgentManager1 *mAgentManager;
	std::string mPath;
	std::unordered_map<std::string, Bluez5AgentPairingInfo*> mDevicePairings;
	Bluez5SIL *mSIL;
	BluetoothPairingIOCapability mCapability;
};

#endif // BLUEZ5AGENT_H
