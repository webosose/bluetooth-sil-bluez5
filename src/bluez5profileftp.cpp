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

#include <memory.h>

#include "bluez5profileftp.h"
#include "bluez5adapter.h"
#include "bluez5obexclient.h"
#include "bluez5obexsession.h"
#include "bluez5obextransfer.h"
#include "asyncutils.h"
#include "logging.h"

using namespace std::placeholders;

const std::string BLUETOOTH_PROFILE_FTP_UUID = "00001106-0000-1000-8000-00805f9b34fb";

Bluez5ProfileFtp::Bluez5ProfileFtp(Bluez5Adapter *adapter) :
	Bluez5ProfileBase(adapter, BLUETOOTH_PROFILE_FTP_UUID),
	mTranfserIdCounter(0)
{
}

Bluez5ProfileFtp::~Bluez5ProfileFtp()
{
}

void Bluez5ProfileFtp::getProperties(const std::string &address, BluetoothPropertiesResultCallback callback)
{
	BluetoothPropertiesList properties;

	callback(BLUETOOTH_ERROR_UNHANDLED, properties);
}

void Bluez5ProfileFtp::getProperty(const std::string &address, BluetoothProperty::Type type, BluetoothPropertyResultCallback callback)
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

void Bluez5ProfileFtp::notifySessionStatus(const std::string &address, bool createdOrRemoved)
{
	if (!observer)
		return;

	BluetoothPropertiesList properties;
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::CONNECTED, createdOrRemoved));

	observer->propertiesChanged(address, properties);
}

void Bluez5ProfileFtp::handleFailedToCreateSession(const std::string &address, BluetoothResultCallback callback)
{
	auto disconnectCallback = [this, address, callback](BluetoothError error) {
		// We always return a failure here that is initial thing someone
		// called this method for
		callback(BLUETOOTH_ERROR_FAIL);
	};

	disconnect(address, disconnectCallback);
}

void Bluez5ProfileFtp::storeSession(const std::string &address, Bluez5ObexSession *session)
{
	mSessions.insert(std::pair<std::string, Bluez5ObexSession*>(address, session));
}

void Bluez5ProfileFtp::handleObexSessionStatus(std::string address, bool lost)
{
	if (!lost)
		return;

	DEBUG("Session lost");

	// At this point we lost the session due to some internal bluez error
	// and need power down everything.
	removeSession(address);
}

void Bluez5ProfileFtp::createSession(const std::string &address, BluetoothResultCallback callback)
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

		session->watch(std::bind(&Bluez5ProfileFtp::handleObexSessionStatus, this, address, _1));

		storeSession(address, session);
		notifySessionStatus(address, true);

		callback(BLUETOOTH_ERROR_NONE);
	};

	obexClient->createSession(Bluez5ObexSession::Type::FTP, address, sessionCreateCallback);
}

void Bluez5ProfileFtp::removeSession(const std::string &address)
{
	auto sessionIter = mSessions.find(address);
	if (sessionIter == mSessions.end())
		return;

	Bluez5ObexSession *session = sessionIter->second;

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

	// This will unregister the session with the obex daemon
	delete session;

	notifySessionStatus(address, false);
}

void Bluez5ProfileFtp::removeTransfer(BluetoothFtpTransferId id)
{
	auto transferIter = mTransfers.find(id);
	if (transferIter == mTransfers.end())
		return;

	Bluez5ObexTransfer *transfer = transferIter->second;

	mTransfers.erase(transferIter);

	delete transfer;
}

Bluez5ObexSession* Bluez5ProfileFtp::findSession(const std::string &address)
{
	auto sessionIter = mSessions.find(address);
	if (sessionIter == mSessions.end())
		return 0;

	Bluez5ObexSession *session = sessionIter->second;

	return session;
}

Bluez5ObexTransfer* Bluez5ProfileFtp::findTransfer(BluetoothFtpTransferId id)
{
	auto transferIter = mTransfers.find(id);
	if (transferIter == mTransfers.end())
		return 0;

	Bluez5ObexTransfer *transfer = transferIter->second;

	return transfer;
}

void Bluez5ProfileFtp::connect(const std::string &address, BluetoothResultCallback callback)
{
	DEBUG("Connecting with device %s on FTP profile", address.c_str());

	auto connectCallback = [this, address, callback](BluetoothError error) {
		if (error != BLUETOOTH_ERROR_NONE)
		{
			callback(error);
			return;
		}

		createSession(address, callback);
	};

	Bluez5ProfileBase::connect(address, connectCallback);
}

void Bluez5ProfileFtp::disconnect(const std::string &address, BluetoothResultCallback callback)
{
	DEBUG("Disconnecting from device %s on FTP profile", address.c_str());

	callback(BLUETOOTH_ERROR_NONE);

	removeSession(address);
}

static uint8_t decodePermissionString(const std::string &str)
{
	uint8_t permission = BluetoothFtpElement::Permission::NONE;

	if (str.find("R") != std::string::npos)
		permission |= BluetoothFtpElement::Permission::READ;
	if (str.find("W") != std::string::npos)
		permission |= BluetoothFtpElement::Permission::WRITE;
	if (str.find("D") != std::string::npos)
		permission |= BluetoothFtpElement::Permission::DELETE;

	return permission;
}

/**
 * Bluez encodes the times in the format "yyyymmddThhmmssZ" where T and Z are delimiters.
 */
static time_t decodeTimeString(const std::string &str)
{
	struct tm t;
	memset(&t, 0, sizeof(struct tm));

	sscanf(str.c_str(), "%4d%2d%2dT%2d%2d%2dZ",
		   &t.tm_year, &t.tm_mon, &t.tm_mday,
		   &t.tm_hour, &t.tm_min, &t.tm_sec);

	// tm_year counts the years from 1900 onwards
	t.tm_year = abs(t.tm_year - 1900);

	// We have to align the time ranges with the time we got from
	// bluez. struct tm normally has a range from 0 - ... and we
	// need to respect that here to get proper values.
	if (t.tm_mon > 0)
		t.tm_mon -= 1;
	if (t.tm_hour > 0)
		t.tm_hour -= 1;
	if (t.tm_min > 0)
		t.tm_min -= 1;
	if (t.tm_sec > 0)
		t.tm_sec -= 1;

	return mktime(&t);
}

std::vector<BluetoothFtpElement> Bluez5ProfileFtp::buildElementList(GVariant *entries)
{
	std::vector<BluetoothFtpElement> elements;

	for (int n = 0; n < g_variant_n_children(entries); n++)
	{
		GVariant *entry = g_variant_get_child_value(entries, n);
		BluetoothFtpElement currentElement;

		for (int m = 0; m < g_variant_n_children(entry); m++)
		{
			GVariant *propertyVar = g_variant_get_child_value(entry, m);

			GVariant *keyVar = g_variant_get_child_value(propertyVar, 0);
			GVariant *valueVar = g_variant_get_child_value(propertyVar, 1);
			GVariant *value = g_variant_get_variant(valueVar);

			std::string key = g_variant_get_string(keyVar, NULL);

			if (key == "Name")
			{
				std::string name = g_variant_get_string(value, NULL);
				currentElement.setName(name);
			}
			else if (key == "Type")
			{
				std::string type = g_variant_get_string(value, NULL);
				if (type == "folder")
					currentElement.setType(BluetoothFtpElement::Type::FOLDER);
				else if (type == "file")
					currentElement.setType(BluetoothFtpElement::Type::FILE);
			}
			else if (key == "Size")
			{
				currentElement.setSize(g_variant_get_uint64(value));
			}
			else if (key == "User-perm")
			{
				std::string permission = g_variant_get_string(value, NULL);
				currentElement.setUserPermission(decodePermissionString(permission));
			}
			else if (key == "Group-perm")
			{
				std::string permission = g_variant_get_string(value, NULL);
				currentElement.setGroupPermission(decodePermissionString(permission));
			}
			else if (key == "Other-perm")
			{
				std::string permission = g_variant_get_string(value, NULL);
				currentElement.setOtherPermission(decodePermissionString(permission));
			}
			else if (key == "Modified")
			{
				std::string modifiedTime = g_variant_get_string(value, NULL);
				currentElement.setModifiedTime(decodeTimeString(modifiedTime));
			}
			else if (key == "Accessed")
			{
				std::string accessedTime = g_variant_get_string(value, NULL);
				currentElement.setAccessedTime(decodeTimeString(accessedTime));
			}
			else if (key == "Created")
			{
				std::string createdTime = g_variant_get_string(value, NULL);
				currentElement.setCreatedTime(decodeTimeString(createdTime));
			}

			g_variant_unref(value);
			g_variant_unref(valueVar);
			g_variant_unref(keyVar);
			g_variant_unref(propertyVar);
		}

		elements.push_back(currentElement);

		g_variant_unref(entry);
	}

	return elements;
}

void Bluez5ProfileFtp::listFolder(const std::string &address, const std::string &path, BluetoothFtpListFolderResultCallback callback)
{
	Bluez5ObexSession *session = findSession(address);
	if (!session)
	{
		callback(BLUETOOTH_ERROR_PARAM_INVALID, std::vector<BluetoothFtpElement>());
		return;
	}

	BluezObexFileTransfer1 *fileTransferProxy = session->getFileTransferProxy();

	auto changeFolderCallback = [this, fileTransferProxy, callback](GAsyncResult *result) {
		GError *error = 0;
		gboolean ret;

		ret = bluez_obex_file_transfer1_call_change_folder_finish(fileTransferProxy, result, &error);
		if (error)
		{
			g_error_free(error);
			callback(BLUETOOTH_ERROR_FAIL, std::vector<BluetoothFtpElement>());
			return;
		}

		auto listFolderCallback = [this, fileTransferProxy, callback](GAsyncResult *result) {
			GError *error = 0;
			gboolean ret;
			GVariant *entries = 0;

			ret = bluez_obex_file_transfer1_call_list_folder_finish(fileTransferProxy, &entries, result, &error);
			if (error)
			{
				g_error_free(error);
				callback(BLUETOOTH_ERROR_FAIL, std::vector<BluetoothFtpElement>());
				return;
			}

			auto elements = buildElementList(entries);
			callback(BLUETOOTH_ERROR_NONE, elements);
		};

		bluez_obex_file_transfer1_call_list_folder(fileTransferProxy, NULL, glibAsyncMethodWrapper,
												  new GlibAsyncFunctionWrapper(listFolderCallback));
	};

	// First we have to change to the folder we want to list, then we can
	// ask for it's content
	bluez_obex_file_transfer1_call_change_folder(fileTransferProxy, path.c_str(),
												NULL, glibAsyncMethodWrapper,
												new GlibAsyncFunctionWrapper(changeFolderCallback));
}

void Bluez5ProfileFtp::updateActiveTransfer(BluetoothFtpTransferId id, Bluez5ObexTransfer *transfer, BluetoothFtpTransferResultCallback callback)
{
	bool cleanup = false;

	if (transfer->getState() == Bluez5ObexTransfer::State::ACTIVE)
	{
		callback(BLUETOOTH_ERROR_NONE, transfer->getBytesTransferred(), false);
	}
	else if (transfer->getState() == Bluez5ObexTransfer::State::COMPLETE)
	{
		callback(BLUETOOTH_ERROR_NONE, transfer->getBytesTransferred(), true);
		cleanup = true;
	}
	else if (transfer->getState() == Bluez5ObexTransfer::State::ERROR)
	{
		DEBUG("File transfer failed");
		callback(BLUETOOTH_ERROR_FAIL, transfer->getBytesTransferred(), false);
		cleanup = true;
	}

	if (cleanup)
		removeTransfer(id);
}

void Bluez5ProfileFtp::startTransfer(BluetoothFtpTransferId id, const std::string &objectPath, BluetoothFtpTransferResultCallback callback)
{
	// NOTE: ownership of the transfer object is passed to updateActiveTransfer which
	// will delete it once there is nothing to left to do with it
	Bluez5ObexTransfer *transfer = new Bluez5ObexTransfer(std::string(objectPath));
	mTransfers.insert(std::pair<BluetoothFtpTransferId, Bluez5ObexTransfer*>(id, transfer));
	transfer->watch(std::bind(&Bluez5ProfileFtp::updateActiveTransfer, this, id, transfer, callback));
}

BluetoothFtpTransferId Bluez5ProfileFtp::pullFile(const std::string &address, const std::string &sourcePath, const std::string &targetPath, BluetoothFtpTransferResultCallback callback)
{
	Bluez5ObexSession *session = findSession(address);
	if (!session || sourcePath.length() == 0 || targetPath.length() == 0)
	{
		callback(BLUETOOTH_ERROR_PARAM_INVALID, 0, false);
		return BLUETOOTH_FTP_TRANSFER_ID_INVALID;
	}

	BluezObexFileTransfer1 *fileTransferProxy = session->getFileTransferProxy();
	if (!fileTransferProxy)
	{
		callback(BLUETOOTH_ERROR_FAIL, 0, false);
		return BLUETOOTH_FTP_TRANSFER_ID_INVALID;
	}

	BluetoothFtpTransferId transferId = nextTransferId();
        gchar *dirName = g_path_get_dirname(sourcePath.c_str());
	gchar *fileName = g_path_get_basename(sourcePath.c_str());

	std::string basePath(dirName);
	std::string sourceFileName(fileName);

	g_free(dirName);
	g_free(fileName);

	auto changeFolderCallback = [this, fileTransferProxy, sourceFileName, targetPath, transferId, callback](GAsyncResult *result) {
		GError *error = 0;
		gboolean ret;

		ret = bluez_obex_file_transfer1_call_change_folder_finish(fileTransferProxy, result, &error);
		if (error)
		{
			g_error_free(error);
			callback(BLUETOOTH_ERROR_FAIL, 0, false);
			return;
		}

		auto getFileCallback = [this, fileTransferProxy, transferId, callback](GAsyncResult *result) {
			GError *error = 0;
			gboolean ret;
			gchar *objectPath = 0;

			ret = bluez_obex_file_transfer1_call_get_file_finish(fileTransferProxy, &objectPath, NULL, result, &error);
			if (error)
			{
				g_error_free(error);
				callback(BLUETOOTH_ERROR_FAIL, 0, false);
				return;
			}

			startTransfer(transferId, std::string(objectPath), callback);
		};

		bluez_obex_file_transfer1_call_get_file(fileTransferProxy, targetPath.c_str(), sourceFileName.c_str(),
											   NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(getFileCallback));
	};

	// First we have to change to the folder we want to get the file from and
	// then we can ask to retrieve the file we want
	bluez_obex_file_transfer1_call_change_folder(fileTransferProxy, basePath.c_str(),
												NULL, glibAsyncMethodWrapper,
												new GlibAsyncFunctionWrapper(changeFolderCallback));

	return transferId;
}

BluetoothFtpTransferId Bluez5ProfileFtp::pushFile(const std::string &address, const std::string &sourcePath, const std::string &targetPath, BluetoothFtpTransferResultCallback callback)
{
	Bluez5ObexSession *session = findSession(address);
	if (!session || sourcePath.length() == 0 || targetPath.length() == 0)
	{
		callback(BLUETOOTH_ERROR_PARAM_INVALID, 0, false);
		return BLUETOOTH_FTP_TRANSFER_ID_INVALID;
	}

	BluezObexFileTransfer1 *fileTransferProxy = session->getFileTransferProxy();
	if (!fileTransferProxy)
	{
		callback(BLUETOOTH_ERROR_FAIL, 0, false);
		return BLUETOOTH_FTP_TRANSFER_ID_INVALID;
	}

	BluetoothFtpTransferId transferId = nextTransferId();

	gchar *dirName = g_path_get_dirname(targetPath.c_str());
	gchar *fileName = g_path_get_basename(targetPath.c_str());

	std::string basePath(dirName);
	std::string targetFileName(fileName);

	g_free(dirName);
	g_free(fileName);

	auto changeFolderCallback = [this, fileTransferProxy, sourcePath, targetFileName, transferId, callback](GAsyncResult *result) {
		GError *error = 0;
		gboolean ret;

		ret = bluez_obex_file_transfer1_call_change_folder_finish(fileTransferProxy, result, &error);
		if (error)
		{
			g_error_free(error);
			callback(BLUETOOTH_ERROR_FAIL, 0, false);
			return;
		}

		auto putFileCallback = [this, transferId, fileTransferProxy, callback](GAsyncResult *result) {
			GError *error = 0;
			gboolean ret;
			gchar *objectPath = 0;

			ret = bluez_obex_file_transfer1_call_put_file_finish(fileTransferProxy, &objectPath, NULL, result, &error);
			if (error)
			{
				g_error_free(error);
				callback(BLUETOOTH_ERROR_FAIL, 0, false);
				return;
			}

			startTransfer(transferId, std::string(objectPath), callback);;
		};

		bluez_obex_file_transfer1_call_put_file(fileTransferProxy, sourcePath.c_str(), targetFileName.c_str(),
											   NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(putFileCallback));
	};

	// First we have to change to the folder we want to put the file into and
	// then we can ask to send the file
	bluez_obex_file_transfer1_call_change_folder(fileTransferProxy, basePath.c_str(),
												NULL, glibAsyncMethodWrapper,
												new GlibAsyncFunctionWrapper(changeFolderCallback));

	return transferId;
}

void Bluez5ProfileFtp::cancelTransfer(BluetoothFtpTransferId id, BluetoothResultCallback callback)
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
