// Copyright (c) 2020 LG Electronics, Inc.
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

#include "bluez5profilepbap.h"
#include "bluez5adapter.h"
#include "logging.h"
#include "asyncutils.h"

#define VERSION "1.1"

const std::string BLUETOOTH_PROFILE_PBAP_UUID = "00001130-0000-1000-8000-00805f9b34fb";
static std::vector<std::string> supportedObjects = {"pb", "ich", "mch", "och", "cch"};
static std::vector<std::string> supportedRepositories = {"sim1", "internal"};

Bluez5ProfilePbap::Bluez5ProfilePbap(Bluez5Adapter *adapter) :
	Bluez5ObexProfileBase(Bluez5ObexSession::Type::PBAP, adapter, BLUETOOTH_PROFILE_PBAP_UUID)
{
    INFO(MSGID_PBAP_PROFILE_ERROR, 0, "Supported PBAP Version:%s",VERSION);
}

Bluez5ProfilePbap::~Bluez5ProfilePbap()
{
}

bool Bluez5ProfilePbap::isObjectValid( const std::string object)
{
    if (std::find(supportedObjects.begin(), supportedObjects.end(), object) != supportedObjects.end())
        return true;
    else
        return false;
}

bool Bluez5ProfilePbap::isRepositoryValid( const std::string repository)
{
    if (std::find(supportedRepositories.begin(), supportedRepositories.end(), repository) != supportedRepositories.end())
        return true;
    else
        return false;
}

void Bluez5ProfilePbap::setPhoneBook(const std::string &address, const std::string &repository, const std::string &object, BluetoothResultCallback callback)
{
    if (repository.empty() || object.empty())
    {
        callback(BLUETOOTH_ERROR_PARAM_INVALID);
        return;
    }

    if (!isObjectValid(object) || !isRepositoryValid(repository))
    {
        callback(BLUETOOTH_ERROR_PARAM_INVALID);
        return;
    }
    const Bluez5ObexSession *session = findSession(address);
    if (!session)
    {
        DEBUG("phonebook session failed");
        callback(BLUETOOTH_ERROR_NOT_ALLOWED);
        return;
    }

    BluezObexPhonebookAccess1 *objectPhonebookProxy = session->getObjectPhoneBookProxy();
    if (!objectPhonebookProxy)
    {
        DEBUG("objectPhonebookProxy failed");
        callback(BLUETOOTH_ERROR_FAIL);
        return;
    }

    auto setPhoneBookCallback = [this, objectPhonebookProxy, callback](GAsyncResult *result) {

    GError *error = 0;
    gboolean ret;
    ret = bluez_obex_phonebook_access1_call_select_finish(objectPhonebookProxy, result, &error);
    if (error)
    {
        ERROR(MSGID_PBAP_PROFILE_ERROR, 0, "Failed to call phonebook access select error:%s",error->message);
        if (strstr(error->message, "Invalid path"))
        {
            callback(BLUETOOTH_ERROR_PARAM_INVALID);
        }
        else
        {
            callback(BLUETOOTH_ERROR_FAIL);
        }
        g_error_free(error);
        return;
    }

    callback(BLUETOOTH_ERROR_NONE);
    };

    bluez_obex_phonebook_access1_call_select(objectPhonebookProxy, repository.c_str(), object.c_str(), NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(setPhoneBookCallback));

}

void Bluez5ProfilePbap::supplyAccessConfirmation(BluetoothPbapAccessRequestId accessRequestId, bool accept, BluetoothResultCallback callback)
{

}
