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

#include <gio/gio.h>

#include <bluetooth-sil-api.h>

#include "bluez5adapter.h"
#include "bluez5agent.h"
#include "bluez5sil.h"
#include "logging.h"
#include "utils.h"

#define BLUEZ5_AGENT_BUS_NAME           "com.webos.service.bluetooth2"
#define BLUEZ5_AGENT_OBJECT_PATH        "/"

#define BLUEZ5_AGENT_ERROR_CANCELED        "org.bluez.Error.Canceled"
#define BLUEZ5_AGENT_ERROR_REJECTED        "org.bluez.Error.Rejected"
#define BLUEZ5_AGENT_ERROR_NOT_IMPLEMENTED "org.bluez.Error.NotImplemented"

std::map<BluetoothPairingIOCapability, std::string> pairingIOCapability =
{
	{BLUETOOTH_PAIRING_IO_CAPABILITY_NO_INPUT_NO_OUTPUT, "NoInputNoOutput"},
	{BLUETOOTH_PAIRING_IO_CAPABILITY_DISPLAY_ONLY, "DisplayOnly"},
	{BLUETOOTH_PAIRING_IO_CAPABILITY_DISPLAY_YES_NO, "DisplayYesNo"},
	{BLUETOOTH_PAIRING_IO_CAPABILITY_KEYBOARD_ONLY, "KeyboardOnly"},
	{BLUETOOTH_PAIRING_IO_CAPABILITY_KEYBOARD_DISPLAY, "KeyboardDisplay"}
};

class Bluez5AgentPairingInfo
{
public:
	Bluez5AgentPairingInfo(Bluez5Adapter *adapter, const std::string &address) :
		adapter(adapter),
		confirmation(false),
		passkey(0),
		deviceAddress(address),
		incoming(false),
		requestConfirmation(0),
		requestAuthorization(0),
		requestPairingSecret(0),
		displayPairingSecret(0)
	{
	}

	Bluez5Adapter *adapter;
	bool confirmation;
	std::string pin;
	BluetoothPasskey passkey;
	std::string deviceAddress;
	bool incoming;

	GDBusMethodInvocation *requestConfirmation;
	GDBusMethodInvocation *requestAuthorization;
	GDBusMethodInvocation *requestPairingSecret;
	GDBusMethodInvocation *displayPairingSecret;
};

Bluez5Agent::Bluez5Agent(BluezAgentManager1 *agentManager, Bluez5SIL *sil) :
	mBusId(0),
	mInterface(0),
	mAgentManager(agentManager),
	mPath(BLUEZ5_AGENT_OBJECT_PATH),
	mSIL(sil),
	mCapability(BLUETOOTH_PAIRING_IO_CAPABILITY_NO_INPUT_NO_OUTPUT)
{
	mBusId = g_bus_own_name(G_BUS_TYPE_SYSTEM, BLUEZ5_AGENT_BUS_NAME,
				G_BUS_NAME_OWNER_FLAGS_NONE,
				handleBusAcquired, NULL, NULL, this, NULL);

	mCapability = mSIL->getCapability();
}

Bluez5Agent::~Bluez5Agent()
{
	if (mInterface)
	{
		g_object_unref(mInterface);
	}

	if (mBusId)
	{
		g_bus_unown_name(mBusId);
	}
}

std::string Bluez5Agent::convertPairingIOCapability(BluetoothPairingIOCapability capability)
{
	if (pairingIOCapability.find(capability) == pairingIOCapability.end())
		WARNING(MSGID_PAIRING_IO_CAPABILITY_STRING_ERROR, 0, "Failed to get pairing IO capability string for capability %d", capability);
	else
		return pairingIOCapability[capability];
}

void Bluez5Agent::handleBusAcquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	Bluez5Agent *agent = static_cast<Bluez5Agent*>(user_data);

	agent->createInterface(connection);
}

void Bluez5Agent::createInterface(GDBusConnection *connection)
{
	mInterface = bluez_agent1_skeleton_new();

	GError *error = 0;

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(mInterface), connection,
                                          mPath.c_str(), &error))
	{
		ERROR(MSGID_AGENT_INIT_ERROR, 0, "Failed to initialize agent on bus: %s", error->message);
		g_error_free(error);
		return;
	}

	std::string pairingIOCap = convertPairingIOCapability(mCapability);
	if (pairingIOCap.empty())
	{
		ERROR(MSGID_PAIRING_IO_CAPABILITY_ERROR, 0, "Failed to get valid pairing IO capability, cannot create bluez5 interface");
		return;
	}

	bluez_agent_manager1_call_register_agent_sync(mAgentManager, mPath.c_str(),
                                                  pairingIOCap.c_str(), NULL, &error);

	if(error)
	{
		ERROR("MSGID_AGENT_REGISTER_ERROR", 0, "Failed to register agent with bluez: %s", error->message);
		g_error_free(error);
		return;
	}

	bluez_agent_manager1_call_request_default_agent_sync(mAgentManager, mPath.c_str(), NULL, &error);
	if(error)
	{
		ERROR("MSGID_AGENT_DEFAULT_ERROR", 0, "Failed to make agent the default one: %s", error->message);
		g_error_free(error);
		return;
	}

	g_signal_connect(mInterface, "handle-request-confirmation", G_CALLBACK(handleRequestConfirmation), this);
	g_signal_connect(mInterface, "handle-request-passkey", G_CALLBACK(handleRequestPasskey), this);
	g_signal_connect(mInterface, "handle-request-pin-code", G_CALLBACK(handleRequestPinCode), this);
	g_signal_connect(mInterface, "handle-display-passkey", G_CALLBACK(handleDisplayPasskey), this);
	g_signal_connect(mInterface, "handle-display-pin-code", G_CALLBACK(handleDisplayPinCode), this);
	g_signal_connect(mInterface, "handle-request-authorization", G_CALLBACK(handleRequestAuthorization), this);
	g_signal_connect(mInterface, "handle-authorize-service", G_CALLBACK(handleAuthorizeService), this);
	g_signal_connect(mInterface, "handle-cancel", G_CALLBACK(handleCancel), this);
	g_signal_connect(mInterface, "handle-release", G_CALLBACK(handleRelease), this);

	g_object_unref(connection);
}

Bluez5AgentPairingInfo* Bluez5Agent::findPairingInfoForDevice(const std::string &objectPath)
{
	if(objectPath.empty())
	{
		return 0;
	}

	auto iter = mDevicePairings.find(objectPath);
	if (iter == mDevicePairings.end())
	{
		return 0;
	}

	return iter->second;
}

Bluez5AgentPairingInfo* Bluez5Agent::findPairingInfoForAddress(const std::string &address)
{
	if(address.empty())
	{
		return 0;
	}

	std::string upperCaseAddress = convertAddressToUpperCase(address);
	Bluez5AgentPairingInfo *pairingInfo = 0;

	for (auto iter = mDevicePairings.begin(); iter != mDevicePairings.end(); ++iter)
	{
		auto currentPairingInfo = (*iter).second;
		if (currentPairingInfo->deviceAddress == upperCaseAddress)
		{
			pairingInfo = currentPairingInfo;
			break;
		}
	}
	return pairingInfo;
}

Bluez5AgentPairingInfo* Bluez5Agent::initiatePairing(GDBusMethodInvocation *invocation, const gchar *objectPath)
{
	auto pairingInfo = findPairingInfoForDevice(objectPath);
	if (!pairingInfo)
	{
		DEBUG("Not active pairing attempt. Assuming it's an incoming request");

		// As we don't have an entry for the device yet we got a
		// pairing request from a remote device
		Bluez5Adapter *defaultAdapter = mSIL->getDefaultBluez5Adapter();
		if (!defaultAdapter)
		{
			DEBUG ("default adapter is not set");
			return NULL;
		}

		Bluez5Device* device = defaultAdapter->findDeviceByObjectPath(objectPath);
		if (!device || !startPairingForDevice(device, true))
		{
			DEBUG("Failed to handle incoming pairing request");
			g_dbus_method_invocation_return_dbus_error(invocation, BLUEZ5_AGENT_ERROR_CANCELED,
													   "Not able to start paring process");
			return NULL;
		}
	}

	pairingInfo = findPairingInfoForDevice(objectPath);
	if (!pairingInfo)
	{
		DEBUG("Failed to find active pairing attempt");

		g_dbus_method_invocation_return_dbus_error(invocation, BLUEZ5_AGENT_ERROR_CANCELED,
												   "Not pairing with device");
		return NULL;
	}
	return pairingInfo;
}

gboolean Bluez5Agent::handleRelease(BluezAgent1 *proxy, GDBusMethodInvocation *invocation, gpointer user_data)
{
	DEBUG("Agent release method was called");
	g_dbus_method_invocation_return_dbus_error(invocation, BLUEZ5_AGENT_ERROR_NOT_IMPLEMENTED, "Not implemented yet");
	return FALSE;
}

gboolean Bluez5Agent::handleCancel(BluezAgent1 *proxy, GDBusMethodInvocation *invocation, gpointer user_data)
{
	DEBUG("Agent cancel method was called");

	Bluez5Agent *agent = static_cast<Bluez5Agent*>(user_data);

	Bluez5Adapter *defaultAdapter = agent->mSIL->getDefaultBluez5Adapter();
	if (defaultAdapter && defaultAdapter->isPairing())
	{
		BluetoothAdapterStatusObserver *observer = defaultAdapter->getObserver();
		if (observer)
			observer->pairingCanceled();
	}

	bluez_agent1_complete_cancel(agent->mInterface, invocation);

	return TRUE;
}

gboolean Bluez5Agent::handleAuthorizeService(BluezAgent1 *proxy, GDBusMethodInvocation *invocation,
                                         const gchar *address, const gchar *service, gpointer user_data)
{
	DEBUG("Agent authorize service method was called");
	g_dbus_method_invocation_return_dbus_error(invocation, BLUEZ5_AGENT_ERROR_NOT_IMPLEMENTED, "Not implemented yet");

	return FALSE;
}

gboolean Bluez5Agent::handleRequestAuthorization(BluezAgent1 *proxy, GDBusMethodInvocation *invocation,
                                                 const gchar *address, gpointer user_data)
{
	DEBUG("Agent request authorize method was called");
	g_dbus_method_invocation_return_dbus_error(invocation, BLUEZ5_AGENT_ERROR_NOT_IMPLEMENTED, "Not implemented yet");

	return FALSE;
}

gboolean Bluez5Agent::handleDisplayPasskey(BluezAgent1 *proxy, GDBusMethodInvocation *invocation,
                                           const gchar *objectPath, const guint passkey, const guint passkey2,
                                           gpointer user_data)
{
	DEBUG("Agent display passkey method was called");
	Bluez5Agent *agent = static_cast<Bluez5Agent*>(user_data);

	Bluez5AgentPairingInfo* pairingInfo = agent->initiatePairing(invocation, objectPath);
	if (!pairingInfo)
		return FALSE;

	pairingInfo->displayPairingSecret = invocation;

	BluetoothAdapterStatusObserver *observer = pairingInfo->adapter->getObserver();
	if (observer)
	{
		observer->displayPairingSecret(pairingInfo->deviceAddress, passkey);
	}

	bluez_agent1_complete_display_passkey(proxy, invocation);
	return TRUE;
}

gboolean Bluez5Agent::handleDisplayPinCode(BluezAgent1 *proxy, GDBusMethodInvocation *invocation,
                                           const gchar *objectPath, const gchar *pincode, gpointer user_data)
{
	DEBUG("Agent display pincode method was called");

	Bluez5Agent *agent = static_cast<Bluez5Agent*>(user_data);
	Bluez5AgentPairingInfo* pairingInfo = agent->initiatePairing(invocation, objectPath);

	if (!pairingInfo)
		return FALSE;

	pairingInfo->displayPairingSecret = invocation;

	BluetoothAdapterStatusObserver *observer = pairingInfo->adapter->getObserver();
	if (observer)
	{
		observer->displayPairingSecret(pairingInfo->deviceAddress, pincode);
	}

	bluez_agent1_complete_display_pin_code(proxy, invocation);

	return TRUE;
}

gboolean Bluez5Agent::handleRequestConfirmation(BluezAgent1 *proxy, GDBusMethodInvocation *invocation,
                                                const gchar *objectPath, const guint passkey, gpointer user_data)
{
	DEBUG("Agent request confirmation method was called: objectPath %s passkey %d",
           objectPath, passkey);

	Bluez5Agent *agent = static_cast<Bluez5Agent*>(user_data);
	Bluez5AgentPairingInfo* pairingInfo = agent->initiatePairing(invocation, objectPath);

	if (!pairingInfo)
		return FALSE;

	pairingInfo->requestConfirmation = invocation;

	BluetoothAdapterStatusObserver *observer = pairingInfo->adapter->getObserver();

	if (observer)
	{
		DEBUG("Telling observer about confirmation request");
		observer->displayPairingConfirmation(pairingInfo->deviceAddress, passkey);
	}

	return TRUE;
}

gboolean Bluez5Agent::handleRequestPasskey(BluezAgent1 *proxy, GDBusMethodInvocation *invocation,
                                           const gchar *objectPath, gpointer user_data)
{
	DEBUG("Agent request passkey method was called");

	Bluez5Agent *agent = static_cast<Bluez5Agent*>(user_data);
	Bluez5AgentPairingInfo* pairingInfo = agent->initiatePairing(invocation, objectPath);

	if (!pairingInfo)
		return FALSE;

	pairingInfo->requestPairingSecret = invocation;

	BluetoothAdapterStatusObserver *observer = pairingInfo->adapter->getObserver();

	if (observer)
	{
		DEBUG ("Calling observer requestPairingSecret for device address %s", pairingInfo->deviceAddress.c_str());
		observer->requestPairingSecret(pairingInfo->deviceAddress, BLUETOOTH_PAIRING_SECRET_TYPE_PASSKEY);
	}

	return TRUE;
}

gboolean Bluez5Agent::handleRequestPinCode(BluezAgent1 *proxy, GDBusMethodInvocation *invocation,
                                           const gchar *objectPath, gpointer user_data)
{
	DEBUG("Agent request pincode method was called");

	Bluez5Agent *agent = static_cast<Bluez5Agent*>(user_data);
	Bluez5AgentPairingInfo* pairingInfo = agent->initiatePairing(invocation, objectPath);

	if (!pairingInfo)
		return FALSE;

	pairingInfo->requestPairingSecret = invocation;

	BluetoothAdapterStatusObserver *observer = pairingInfo->adapter->getObserver();
	if (observer)
	{
		DEBUG ("Calling observer requestPairingSecret for device address %s", pairingInfo->deviceAddress.c_str());
		observer->requestPairingSecret(pairingInfo->deviceAddress, BLUETOOTH_PAIRING_SECRET_TYPE_PIN);
	}

	return TRUE;
}

bool Bluez5Agent::startPairingForDevice(Bluez5Device *device, bool incoming)
{
	if (!device)
	{
		DEBUG("Could not start to pair, device does not exist");
		return false;
	}

	DEBUG("Start paring with %s", device->getAddress().c_str());

	auto iter = mDevicePairings.find(device->getObjectPath());
	if (iter != mDevicePairings.end())
	{
		DEBUG("Pairing attempt already exists for device %s", device->getAddress().c_str());
		return false;
	}

	Bluez5AgentPairingInfo *pairingInfo = new Bluez5AgentPairingInfo(device->getAdapter(), device->getAddress());
	pairingInfo->incoming = incoming;

	mDevicePairings.insert(std::pair<std::string,Bluez5AgentPairingInfo*>(device->getObjectPath(), pairingInfo));

	device->getAdapter()->setPairing(true);

	return true;
}

void Bluez5Agent::stopPairingForDevice(const std::string &address)
{
	Bluez5Adapter *defaultAdapter = mSIL->getDefaultBluez5Adapter();
	if (!defaultAdapter)
		return;

	Bluez5Device *device = defaultAdapter->findDevice(address);
	if (!device)
		return;

	stopPairingForDevice(device);
}

void Bluez5Agent::stopPairingForDevice(Bluez5Device *device)
{
	if (!device)
		return;

	DEBUG("Stop pairing with %s", device->getAddress().c_str());

	auto iter = mDevicePairings.find(device->getObjectPath());
	if (iter == mDevicePairings.end())
	{
		DEBUG("Pairing attempt for device %s does not exists", device->getAddress().c_str());
		return;
	}

	delete iter->second;

	mDevicePairings.erase(iter);

	device->getAdapter()->setPairing(false);
}

bool Bluez5Agent::supplyPairingConfirmation(const std::string &address, bool accept)
{
	auto pairingInfo = findPairingInfoForAddress(address);

	DEBUG("supplyPairingConfirmation: address %s accept %d", address.c_str(), accept);

	if (!pairingInfo || !pairingInfo->requestConfirmation)
	{
		DEBUG("Missing information to finish pairing attempt");
		return false;
	}

	if (!accept)
	{
		g_dbus_method_invocation_return_dbus_error(pairingInfo->requestConfirmation,
                                                           BLUEZ5_AGENT_ERROR_REJECTED, "User rejected confirmation");
	}
	else
	{
		bluez_agent1_complete_request_confirmation(mInterface, pairingInfo->requestConfirmation);
	}

	// For incoming pairing requests we can remove the pairing info here
	// and for outgoing ones it will be removed once the pair callback comes
	// back from bluez
	if (pairingInfo->incoming)
	{
		Bluez5Adapter *defaultAdapter = mSIL->getDefaultBluez5Adapter();
		if (!defaultAdapter)
			return false;

		Bluez5Device *device = defaultAdapter->findDevice(address);
		if (!device)
			return false;

		stopPairingForDevice(device);

		// We're done with pairing at this point. Everything else is up to the remote
		// party and will notify our layer on top through a property change of the
		// newly paired device.
	}

	return true;
}

bool Bluez5Agent::supplyPairingSecret(const std::string &address, BluetoothPasskey passkey)
{
	auto pairingInfo = findPairingInfoForAddress(address);

	if (!(pairingInfo && pairingInfo->requestPairingSecret))
		return false;

	bluez_agent1_complete_request_passkey(mInterface, pairingInfo->requestPairingSecret, passkey);

	return true;
}

bool Bluez5Agent::supplyPairingSecret(const std::string &address, const std::string &pin)
{
	auto pairingInfo = findPairingInfoForAddress(address);

	if (!(pairingInfo && pairingInfo->requestPairingSecret))
		return false;

	if (pin.empty())
		return false;

	bluez_agent1_complete_request_pin_code(mInterface, pairingInfo->requestPairingSecret, pin.c_str());

	return true;
}

bool Bluez5Agent::cancelPairing(const std::string address)
{
	auto pairingInfo = findPairingInfoForAddress(address);
	GDBusMethodInvocation *invocation = 0;
	if (pairingInfo)
	{
		if (pairingInfo->requestConfirmation)
			invocation = pairingInfo->requestConfirmation;
		else if (pairingInfo->requestAuthorization)
			invocation = pairingInfo->requestAuthorization;
		else if (pairingInfo->requestPairingSecret)
			invocation = pairingInfo->requestPairingSecret;

		if (invocation)
		{
			DEBUG("Sending cancel signal to remote device");
			g_dbus_method_invocation_return_dbus_error(invocation, BLUEZ5_AGENT_ERROR_CANCELED,
                                                                   "Pairing canceled by user");
			return true;
		}
	}
	return false;
}

