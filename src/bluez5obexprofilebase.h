// Copyright (c) 2018-2020 LG Electronics, Inc.
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

#ifndef BLUEZ5OBEXPROFILEBASE_H
#define BLUEZ5OBEXPROFILEBASE_H

#include <string>

#include <bluetooth-sil-api.h>

#include "bluez5profilebase.h"
#include "bluez5obexsession.h"
#include "bluez5obextransfer.h"

class Bluez5Adapter;
class Bluez5ObexTransfer;

class Bluez5ObexProfileBase : public Bluez5ProfileBase
{
public:
	Bluez5ObexProfileBase(Bluez5ObexSession::Type type,Bluez5Adapter *adapter, const std::string &uuid);
	virtual ~Bluez5ObexProfileBase() = default;

	void getProperties(const std::string &address, BluetoothPropertiesResultCallback callback);
	void getProperty(const std::string &address, BluetoothProperty::Type type, BluetoothPropertyResultCallback callback);

	void connect(const std::string &address, BluetoothResultCallback callback);
	void disconnect(const std::string& address, BluetoothResultCallback callback);
	void handleObexSessionStatus(const std::string &address, bool lost);
	virtual void updateProperties(GVariant *changedProperties);
	static void handlePropertiesChanged(BluezObexSession1 *, gchar *interface,  GVariant *changedProperties, GVariant *invalidatedProperties, gpointer userData);
	virtual void notifySessionStatus(const std::string &address, bool createdOrRemoved);

private:

	std::map<std::string, Bluez5ObexSession*> mSessions;
	std::map<BluetoothFtpTransferId, Bluez5ObexTransfer*> mTransfers;
	Bluez5ObexSession::Type mType;

protected:
	void createSession(const std::string &address, Bluez5ObexSession::Type type, BluetoothResultCallback callback);
	void removeSession(const std::string &address);
	void storeSession(const std::string &address, Bluez5ObexSession *session);
	const Bluez5ObexSession* findSession(const std::string &address) const;
	void handleFailedToCreateSession(const std::string &address, BluetoothResultCallback callback);
	void startTransfer(BluetoothFtpTransferId id, const std::string &objectPath, BluetoothOppTransferResultCallback callback, Bluez5ObexTransfer::TransferType type);
	void updateActiveTransfer(BluetoothFtpTransferId id, Bluez5ObexTransfer *transfer, BluetoothOppTransferResultCallback callback);
	void removeTransfer(BluetoothFtpTransferId id);
	Bluez5ObexTransfer* findTransfer(BluetoothFtpTransferId id);
	void removeFromSessionList(std::string address);
};

#endif
