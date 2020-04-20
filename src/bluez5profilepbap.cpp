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
    Bluez5ObexProfileBase(Bluez5ObexSession::Type::PBAP, adapter, BLUETOOTH_PROFILE_PBAP_UUID),
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

    updateVersion();

    };

    bluez_obex_phonebook_access1_call_select(mObjectPhonebookProxy, repository.c_str(), object.c_str(), NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(setPhoneBookCallback));

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

void Bluez5ProfilePbap::getPhoneBookProperties(const std::string &address, BluetoothPropertiesResultCallback callback)
{
    const Bluez5ObexSession *session = findSession(address);

    if (!session)
    {
        DEBUG("phonebook session failed");
        callback(BLUETOOTH_ERROR_NOT_ALLOWED, BluetoothPropertiesList());
        return;
    }

    mPropertiesProxy = session->getObjectPropertiesProxy();
    mDeviceAddress = session->getDeviceAddress();

    if (!mPropertiesProxy)
    {
        DEBUG("getObjectPropertiesProxy failed");
        callback(BLUETOOTH_ERROR_NOT_ALLOWED, BluetoothPropertiesList());
        return;
    }

    setErrorProperties();

    if(mHandleId <= 0)
    {
        mHandleId = g_signal_connect(G_OBJECT(mPropertiesProxy), "properties-changed", G_CALLBACK(handlePropertiesChanged),this);
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
            callback(BLUETOOTH_ERROR_FAIL, BluetoothPropertiesList());
            return;
        }
        BluetoothPropertiesList properties;
        parseAllProperties(properties, propsVar);
        callback(BLUETOOTH_ERROR_NONE, properties);
    };

    free_desktop_dbus_properties_call_get_all(mPropertiesProxy,"org.bluez.obex.PhonebookAccess1", NULL,
                        glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(propertiesGetCallback));
}

void Bluez5ProfilePbap::handlePropertiesChanged(Bluez5ProfilePbap *, gchar *interface,  GVariant *changedProperties, GVariant *invalidatedProperties, gpointer userData)
{

    auto pbap = static_cast<Bluez5ProfilePbap*>(userData);
    bool changed = false;
    BluetoothPropertiesList properties;
    DEBUG("properties-changed signal triggered for interface: %s", interface);

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
        pbap->notifyUpdatedProperties();
}

void Bluez5ProfilePbap::addPropertyFromVariant(BluetoothPropertiesList& properties, const std::string &key, GVariant *valueVar)
{
    if (key == "PrimaryCounter")
    {
        mPrimaryCounter = g_variant_get_string(valueVar, NULL);
    }
    else if (key == "SecondaryCounter")
    {
        mSecondaryCounter = g_variant_get_string(valueVar, NULL);
    }
    else if (key == "DatabaseIdentifier")
    {
        mDatabaseIdentifier = g_variant_get_string(valueVar, NULL);
    }
    else if (key == "FixedImageSize")
    {
        mFixedImageSize = g_variant_get_boolean(valueVar);
    }
    else  if (key == "Folder")
    {
        mFolder = g_variant_get_string(valueVar, NULL);
    }

    properties.push_back(BluetoothProperty(BluetoothProperty::Type::FOLDER, mFolder));
    properties.push_back(BluetoothProperty(BluetoothProperty::Type::FIXED_IMAGE_SIZE, mFixedImageSize));
    properties.push_back(BluetoothProperty(BluetoothProperty::Type::DATABASE_IDENTIFIER, mDatabaseIdentifier));
    properties.push_back(BluetoothProperty(BluetoothProperty::Type::SECONDERY_COUNTER, mSecondaryCounter));
    properties.push_back(BluetoothProperty(BluetoothProperty::Type::PRIMARY_COUNTER, mPrimaryCounter));
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
        BluetoothPropertiesList properties;
        parseAllProperties(properties, propsVar);
        getPbapObserver()->profilePropertiesChanged(properties, mDeviceAddress);
    };

    free_desktop_dbus_properties_call_get_all(mPropertiesProxy,"org.bluez.obex.PhonebookAccess1", NULL,
                        glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(propertiesGetCallback));

}
void Bluez5ProfilePbap::setErrorProperties()
{
    mFolder = "No folder is selected";
    mPrimaryCounter = "NULL";
    mSecondaryCounter = "NULL";
    mDatabaseIdentifier = "NULL";
    mFixedImageSize = false;
}

void Bluez5ProfilePbap::parseAllProperties(BluetoothPropertiesList& properties, GVariant *propsVar)
{

    for (int n = 0; n < g_variant_n_children(propsVar); n++)
    {
        GVariant *propertyVar = g_variant_get_child_value(propsVar, n);
        GVariant *keyVar = g_variant_get_child_value(propertyVar, 0);
        GVariant *valueVar = g_variant_get_child_value(propertyVar, 1);

        std::string key = g_variant_get_string(keyVar, NULL);
        GVariant *propertyData = g_variant_get_variant(valueVar);
        addPropertyFromVariant(properties, key, propertyData);

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