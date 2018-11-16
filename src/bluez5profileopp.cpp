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

#include "bluez5profileopp.h"
#include "bluez5adapter.h"
#include "logging.h"
#include "asyncutils.h"
#include "bluez5obextransfer.h"
#include "bluez5obexsession.h"
#include "bluez5obexclient.h"
#include "bluez5busconfig.h"

using namespace std::placeholders;

const std::string BLUETOOTH_PROFILE_OPP_UUID = "00001105-0000-1000-8000-00805f9b34fb";

#define BLUEZ5_OBEX_AGENT_ERROR_CANCELED        "org.bluez.Error.Canceled"
#define BLUEZ5_OBEX_AGENT_ERROR_REJECTED        "org.bluez.Error.Rejected"
#define BLUEZ5_OBEX_AGENT_ERROR_NOT_IMPLEMENTED "org.bluez.Error.NotImplemented"

Bluez5ProfileOpp::Bluez5ProfileOpp(Bluez5Adapter *adapter) :
	Bluez5ObexProfileBase(adapter, BLUETOOTH_PROFILE_OPP_UUID),
	mTranfserIdCounter(0),
	mInvocation(nullptr)
{
}

Bluez5ProfileOpp::~Bluez5ProfileOpp()
{
}

BluetoothOppTransferId Bluez5ProfileOpp::pushFile(const std::string &address,
								const std::string &sourcePath,
								BluetoothOppTransferResultCallback callback)
{
	const Bluez5ObexSession *session = findSession(address);
	if (!session || sourcePath.length() == 0)
	{
		callback(BLUETOOTH_ERROR_PARAM_INVALID, 0, 0, false);
		return BLUETOOTH_OPP_TRANSFER_ID_INVALID;
	}

	BluezObexObjectPush1 *objectPushProxy = session->getObjectPushProxy();
	if (!objectPushProxy)
	{
		callback(BLUETOOTH_ERROR_FAIL, 0, 0, false);
		return BLUETOOTH_OPP_TRANSFER_ID_INVALID;
	}

	BluetoothOppTransferId transferId = nextTransferId();

	auto pushCallback = [this, objectPushProxy, sourcePath, transferId, callback](GAsyncResult *result) {
		GError *error = 0;
		gboolean ret;

		char *objectPath = 0;
		ret = bluez_obex_object_push1_call_send_file_finish(objectPushProxy, &objectPath , NULL, result, &error);
		if (error)
		{
			g_error_free(error);
			callback(BLUETOOTH_ERROR_FAIL, 0, 0, false);
			return;
		}

		startTransfer(transferId, std::string(objectPath), callback);
	};

	bluez_obex_object_push1_call_send_file(objectPushProxy, sourcePath.c_str(),
												NULL, glibAsyncMethodWrapper,
												new GlibAsyncFunctionWrapper(pushCallback));

	return transferId;
}

void Bluez5ProfileOpp::cancelTransfer(BluetoothOppTransferId id, BluetoothResultCallback callback)
{
	Bluez5ObexTransfer *transfer = findTransfer(id);
	if (!transfer)
	{
		callback(BLUETOOTH_ERROR_PARAM_INVALID);
		return;
	}

	auto cancelCallback = [this, id, callback](BluetoothError error) {
		if (error != BLUETOOTH_ERROR_NONE)
		{
			callback(error);
			return;
		}

		removeTransfer(id);

		callback(BLUETOOTH_ERROR_NONE);
	};

	transfer->cancel(cancelCallback);
}

void Bluez5ProfileOpp::agentTransferConfirmationRequested(BluezObexAgent1 *interface, GDBusMethodInvocation *invocation, const gchar *arg_path)
{
	mInterface = interface;
	mInvocation = invocation;
	mTransferObjPath = std::string(arg_path);
	BluetoothOppTransferId transferId = nextTransferId();

	GError *error = 0;
	BluezObexTransfer1* mTransFerProxy = bluez_obex_transfer1_proxy_new_for_bus_sync(BLUEZ5_OBEX_DBUS_BUS_TYPE, G_DBUS_PROXY_FLAGS_NONE,
	                                                           "org.bluez.obex", arg_path, NULL, &error);
	if (error)
	{
		g_dbus_method_invocation_return_dbus_error(mInvocation, BLUEZ5_OBEX_AGENT_ERROR_REJECTED, "User rejected confirmation");
		mInvocation = nullptr;
		g_error_free(error);
		return;
	}

	const gchar * sessionPath = bluez_obex_transfer1_get_session(mTransFerProxy);
	const char* fileName = bluez_obex_transfer1_get_name(mTransFerProxy);
	if (!fileName)
	{
		/*work around, if we cancel ongoing transfer then continuously handle-confirmation is called
		 so closing session to avoid it*/
		mAdapter->getObexClient()->destroySession(sessionPath);
		return;
	}

	mFileName = fileName;
	guint64 size = bluez_obex_transfer1_get_size(mTransFerProxy);

	BluezObexSession1* mSessionProxy = bluez_obex_session1_proxy_new_for_bus_sync(BLUEZ5_OBEX_DBUS_BUS_TYPE, G_DBUS_PROXY_FLAGS_NONE,
	                                                           "org.bluez.obex", sessionPath, NULL, &error);
	if (error)
	{
		g_dbus_method_invocation_return_dbus_error(mInvocation, BLUEZ5_OBEX_AGENT_ERROR_REJECTED, "User rejected confirmation");
		mInvocation = nullptr;
		g_error_free(error);
		return;
	}

	std::string deviceAddress = bluez_obex_session1_get_destination(mSessionProxy);
	auto device = mAdapter->findDevice(deviceAddress);

	if (!device)
	{
		g_dbus_method_invocation_return_dbus_error(mInvocation, BLUEZ5_OBEX_AGENT_ERROR_REJECTED, "User rejected confirmation");
		mInvocation = nullptr;
		return;
	}
	else
	{
		Bluez5ObexSession *session = new Bluez5ObexSession(mAdapter->getObexClient(), Bluez5ObexSession::Type::OPP, std::string(sessionPath), deviceAddress);
		session->watch(std::bind(&Bluez5ObexProfileBase::handleObexSessionStatus, this, deviceAddress, _1));
		storeSession(deviceAddress, session);
		notifySessionStatus(deviceAddress, true);
		mTransfersMap[transferId] = 0;
		getOppObserver()->transferConfirmationRequested(transferId, deviceAddress, device->getName(), mFileName, size);
	}

	g_object_unref(mSessionProxy);
	g_object_unref(mTransFerProxy);
}

void Bluez5ProfileOpp::supplyTransferConfirmation(BluetoothOppTransferId transferId, bool accept, BluetoothResultCallback callback)
{
	if (accept)
	{
		auto resultCallBack = [this, transferId, callback](BluetoothError error, uint64_t bytesTransferred,
														   uint64_t totalSize, bool finished)
		{
			if (mTransfersMap[transferId])
				getOppObserver()->transferStateChanged(transferId, bytesTransferred - mTransfersMap[transferId], finished);
			else
				getOppObserver()->transferStateChanged(transferId, bytesTransferred, finished);

			if (finished)
				mTransfersMap[transferId] = 0;
			else
				mTransfersMap[transferId] = bytesTransferred;
		};

		startTransfer(transferId, mTransferObjPath, resultCallBack);

		if (mInvocation)
		{
			bluez_obex_agent1_complete_authorize_push(mInterface, mInvocation, mFileName.c_str());
			mInvocation = nullptr;
		}
	}
	else
	{
		g_dbus_method_invocation_return_dbus_error(mInvocation, BLUEZ5_OBEX_AGENT_ERROR_REJECTED, "User rejected confirmation");
		mInvocation = nullptr;
	}

	callback(BLUETOOTH_ERROR_NONE);

}
