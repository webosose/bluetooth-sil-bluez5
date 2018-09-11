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

#ifndef BLUEZ5OBEXSESSION_H
#define BLUEZ5OBEXSESSION_H

#include <string>
#include <functional>

extern "C" {
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5ObexClient;

namespace DBusUtils
{
	class ObjectWatch;
}

typedef std::function<void(bool)> Bluez5ObexSessionStatusCallback;

class Bluez5ObexSession
{
public:
	enum Type
	{
		FTP,
		MAP,
		OPP,
		PBAP,
		SYNC
	};

	Bluez5ObexSession(Bluez5ObexClient *client, Type type, const std::string &objectPath, const std::string &deviceAddress);
	~Bluez5ObexSession();

	Type getType() const { return mType; }
	std::string getDeviceAddress() const { return mDeviceAddress; }
	std::string getObjectPath() const { return mObjectPath; }

	BluezObexFileTransfer1* getFileTransferProxy() const { return mFileTransferProxy; }

	BluezObexObjectPush1* getObjectPushProxy() const { return mObjectPushProxy; }

	void watch(Bluez5ObexSessionStatusCallback callback);

private:
	Bluez5ObexClient *mClient;
	Type mType;
	std::string mObjectPath;
	std::string mDeviceAddress;
	BluezObexSession1 *mSessionProxy;
	BluezObexFileTransfer1 *mFileTransferProxy;
	BluezObexObjectPush1 *mObjectPushProxy;
	bool mLostRemote;
	DBusUtils::ObjectWatch *mObjectWatch;
	Bluez5ObexSessionStatusCallback mStatusCallback;
};

#endif // BLUEZ5OBEXSESSION_H
