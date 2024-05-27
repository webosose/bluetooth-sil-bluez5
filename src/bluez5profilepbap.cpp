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
#include "utils.h"

#define VERSION "1.1"
#define MAX_FILTER_BIT 32

const std::string BLUETOOTH_PROFILE_PBAP_UUID = "00001130-0000-1000-8000-00805f9b34fb";
static std::vector<std::string> supportedObjects = {"pb", "ich", "mch", "och", "cch"};
static std::vector<std::string> supportedRepositories = {"sim1", "internal"};
static std::vector<std::string> supportedSearchKey = {"name", "number", "sound"};
static std::vector<std::string> supportedSearchOrder = {"indexed", "alphanumeric", "phonetic"};
static std::map<std::string, std::string> supportedVCardVersions = {{"2.1", "vcard21"}, {"3.0", "vcard30"}};
static std::vector<std::string> stateString = {"idle", "idle", "active", "active","completed","error"};

Bluez5ProfilePbap::Bluez5ProfilePbap(Bluez5Adapter *adapter) :
    Bluez5ObexProfileBase(Bluez5ObexSession::Type::PBAP, adapter, BLUETOOTH_PROFILE_PBAP_UUID),
    mAdapter(adapter),
    mObjectPhonebookProxy(nullptr),
    mPropertiesProxy(nullptr)
{
    INFO(MSGID_PBAP_PROFILE_ERROR, 0, "Supported PBAP Version:%s",VERSION);
}

Bluez5ProfilePbap::~Bluez5ProfilePbap()
{
    if (mPropertiesProxy)
        g_object_unref(mPropertiesProxy);
    if (mObjectPhonebookProxy)
        g_object_unref(mObjectPhonebookProxy);

    mTransferStateMap.clear();
    clearTransferMap();
}

bool Bluez5ProfilePbap::isSearchKeyValid( const std::string &searchKey)
{
    if (std::find(supportedSearchKey.begin(), supportedSearchKey.end(), searchKey) != supportedSearchKey.end())
        return true;
    else
        return false;
}

bool Bluez5ProfilePbap::isSearchOrderValid( const std::string &searchOrder)
{
    if (std::find(supportedSearchOrder.begin(), supportedSearchOrder.end(), searchOrder) != supportedSearchOrder.end())
        return true;
    else
        return false;
}

void Bluez5ProfilePbap::clearTransferMap()
{
    if(mTransfersMap.size()>0)
    {
        auto it = mTransfersMap.begin();
        while (it != mTransfersMap.end())
        {
            if(it->second)
                delete it->second;
            it = mTransfersMap.erase(it);
        }
    }
    mTransfersMap.clear();
}

bool Bluez5ProfilePbap::isObjectValid( const std::string &object)
{
    if (std::find(supportedObjects.begin(), supportedObjects.end(), object) != supportedObjects.end())
        return true;
    else
        return false;
}

bool Bluez5ProfilePbap::isVCardVersionValid( const std::string &vCardVersion)
{
    auto vCardVersionIter = supportedVCardVersions.find(vCardVersion);
    if (vCardVersionIter != supportedVCardVersions.end())
        return true;
    else
        return false;
}

bool Bluez5ProfilePbap::isRepositoryValid( const std::string &repository)
{
    if (std::find(supportedRepositories.begin(), supportedRepositories.end(), repository) != supportedRepositories.end())
        return true;
    else
        return false;
}

std::string Bluez5ProfilePbap::convertToSupportedVcardVersion(const std::string &vCardVersion)
{
    auto vCardVersionIter = supportedVCardVersions.find(vCardVersion);
    if (vCardVersionIter != supportedVCardVersions.end())
        return vCardVersionIter->second;
    else
        return "NA";
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

    mObjectPhonebookProxy = session->getObjectPhoneBookProxy();

    if (!mObjectPhonebookProxy)
    {
        DEBUG("objectPhonebookProxy failed");
        callback(BLUETOOTH_ERROR_NOT_ALLOWED);
        return;
    }

    auto setPhoneBookCallback = [this, callback](GAsyncResult *result) {

    GError *error = 0;
    gboolean ret;
    ret = bluez_obex_phonebook_access1_call_select_finish(this->mObjectPhonebookProxy, result, &error);
    if (error)
    {
        ERROR(MSGID_PBAP_PROFILE_ERROR, 0, "Failed to call phonebook access select error:%s",error->message);
        if (strstr(error->message, "Invalid path"))
        {
            callback(BLUETOOTH_ERROR_PARAM_INVALID);
        }
        else if (strstr(error->message, "Not Found"))
        {
            callback(BLUETOOTH_ERROR_UNSUPPORTED);
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

    bluez_obex_phonebook_access1_call_select(mObjectPhonebookProxy, repository.c_str(), object.c_str(), NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(setPhoneBookCallback));

}

void Bluez5ProfilePbap::getvCardFilters(const std::string &address, BluetoothPbapListFiltersResultCallback callback)
{

    const Bluez5ObexSession *session = findSession(address);
    std::list<std::string> emptyFilterList;
    if (!session)
    {
        callback(BLUETOOTH_ERROR_PARAM_INVALID,emptyFilterList);
        return;
    }

    BluezObexPhonebookAccess1 *objectPhonebookProxy = session->getObjectPhoneBookProxy();
    if (!objectPhonebookProxy)
    {
        callback(BLUETOOTH_ERROR_FAIL,emptyFilterList);
        return;
    }

    auto getFilterListCallback = [this, objectPhonebookProxy, callback](GAsyncResult *result) {

        gchar **out_fields = NULL;
        GError *error = 0;
        gboolean ret;
        std::list<std::string> filterList;
        ret = bluez_obex_phonebook_access1_call_list_filter_fields_finish(objectPhonebookProxy, &out_fields, result, &error);
        if (error)
        {
            g_error_free(error);
            callback(BLUETOOTH_ERROR_FAIL,filterList);
            return;
        }

        for (int filterBit = 0; *out_fields != NULL && filterBit < MAX_FILTER_BIT; filterBit++ )
        {
            filterList.push_back(*out_fields++);
        }
        callback(BLUETOOTH_ERROR_NONE,filterList);
    };

    bluez_obex_phonebook_access1_call_list_filter_fields(objectPhonebookProxy, NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(getFilterListCallback));

}

void Bluez5ProfilePbap::getPhonebookSize(const std::string &address, BluetoothPbapGetSizeResultCallback callback)
{
    const Bluez5ObexSession *session = findSession(address);

    if (!session)
    {
        DEBUG("phonebook session failed");
        callback(BLUETOOTH_ERROR_NOT_ALLOWED,0);
        return;
    }

    mObjectPhonebookProxy = session->getObjectPhoneBookProxy();

    if (!mObjectPhonebookProxy)
    {
        DEBUG("objectPhonebookProxy failed");
        callback(BLUETOOTH_ERROR_NOT_ALLOWED, 0);
        return;
    }
    auto getSizeCallback = [this, callback](GAsyncResult *result) {

    uint16_t size = 0;
    GError *error = 0;
    gboolean ret;

    ret = bluez_obex_phonebook_access1_call_get_size_finish(this->mObjectPhonebookProxy, &size, result, &error);
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

    bluez_obex_phonebook_access1_call_get_size(mObjectPhonebookProxy, NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(getSizeCallback));
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

    if (!mObjectPhonebookProxy)
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


    auto vCardListingCallback = [this, callback](GAsyncResult *result) {
    GError *error = 0;
    gboolean ret;
    GVariant *out_vcard_listing = 0;
    BluetoothPbapVCardList vCardList;

    ret = bluez_obex_phonebook_access1_call_list_finish(this->mObjectPhonebookProxy, &out_vcard_listing, result, &error);
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

    bluez_obex_phonebook_access1_call_list(mObjectPhonebookProxy, arguments, NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(vCardListingCallback));
    return;
}

void Bluez5ProfilePbap::getPhoneBookProperties(const std::string &address, BluetoothPbapApplicationParameterCallback callback)
{
    const Bluez5ObexSession *session = findSession(address);

    initializePbapApplicationParameters();

    if (!session)
    {
        DEBUG("phonebook session failed");
        callback(BLUETOOTH_ERROR_NOT_ALLOWED, pbapApplicationParameters);
        return;
    }

    mPropertiesProxy = session->getObjectPropertiesProxy();
    mDeviceAddress = session->getDeviceAddress();

    if (!mPropertiesProxy)
    {
        DEBUG("getObjectPropertiesProxy failed");
        callback(BLUETOOTH_ERROR_NOT_ALLOWED, pbapApplicationParameters);
        return;
    }

    auto propertiesGetCallback = [this, callback](GAsyncResult *result) {
        GVariant *propsVar;
        GError *error = 0;
        gboolean ret;

        ret = free_desktop_dbus_properties_call_get_all_finish(this->mPropertiesProxy, &propsVar, result, &error);
        if (error)
        {
            g_error_free(error);
            DEBUG("free_desktop_dbus_properties_call_get_all_finish failed");
            callback(BLUETOOTH_ERROR_FAIL, pbapApplicationParameters);
            return;
        }

        pbapApplicationParameters.setFolder("No folder is selected");
        parseAllProperties(propsVar);
        callback(BLUETOOTH_ERROR_NONE, pbapApplicationParameters);
    };

    free_desktop_dbus_properties_call_get_all(mPropertiesProxy,"org.bluez.obex.PhonebookAccess1", NULL,
                        glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(propertiesGetCallback));
}

void Bluez5ProfilePbap::updateProperties(GVariant *changedProperties)
{
    bool changed = false;

    for (int n = 0; n < g_variant_n_children(changedProperties); n++)
    {
        GVariant *propertyVar = g_variant_get_child_value(changedProperties, n);
        GVariant *keyVar = g_variant_get_child_value(propertyVar, 0);
        std::string key = g_variant_get_string(keyVar, NULL);
        if (key == "Folder")
        {
            changed = true;
        }
        g_variant_unref(keyVar);
        g_variant_unref(propertyVar);
    }

    if (changed)
    {
        updateVersion();
        notifyUpdatedProperties();
    }

}

void Bluez5ProfilePbap::addPropertyFromVariant(const std::string &key, GVariant *valueVar)
{
    if (key == "PrimaryCounter")
    {
        pbapApplicationParameters.setPrimaryCounter(g_variant_get_string(valueVar, NULL));
    }
    else if (key == "SecondaryCounter")
    {
        pbapApplicationParameters.setSecondaryCounter(g_variant_get_string(valueVar, NULL));
    }
    else if (key == "DatabaseIdentifier")
    {
        pbapApplicationParameters.setDataBaseIdentifier(g_variant_get_string(valueVar, NULL));
    }
    else if (key == "FixedImageSize")
    {
        pbapApplicationParameters.setFixedImageSize(g_variant_get_boolean(valueVar));
    }
    else  if (key == "Folder")
    {
        pbapApplicationParameters.setFolder(g_variant_get_string(valueVar, NULL));
    }
}

void Bluez5ProfilePbap::notifyUpdatedProperties()
{

    auto propertiesGetCallback = [this](GAsyncResult *result) {
        GVariant *propsVar;
        GError *error = 0;
        gboolean ret;

        ret = free_desktop_dbus_properties_call_get_all_finish(this->mPropertiesProxy, &propsVar, result, &error);
        if (error)
        {
            g_error_free(error);
            DEBUG("free_desktop_dbus_properties_call_get_all_finish failed");
            return;
        }

        parseAllProperties(propsVar);

        getPbapObserver()->profilePropertiesChanged(convertAddressToLowerCase(mAdapter->getAddress()), mDeviceAddress, pbapApplicationParameters);
    };

    free_desktop_dbus_properties_call_get_all(mPropertiesProxy,"org.bluez.obex.PhonebookAccess1", NULL,
                        glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(propertiesGetCallback));

}

void Bluez5ProfilePbap::initializePbapApplicationParameters()
{
    pbapApplicationParameters.setFolder("NULL");
    pbapApplicationParameters.setPrimaryCounter("NULL");
    pbapApplicationParameters.setSecondaryCounter("NULL");
    pbapApplicationParameters.setDataBaseIdentifier("NULL");
    pbapApplicationParameters.setFixedImageSize(false);
}

void Bluez5ProfilePbap::parseAllProperties(GVariant *propsVar)
{

    for (int n = 0; n < g_variant_n_children(propsVar); n++)
    {
        GVariant *propertyVar = g_variant_get_child_value(propsVar, n);
        GVariant *keyVar = g_variant_get_child_value(propertyVar, 0);
        GVariant *valueVar = g_variant_get_child_value(propertyVar, 1);

        std::string key = g_variant_get_string(keyVar, NULL);
        GVariant *propertyData = g_variant_get_variant(valueVar);
        addPropertyFromVariant(key, propertyData);

        g_variant_unref(propertyData);
        g_variant_unref(valueVar);
        g_variant_unref(keyVar);
        g_variant_unref(propertyVar);
    }
}

void Bluez5ProfilePbap::updateVersion()
{

    GError *error = 0;
    gboolean ret;
    ret = bluez_obex_phonebook_access1_call_update_version_sync(mObjectPhonebookProxy, NULL, &error);
    if (error)
    {
        ERROR(MSGID_PBAP_PROFILE_ERROR, 0, "Failed to call phonebook access update version error:%s",error->message);
        g_error_free(error);
    }
}
void Bluez5ProfilePbap::supplyAccessConfirmation(BluetoothPbapAccessRequestId accessRequestId, bool accept, BluetoothResultCallback callback)
{

}

GVariant * Bluez5ProfilePbap::setFilters(const std::string &vCardVersion, BluetoothPbapVCardFilterList &vCardFilters)
{
    GVariant *vCardVersionValue = g_variant_new_string(vCardVersion.c_str());
    GVariantBuilder *builderVcardFiltersValue = g_variant_builder_new (G_VARIANT_TYPE ("as"));
    GVariantBuilder *builder = 0;
    GVariant *filters = 0;
    std::string format="Format";
    std::string fields="Fields";

    if(vCardFilters.size())
    {
        for(int i = 0; i < vCardFilters.size(); i++)
        {
            g_variant_builder_add(builderVcardFiltersValue, "s", vCardFilters[i].c_str());
        }
    }
    else
    {
         g_variant_builder_add(builderVcardFiltersValue, "s", "ALL");
    }

    GVariant *vCardFiltersValue = g_variant_builder_end(builderVcardFiltersValue);

    builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
    g_variant_builder_add (builder, "{sv}",format.c_str() , vCardVersionValue);
    g_variant_builder_add (builder, "{sv}",fields.c_str() , vCardFiltersValue);
    filters = g_variant_builder_end(builder);
    g_variant_builder_unref(builder);
    return filters;
}

GVariant * Bluez5ProfilePbap::setSearchFilters(const std::string &searchOrder)
{

    GVariant *searchOrderValue = g_variant_new_string(searchOrder.c_str());
    GVariantBuilder *builder = 0;
    GVariant *filters = 0;
    std::string order="Order";
    builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
    g_variant_builder_add (builder, "{sv}",order.c_str() , searchOrderValue);
    filters = g_variant_builder_end(builder);
    g_variant_builder_unref(builder);
    return filters;
}

GVariant * Bluez5ProfilePbap::setPhoneBookFilters(const std::string &vCardVersion, BluetoothPbapVCardFilterList &vCardFilters , uint16_t startOffset , uint16_t maxCount)
{
    GVariant *vCardVersionValue = g_variant_new_string(vCardVersion.c_str());
    GVariantBuilder *builderVcardFiltersValue = g_variant_builder_new (G_VARIANT_TYPE ("as"));
    GVariant *startIndexValue = g_variant_new_uint16(startOffset);
    GVariant *maxCountValue = g_variant_new_uint16(maxCount);
    GVariantBuilder *builder = 0;
    GVariant *filters = 0;
    std::string format="Format";
    std::string fields="Fields";

    if(vCardFilters.size())
    {
        for(int i = 0; i < vCardFilters.size(); i++)
        {
            g_variant_builder_add(builderVcardFiltersValue, "s", vCardFilters[i].c_str());
        }
    }
    else
    {
         g_variant_builder_add(builderVcardFiltersValue, "s", "ALL");
    }

    GVariant *vCardFiltersValue = g_variant_builder_end(builderVcardFiltersValue);

    builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
    g_variant_builder_add (builder, "{sv}",format.c_str() , vCardVersionValue);
    g_variant_builder_add (builder, "{sv}","Offset" , startIndexValue);
    g_variant_builder_add (builder, "{sv}","MaxCount" , maxCountValue);
    g_variant_builder_add (builder, "{sv}",fields.c_str() , vCardFiltersValue);
    filters = g_variant_builder_end(builder);
    g_variant_builder_unref(builder);
    return filters;
}

void Bluez5ProfilePbap::searchPhoneBook (const std::string &address, const std::string &searchOrder, const std::string &searchKey, const std::string &searchValue, BluetoothPbapVCardListResultCallback callback)
{
    BluetoothPbapVCardList emptySearchList;
    if (searchOrder.empty() || searchKey.empty() || searchValue.empty())
    {
        callback(BLUETOOTH_ERROR_PARAM_INVALID, emptySearchList);
        return;
    }

    if (!isSearchKeyValid(searchKey))
    {
        callback(BLUETOOTH_ERROR_PARAM_INVALID, emptySearchList);
        return;
    }

    if (!isSearchOrderValid(searchOrder))
    {
        callback(BLUETOOTH_ERROR_PARAM_INVALID, emptySearchList);
        return;
    }

    const Bluez5ObexSession *session = findSession(address);

    if (!session)
    {
        callback(BLUETOOTH_ERROR_NOT_ALLOWED,emptySearchList);
        return;
    }

    mObjectPhonebookProxy = session->getObjectPhoneBookProxy();

    if (!mObjectPhonebookProxy)
    {
        DEBUG("objectPhonebookProxy failed");
        callback(BLUETOOTH_ERROR_NOT_ALLOWED,emptySearchList);
        return;
    }

    auto searchCallback = [this, callback](GAsyncResult *result) {
        GError *error = 0;
        gboolean ret;
        GVariant *outSearchlisting = 0;
        BluetoothPbapVCardList searchList;

        ret = bluez_obex_phonebook_access1_call_search_finish(this->mObjectPhonebookProxy, &outSearchlisting, result, &error);
        if (error)
        {
            ERROR(MSGID_PBAP_PROFILE_ERROR, 0, "Failed to call phonebook access list error:%s",error->message);
            if (strstr(error->message, "Call Select first of all"))
            {
                callback(BLUETOOTH_ERROR_PBAP_CALL_SELECT_FOLDER_TYPE, searchList);
            }
            else
            {
                callback(BLUETOOTH_ERROR_FAIL, searchList);
            }
            g_error_free(error);
            return;
        }

        GVariantIter *iter;
        gchar *str1, *str2;

        g_variant_get (outSearchlisting, "a(ss)", &iter);
        while (g_variant_iter_loop (iter, "(ss)", &str1, &str2))
        {
            searchList.insert(std::pair<std::string, std::string>(std::string(str1),std::string(str2)));
        }
        g_variant_iter_free (iter);
        callback(BLUETOOTH_ERROR_NONE, searchList);
    };

    bluez_obex_phonebook_access1_call_search(mObjectPhonebookProxy, searchKey.c_str(), searchValue.c_str(), setSearchFilters(searchOrder), NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(searchCallback));
    return;
}

void Bluez5ProfilePbap::pullvCard(const std::string &address, const std::string &targetFile, const std::string &vCardHandle, const std::string &vCardVersion, BluetoothPbapVCardFilterList &vCardFilters, BluetoothResultCallback callback)
{
    if (targetFile.empty() || vCardHandle.empty())
    {
        callback(BLUETOOTH_ERROR_PARAM_INVALID);
        return;
    }

    if (!isVCardVersionValid(vCardVersion))
    {
        callback(BLUETOOTH_ERROR_PARAM_INVALID);
        return;
    }

    const std::string supportedVcardVersion = convertToSupportedVcardVersion(vCardVersion);

    const Bluez5ObexSession *session = findSession(address);
    if (!session)
    {
        DEBUG("phonebook session failed");
        callback(BLUETOOTH_ERROR_NOT_ALLOWED);
        return;
    }

    mObjectPhonebookProxy = session->getObjectPhoneBookProxy();

    if (!mObjectPhonebookProxy)
    {
        DEBUG("objectPhonebookProxy failed");
        callback(BLUETOOTH_ERROR_NOT_ALLOWED);
        return;
    }

    auto pullCallback = [this, callback](GAsyncResult *result) {
        GError *error = 0;
        gboolean ret;
        gchar *objectPath = 0;

        ret = bluez_obex_phonebook_access1_call_pull_finish(this->mObjectPhonebookProxy, &objectPath, NULL, result, &error);
        if (error)
        {
            ERROR(MSGID_PBAP_PROFILE_ERROR, 0, "Failed to call phonebook access list error:%s",error->message);
            if (strstr(error->message, "Call Select first of all"))
            {
                callback(BLUETOOTH_ERROR_PBAP_CALL_SELECT_FOLDER_TYPE);
            }
            else
            {
                callback(BLUETOOTH_ERROR_FAIL);
            }
            g_error_free(error);
            return;
        }

        startTransfer(std::string(objectPath), callback, Bluez5ObexTransfer::TransferType::RECEIVING);
    };

    bluez_obex_phonebook_access1_call_pull(mObjectPhonebookProxy, vCardHandle.c_str(),  targetFile.c_str(),setFilters(supportedVcardVersion, vCardFilters),
                                        NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(pullCallback));
}


void Bluez5ProfilePbap::pullPhoneBook(const std::string &address,const std::string &destinationFile,const std::string &vCardVersion,BluetoothPbapVCardFilterList &vCardFilters, const uint16_t &startIndex, const uint16_t &maxCount, BluetoothGetPhoneBookResultCallback callback)
{
    if (destinationFile.empty())
    {
        callback(BLUETOOTH_ERROR_PARAM_INVALID, "");
        return;
    }

    if (!isVCardVersionValid(vCardVersion))
    {
        callback(BLUETOOTH_ERROR_PARAM_INVALID, "");
        return;
    }

    const std::string supportedVcardVersion = convertToSupportedVcardVersion(vCardVersion);

    const Bluez5ObexSession *session = findSession(address);
    if (!session)
    {
        DEBUG("phonebook session failed");
        callback(BLUETOOTH_ERROR_NOT_ALLOWED, "");
        return;
    }

    mObjectPhonebookProxy = session->getObjectPhoneBookProxy();

    if (!mObjectPhonebookProxy)
    {
        DEBUG("objectPhonebookProxy failed");
        callback(BLUETOOTH_ERROR_NOT_ALLOWED, "");
        return;
    }

    auto getPhoneBookCallback = [this,address,callback](GAsyncResult *result) {
        GError *error = 0;
        gboolean ret;
        gchar *objectPath = 0;

        ret = bluez_obex_phonebook_access1_call_pull_all_finish(this->mObjectPhonebookProxy, &objectPath, NULL, result, &error);
        if (error)
        {
            ERROR(MSGID_PBAP_PROFILE_ERROR, 0, "Failed to call phonebook access list error:%s",error->message);
            if (strstr(error->message, "Call Select first of all"))
            {
                callback(BLUETOOTH_ERROR_PBAP_CALL_SELECT_FOLDER_TYPE, "");
            }
            else
            {
                callback(BLUETOOTH_ERROR_FAIL, "");
            }
            g_error_free(error);
            return;
        }

        callback(BLUETOOTH_ERROR_NONE, std::string(objectPath));
        startPhoneBookTransfer(std::string(objectPath),address,Bluez5ObexTransfer::TransferType::RECEIVING);
    };

    bluez_obex_phonebook_access1_call_pull_all(mObjectPhonebookProxy,destinationFile.c_str(),setPhoneBookFilters(supportedVcardVersion, vCardFilters, startIndex, maxCount),
                                        NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(getPhoneBookCallback));
}

void Bluez5ProfilePbap::startPhoneBookTransfer(const std::string &objectPath, const std::string &address ,Bluez5ObexTransfer::TransferType type)
{
    // NOTE: ownership of the transfer object is passed to updateActiveTransfer which
    // will delete it once there is nothing to left to do with it
    Bluez5ObexTransfer *transfer = new Bluez5ObexTransfer(std::string(objectPath), type);
    mTransfersMap.insert({std::string(objectPath), transfer});
    transfer->watch(std::bind(&Bluez5ProfilePbap::monitorTransfer, this, objectPath,address,transfer));
}

void Bluez5ProfilePbap::startTransfer(const std::string &objectPath, BluetoothResultCallback callback, Bluez5ObexTransfer::TransferType type)
{
    // NOTE: ownership of the transfer object is passed to updateActiveTransfer which
    // will delete it once there is nothing to left to do with it
    Bluez5ObexTransfer *transfer = new Bluez5ObexTransfer(std::string(objectPath), type);
    mTransfersMap.insert({std::string(objectPath), transfer});
    transfer->watch(std::bind(&Bluez5ProfilePbap::updateActiveTransfer, this, objectPath, transfer, callback));
}

void Bluez5ProfilePbap::removeTransfer(const std::string &objectPath)
{
    auto transferIter = mTransfersMap.find(objectPath);
    if (transferIter == mTransfersMap.end())
        return;

    Bluez5ObexTransfer *transfer = transferIter->second;
    mTransfersMap.erase(transferIter);
    delete transfer;
}

void Bluez5ProfilePbap::monitorTransfer(const std::string &objectPath, const std::string &address, Bluez5ObexTransfer *transfer)
{
    std::string state = stateString[transfer->getState()];
    auto itr = mTransferStateMap.find(std::string(objectPath));
    if(itr != mTransferStateMap.end())
    {
        if(state != itr->second)
        {
            if ((Bluez5ObexTransfer::State::COMPLETE == transfer->getState()) && (itr->second == stateString[Bluez5ObexTransfer::State::QUEUED])) 
                getPbapObserver()->transferStatusChanged(convertAddressToLowerCase(mAdapter->getAddress()), address ,transfer->getFilePath(),std::string(objectPath),stateString[Bluez5ObexTransfer::State::ACTIVE]);

            itr->second = state;

            getPbapObserver()->transferStatusChanged(convertAddressToLowerCase(mAdapter->getAddress()), address ,transfer->getFilePath(),std::string(objectPath),state);
        }
    }
    else
    {
        mTransferStateMap.insert({std::string(objectPath),state});
        getPbapObserver()->transferStatusChanged(convertAddressToLowerCase(mAdapter->getAddress()), address ,transfer->getFilePath(),std::string(objectPath),state);
    }

    if ((Bluez5ObexTransfer::State::COMPLETE == transfer->getState())||
         (Bluez5ObexTransfer::State::ERROR == transfer->getState()))
    {
        removeTransfer(objectPath);
        mTransferStateMap.erase(itr);
    }
}

void Bluez5ProfilePbap::updateActiveTransfer(const std::string &objectPath, Bluez5ObexTransfer *transfer, BluetoothResultCallback callback)
{
    bool cleanup = false;

    if (transfer->getState() == Bluez5ObexTransfer::State::COMPLETE)
    {
        callback(BLUETOOTH_ERROR_NONE);
        cleanup = true;
    }
    else if (transfer->getState() == Bluez5ObexTransfer::State::ERROR)
    {
        DEBUG("File transfer failed");
        callback(BLUETOOTH_ERROR_FAIL);
        cleanup = true;
    }

    if (cleanup)
        removeTransfer(objectPath);
}
