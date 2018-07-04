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

#ifndef BLUEZ5PROFILEFTP_H
#define BLUEZ5PROFILEFTP_H

#include <string>
#include <map>
#include <glib.h>

#include <bluetooth-sil-api.h>

#include "bluez5profilebase.h"

class Bluez5Adapter;
class Bluez5Device;
class Bluez5ObexSession;
class Bluez5ObexTransfer;

class Bluez5ProfileFtp : public Bluez5ProfileBase,
                         public BluetoothFtpProfile
{
public:
	Bluez5ProfileFtp(Bluez5Adapter *adapter);
	~Bluez5ProfileFtp();

	void getProperties(const std::string &address, BluetoothPropertiesResultCallback callback);
	void getProperty(const std::string &address, BluetoothProperty::Type type, BluetoothPropertyResultCallback callback);

	void connect(const std::string &address, BluetoothResultCallback callback);
	void disconnect(const std::string& address, BluetoothResultCallback callback);

	void listFolder(const std::string &address, const std::string &path,
				BluetoothFtpListFolderResultCallback callback);
	BluetoothFtpTransferId pullFile(const std::string &address, const std::string &sourcePath,
				const std::string &targetPath, BluetoothFtpTransferResultCallback callback);
	BluetoothFtpTransferId pushFile(const std::string &address, const std::string &sourcePath,
				const std::string &targetPath, BluetoothFtpTransferResultCallback callback);
	void cancelTransfer(BluetoothFtpTransferId id, BluetoothResultCallback callback);

private:
	std::map<std::string, Bluez5ObexSession*> mSessions;
	std::map<BluetoothFtpTransferId, Bluez5ObexTransfer*> mTransfers;
	uint64_t mTranfserIdCounter;

	inline uint64_t nextTransferId() { return ++mTranfserIdCounter; }

	void createSession(const std::string &address, BluetoothResultCallback callback);
	void removeSession(const std::string &address);
	void storeSession(const std::string &address, Bluez5ObexSession *session);
	void notifySessionStatus(const std::string &address, bool createdOrRemoved);
	void handleFailedToCreateSession(const std::string &address, BluetoothResultCallback callback);
	Bluez5ObexSession* findSession(const std::string &address);
	Bluez5ObexTransfer* findTransfer(BluetoothFtpTransferId id);
	std::vector<BluetoothFtpElement> buildElementList(GVariant *entries);
	void startTransfer(BluetoothFtpTransferId id, const std::string &objectPath, BluetoothFtpTransferResultCallback callback);
	void updateActiveTransfer(BluetoothFtpTransferId id, Bluez5ObexTransfer *transfer, BluetoothFtpTransferResultCallback callback);
	void removeTransfer(BluetoothFtpTransferId id);

	void handleObexSessionStatus(std::string address, bool lost);
};

#endif // BLUEZ5PROFILEFTP_H
