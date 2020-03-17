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

void Bluez5ProfilePbap::getPhonebookSize(const std::string &address, BluetoothPbapGetSizeResultCallback callback)
{
    const Bluez5ObexSession *session = findSession(address);

    if (!session)
    {
        DEBUG("phonebook session failed");
        callback(BLUETOOTH_ERROR_PARAM_INVALID,0);
        return;
    }

    BluezObexPhonebookAccess1 *objectPhonebookProxy = session->getObjectPhoneBookProxy();
    if (!objectPhonebookProxy)
    {
        DEBUG("objectPhonebookProxy failed");
        callback(BLUETOOTH_ERROR_FAIL,0);
        return;
    }

    auto getSizeCallback = [this, objectPhonebookProxy, callback](GAsyncResult *result) {

    uint16_t size = 0;
    GError *error = 0;
    gboolean ret;

    ret = bluez_obex_phonebook_access1_call_get_size_finish(objectPhonebookProxy, &size, result, &error);
    if (error)
    {
        ERROR(MSGID_PBAP_PROFILE_ERROR, 0, "Failed to call phonebook access get size error:%s",error->message);
        if (strstr(error->message, "Call Select first of all"))
        {
            callback(BLUETOOTH_ERROR_PBAP_CALL_SELECT_FOLDER_TYPE, 0);
        }
        else
        {
            callback(BLUETOOTH_ERROR_FAIL, 0);
        }
        g_error_free(error);
        return;
    }

    callback(BLUETOOTH_ERROR_NONE,size);
    };

    bluez_obex_phonebook_access1_call_get_size(objectPhonebookProxy, NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(getSizeCallback));
}

void Bluez5ProfilePbap::vCardListing(const std::string &address, BluetoothPbapVCardListResultCallback callback)
{
    BluetoothPbapVCardList emptyVCardList;
    const Bluez5ObexSession *session = findSession(address);
    if (!session )
    {
        callback(BLUETOOTH_ERROR_NOT_ALLOWED, emptyVCardList);
        return;
    }
    BluezObexPhonebookAccess1 *objectPhonebookProxy = session->getObjectPhoneBookProxy();
    if (!objectPhonebookProxy)
    {
        callback(BLUETOOTH_ERROR_NOT_ALLOWED, emptyVCardList);
        return;
    }
    GVariantBuilder *dataBuilder;
    dataBuilder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
    g_variant_builder_add(dataBuilder, "s", "Offset");
    g_variant_builder_add(dataBuilder, "s", "MaxCount");
    GVariant *dataValue = g_variant_builder_end(dataBuilder);
    g_variant_builder_unref(dataBuilder);

    GVariantBuilder *builder = 0;
    GVariant *arguments = 0;
    std::string temp="filters";
    builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
    g_variant_builder_add (builder, "{sv}",temp.c_str() , dataValue);
    arguments = g_variant_builder_end(builder);
    g_variant_builder_unref(builder);


    auto vCardListingCallback = [this, objectPhonebookProxy, callback](GAsyncResult *result) {
    GError *error = 0;
    gboolean ret;
    GVariant *out_vcard_listing = 0;
    BluetoothPbapVCardList vCardList;

    ret = bluez_obex_phonebook_access1_call_list_finish(objectPhonebookProxy, &out_vcard_listing, result, &error);
    if (error)
    {
        ERROR(MSGID_PBAP_PROFILE_ERROR, 0, "Failed to call phonebook access list error:%s",error->message);
        if (strstr(error->message, "Call Select first of all"))
        {
            callback(BLUETOOTH_ERROR_PBAP_CALL_SELECT_FOLDER_TYPE, vCardList);
        }
        else
        {
            callback(BLUETOOTH_ERROR_FAIL, vCardList);
        }
        g_error_free(error);
        return;
    }

    GVariantIter *iter;
    gchar *str1, *str2;

    g_variant_get (out_vcard_listing, "a(ss)", &iter);
    while (g_variant_iter_loop (iter, "(ss)", &str1, &str2))
    {
        vCardList.insert(std::pair<std::string, std::string>(std::string(str1),std::string(str2)));
    }
    g_variant_iter_free (iter);
    callback(BLUETOOTH_ERROR_NONE, vCardList);
    };

    bluez_obex_phonebook_access1_call_list(objectPhonebookProxy, arguments, NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(vCardListingCallback));
    return;
}

void Bluez5ProfilePbap::supplyAccessConfirmation(BluetoothPbapAccessRequestId accessRequestId, bool accept, BluetoothResultCallback callback)
{

}
