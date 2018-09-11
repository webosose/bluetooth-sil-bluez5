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

const std::string BLUETOOTH_PROFILE_OPP_UUID = "00001105-0000-1000-8000-00805f9b34fb";

Bluez5ProfileOpp::Bluez5ProfileOpp(Bluez5Adapter *adapter) :
	Bluez5ObexProfileBase(adapter, BLUETOOTH_PROFILE_OPP_UUID),
	mTranfserIdCounter(0)
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
