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

#include <memory.h>

#include "bluez5obexprofilebase.h"
#include "bluez5adapter.h"
#include "bluez5obexclient.h"
#include "bluez5obexsession.h"
#include "bluez5obextransfer.h"
#include "asyncutils.h"
#include "logging.h"
#include "utils.h"

using namespace std::placeholders;

Bluez5ObexProfileBase::Bluez5ObexProfileBase(Bluez5ObexSession::Type type, Bluez5Adapter *adapter, const std::string &uuid) :
	Bluez5ProfileBase(adapter, uuid),
	mType(type)
{
}

void Bluez5ObexProfileBase::notifySessionStatus(const std::string &address, bool createdOrRemoved)
{
	BluetoothPropertiesList properties;
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::CONNECTED, createdOrRemoved));

	getObserver()->propertiesChanged(convertAddressToLowerCase(mAdapter->getAddress()), convertAddressToLowerCase(address), properties);
}

void Bluez5ObexProfileBase::handleFailedToCreateSession(const std::string &address, BluetoothResultCallback callback)
{
	auto disconnectCallback = [this, address, callback](BluetoothError error) {
		DEBUG("Connecting with device %s on uuid %s profile", address.c_str(), getProfileUuid().c_str());
		// We always return a failure here that is initial thing someone
		// called this method for
		callback(BLUETOOTH_ERROR_FAIL);
	};

	disconnect(address, disconnectCallback);
}

void Bluez5ObexProfileBase::removeSession(const std::string &address)
{
	auto sessionIter = mSessions.find(address);
	if (sessionIter == mSessions.end())
		return;

	Bluez5ObexSession *session = sessionIter->second;
	if (!session)
		return;

	for (auto transferIter = mTransfers.begin(); transferIter != mTransfers.end(); )
	{
		Bluez5ObexTransfer *transfer = transferIter->second;

		if (transfer->isPartOfSession(session))
		{
			// We don't have to cancel the transfer here. If the session goes down
			// the transfer will too. This will also trigger a possible watcher
			// of the transfer so he gets notified about the aborted transfer.
			mTransfers.erase(transferIter++);
			delete transfer;

			continue;
		}

		transferIter++;
	}

	mSessions.erase(sessionIter);

	notifySessionStatus(address, false);

	// This will unregister the session with the obex daemon
	delete session;
}

void Bluez5ObexProfileBase::createSession(const std::string &address, Bluez5ObexSession::Type type, BluetoothResultCallback callback)
{
	Bluez5ObexClient *obexClient = mAdapter->getObexClient();

	if (!obexClient)
	{
		handleFailedToCreateSession(address, callback);
		return;
	}

	auto sessionCreateCallback = [this, address, callback](Bluez5ObexSession *session) {
		if (!session)
		{
			handleFailedToCreateSession(address, callback);
			return;
		}

		session->watch(std::bind(&Bluez5ObexProfileBase::handleObexSessionStatus, this, address, _1));

		storeSession(address, session);
		notifySessionStatus(address, true);

		callback(BLUETOOTH_ERROR_NONE);
	};

	obexClient->createSession(type, address, sessionCreateCallback);
}

void Bluez5ObexProfileBase::handleObexSessionStatus(const std::string &address, bool lost)
{
	if (!lost)
		return;

	DEBUG("Session lost for address %s", address.c_str());

	// At this point we lost the session due to some internal bluez error
	// and need power down everything.
	removeSession(address);
}

void Bluez5ObexProfileBase::storeSession(const std::string &address, Bluez5ObexSession *session)
{
	mSessions.insert(std::pair<std::string, Bluez5ObexSession*>(address, session));
}

void Bluez5ObexProfileBase::connect(const std::string &address, BluetoothResultCallback callback)
{
	DEBUG("Connecting with device %s on uuid %s profile", address.c_str(), getProfileUuid().c_str());

	createSession(address, mType, callback);
}

void Bluez5ObexProfileBase::disconnect(const std::string& address, BluetoothResultCallback callback)
{
	DEBUG("Disconnecting from device %s on uuid %s profile", address.c_str(), getProfileUuid().c_str());

	callback(BLUETOOTH_ERROR_NONE);

	removeSession(address);
}

void Bluez5ObexProfileBase::getProperties(const std::string &address, BluetoothPropertiesResultCallback callback)
{
	BluetoothPropertiesList properties;

	callback(BLUETOOTH_ERROR_UNHANDLED, properties);
}

void Bluez5ObexProfileBase::getProperty(const std::string &address, BluetoothProperty::Type type, BluetoothPropertyResultCallback callback)
{
	bool sessionExists = false;
	BluetoothProperty prop(type);

	auto sessionIter = mSessions.find(address);

	switch (type)
	{
	case BluetoothProperty::Type::CONNECTED:
		sessionExists = (sessionIter != mSessions.end());
		prop.setValue<bool>(sessionExists);
		break;
	default:
		callback(BLUETOOTH_ERROR_PARAM_INVALID, prop);
		return;
	}

	callback(BLUETOOTH_ERROR_NONE, prop);
}

const Bluez5ObexSession* Bluez5ObexProfileBase::findSession(const std::string &address) const
{
	auto sessionIter = mSessions.find(address);
	if (sessionIter == mSessions.end())
		return nullptr;

	return sessionIter->second;
}

void Bluez5ObexProfileBase::startTransfer(BluetoothFtpTransferId id, const std::string &objectPath, BluetoothOppTransferResultCallback callback, Bluez5ObexTransfer::TransferType type)
{
	// NOTE: ownership of the transfer object is passed to updateActiveTransfer which
	// will delete it once there is nothing to left to do with it
	Bluez5ObexTransfer *transfer = new Bluez5ObexTransfer(std::string(objectPath), type);
	mTransfers.insert(std::pair<BluetoothFtpTransferId, Bluez5ObexTransfer*>(id, transfer));
	transfer->watch(std::bind(&Bluez5ObexProfileBase::updateActiveTransfer, this, id, transfer, callback));
}

void Bluez5ObexProfileBase::updateActiveTransfer(BluetoothFtpTransferId id, Bluez5ObexTransfer *transfer, BluetoothOppTransferResultCallback callback)
{
	bool cleanup = false;

	if (transfer->getState() == Bluez5ObexTransfer::State::ACTIVE)
	{
		callback(BLUETOOTH_ERROR_NONE, transfer->getBytesTransferred(), transfer->getFileSize(), false);
	}
	else if (transfer->getState() == Bluez5ObexTransfer::State::COMPLETE && transfer->getBytesTransferred())
	{
		callback(BLUETOOTH_ERROR_NONE, transfer->getBytesTransferred(), transfer->getFileSize(), true);
		cleanup = true;
	}
	else if (transfer->getState() == Bluez5ObexTransfer::State::ERROR)
	{
		DEBUG("File transfer failed");
		callback(BLUETOOTH_ERROR_FAIL, transfer->getBytesTransferred(), transfer->getFileSize(), false);
		cleanup = true;
	}

	if (cleanup)
		removeTransfer(id);
}

void Bluez5ObexProfileBase::removeTransfer(BluetoothFtpTransferId id)
{
	auto transferIter = mTransfers.find(id);
	if (transferIter == mTransfers.end())
		return;

	Bluez5ObexTransfer *transfer = transferIter->second;

	mTransfers.erase(transferIter);

	delete transfer;
}

Bluez5ObexTransfer* Bluez5ObexProfileBase::findTransfer(BluetoothFtpTransferId id)
{
	auto transferIter = mTransfers.find(id);
	if (transferIter == mTransfers.end())
		return 0;

	Bluez5ObexTransfer *transfer = transferIter->second;

	return transfer;
}
