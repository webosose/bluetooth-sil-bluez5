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

#ifndef BLUEZ5OBEXTRANSFER_H
#define BLUEZ5OBEXTRANSFER_H

#include <string>
#include <functional>

#include <bluetooth-sil-api.h>

extern "C" {
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

typedef std::function<void()> Bluez5ObexTransferWatchCallback;

class Bluez5ObexSession;

class Bluez5ObexTransfer
{
public:
	enum State
	{
		INACTIVE,
		QUEUED,
		ACTIVE,
		SUSPENDED,
		COMPLETE,
		ERROR
	};

	enum TransferType
	{
		SENDING,
		RECEIVING
	};

	Bluez5ObexTransfer(const std::string &objectPath, TransferType type = RECEIVING);
	~Bluez5ObexTransfer();

	Bluez5ObexTransfer(const Bluez5ObexTransfer&) = delete;
	Bluez5ObexTransfer& operator = (const Bluez5ObexTransfer&) = delete;

	void cancel(BluetoothResultCallback callback);

	void watch(Bluez5ObexTransferWatchCallback callback);

	bool isPartOfSession(Bluez5ObexSession *session);

	uint64_t getBytesTransferred() const { return mBytesTransferred; }
	State getState() const { return mState; }
	uint64_t getFileSize() const { return mFileSize; }
	const std::string& getFileName() const { return mFileName; }
	const std::string& getFilePath() const { return mFilePath; }

public:
	static void handlePropertiesChanged(BluezObexTransfer1 *, gchar *interface,  GVariant *changedProperties,
															GVariant *invalidatedProperties, gpointer userData);

private:
	std::string mObjectPath;
	BluezObexTransfer1 *mTransferProxy;
	FreeDesktopDBusProperties *mPropertiesProxy;
	Bluez5ObexTransferWatchCallback mWatchCallback;
	uint64_t mBytesTransferred;
	uint64_t mFileSize;
	State mState;
	TransferType mTransferType;
	std::string mFileName;
	std::string mFilePath;
	void updateFromProperties(GVariant *properties);
	bool parsePropertyFromVariant(const std::string &key, GVariant *valueVar);
	void notifyWatcherAboutChangedProperties();
};

#endif // BLUEZ5OBEXTRANSFER_H
