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

#include "bluez5profilemap.h"
#include "bluez5adapter.h"
#include "logging.h"
#include "bluez5obexclient.h"
#include "asyncutils.h"
#include "utils.h"
#include "bluez5busconfig.h"

const std::string BLUETOOTH_PROFILE_MAS_UUID = "00001132-0000-1000-8000-00805f9b34fb";
using namespace std::placeholders;

std::unordered_map<std::string,BluetoothMapProperty::Type> propertyMap = {
    {"Folder",BluetoothMapProperty::Type::FOLDER},
    {"Subject",BluetoothMapProperty::Type::SUBJECT},
    {"Timestamp",BluetoothMapProperty::Type::TIMESTAMP},
    {"Sender",BluetoothMapProperty::Type::SENDER},
    {"SenderAddress",BluetoothMapProperty::Type::SENDERADDRESS},
    {"ReplyTo",BluetoothMapProperty::Type::REPLYTO},
    {"Recipient",BluetoothMapProperty::Type::RECIPIENT},
    {"RecipientAddress",BluetoothMapProperty::Type::RECIPIENTADDRESS},
    {"Type",BluetoothMapProperty::Type::MESSAGETYPES},
    {"Status",BluetoothMapProperty::Type::STATUS},
    {"Size",BluetoothMapProperty::Type::SIZE},
    {"AttachmentSize",BluetoothMapProperty::Type::ATTACHMENTSIZE},
    {"Priority",BluetoothMapProperty::Type::PRIORITY},
    {"Read",BluetoothMapProperty::Type::READ},
    {"Sent",BluetoothMapProperty::Type::SENT},
    {"Protected",BluetoothMapProperty::Type::PROTECTED},
    {"Text",BluetoothMapProperty::Type::TEXTTYPE},
    {"ObjectPath",BluetoothMapProperty::Type::OBJECTPATH},
    {"EventType",BluetoothMapProperty::Type::EVENTTYPE},
    {"OldFolder",BluetoothMapProperty::Type::OLDFOLDER},
    };

Bluez5ProfileMap::Bluez5ProfileMap(Bluez5Adapter *adapter) :
    Bluez5ObexProfileBase(Bluez5ObexSession::Type::MAP, adapter, BLUETOOTH_PROFILE_MAS_UUID),
    mAdapter(adapter)
{

}

Bluez5ProfileMap::~Bluez5ProfileMap()
{
}

void Bluez5ProfileMap::connect(const std::string &address, const std::string &instanceName, BluetoothMapCallback callback)
{
	DEBUG("Connecting with device %s on instanceName %s profile", address.c_str(), instanceName.c_str());
	createSession(address, instanceName, callback);
}

void Bluez5ProfileMap::disconnect(const std::string &sessionKey, const std::string &sessionId, BluetoothMapCallback callback)
{
	DEBUG("Disconnecting with sessionKey %s  ", sessionKey.c_str());
	std::string instanceName;
	if(!sessionKey.empty())
	{
		std::size_t pos = sessionKey.find("_");

		if(pos != std::string::npos)
			instanceName = sessionKey.substr(pos+1);
		DEBUG("Disconnecting with instanceName %s  ", instanceName.c_str());
		removeFromSessionList(sessionKey);
		callback(BLUETOOTH_ERROR_NONE,instanceName);
	}
	else
	{
		callback(BLUETOOTH_ERROR_FAIL,instanceName);
	}

}

void Bluez5ProfileMap::createSession(const std::string &address, const std::string &instanceName, BluetoothMapCallback callback)
{
	Bluez5ObexClient *obexClient = mAdapter->getObexClient();

	if (!obexClient)
	{
		callback(BLUETOOTH_ERROR_FAIL,"");
		return;
	}
	std::string sessionkey = address+"_"+instanceName;

	auto sessionCreateCallback = [this, sessionkey, callback](Bluez5ObexSession *session) {
		if (!session)
		{
			callback(BLUETOOTH_ERROR_FAIL,"");
			return;
		}

		session->watch(std::bind(&Bluez5ObexProfileBase::handleObexSessionStatus, this, sessionkey, _1));
		DEBUG("createSession g_signal_connect");
		g_signal_connect(G_OBJECT(session->getObjectPropertiesProxy()), "properties-changed", G_CALLBACK(handlePropertiesChanged), this);
		std::string sessionId = getSessionIdFromSessionPath(session->getObjectPath());
		storeSession(sessionkey, session);
		callback(BLUETOOTH_ERROR_NONE,sessionId);
	};

	std::string upperCaseAddress = convertAddressToUpperCase(address);
	obexClient->createSession(Bluez5ObexSession::Type::MAP, upperCaseAddress, sessionCreateCallback, instanceName);
}

void Bluez5ProfileMap::notifySessionStatus(const std::string &sessionKey, bool createdOrRemoved)
{
	BluetoothPropertiesList properties;
	properties.push_back(BluetoothProperty(BluetoothProperty::Type::CONNECTED, createdOrRemoved));
	std::string convertedAddress = convertSessionKey(sessionKey);
	DEBUG("notifySessionStatus convertedAddress %s ", convertedAddress.c_str());
	getObserver()->propertiesChanged(convertAddressToLowerCase(mAdapter->getAddress()), convertedAddress, properties);
}

std::string Bluez5ProfileMap::convertSessionKey(const std::string &sessionKey)
{
	std::size_t keyPos = sessionKey.find("_");
	std::string address = ( keyPos == std::string::npos ? "" : sessionKey.substr(0, keyPos));
	return convertAddressToLowerCase(address) + sessionKey.substr(keyPos);
}

std::string Bluez5ProfileMap::getSessionIdFromSessionPath(const std::string &sessionPath)
{
	std::size_t idPos = sessionPath.find_last_of("/");
	return ( idPos == std::string::npos ? "" : sessionPath.substr(idPos + 1));
}

void Bluez5ProfileMap::getMessageFilters(const std::string &sessionKey, const std::string &sessionId, BluetoothMapListFiltersResultCallback callback)
{

    const Bluez5ObexSession *session = findSession(sessionKey);
    std::list<std::string> emptyFilterList;
    if (!session)
    {
        callback(BLUETOOTH_ERROR_PARAM_INVALID,emptyFilterList);
        return;
    }

    BluezObexMessageAccess1 *objectMessageProxy = session->getObjectMessageProxy();
    if (!objectMessageProxy)
    {
        callback(BLUETOOTH_ERROR_FAIL,emptyFilterList);
        return;
    }

    auto getFilterListCallback = [ = ](GAsyncResult *result) {

        gchar **out_fields = NULL;
        GError *error = 0;
        gboolean ret;
        std::list<std::string> filterList;
        ret = bluez_obex_message_access1_call_list_filter_fields_finish(objectMessageProxy, &out_fields, result, &error);
        if (error)
        {
            g_error_free(error);
            callback(BLUETOOTH_ERROR_FAIL,filterList);
            return;
        }

        for (int filterBit = 0; *out_fields != NULL /*&& filterBit < MAX_FILTER_BIT*/; filterBit++ )
        {
            filterList.push_back(*out_fields++);
        }
        callback(BLUETOOTH_ERROR_NONE,filterList);
    };

    bluez_obex_message_access1_call_list_filter_fields(objectMessageProxy, NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(getFilterListCallback));

 }

void Bluez5ProfileMap::getFolderList(const std::string &sessionKey, const std::string &sessionId, const uint16_t &startOffset, const uint16_t &maxCount, BluetoothMapGetFoldersCallback callback)
{
    DEBUG("%s", __FUNCTION__);
    std::vector<std::string> folderList;
    const Bluez5ObexSession *session = findSession(sessionKey);
    if (!session)
    {
        callback(BLUETOOTH_ERROR_PARAM_INVALID,folderList);
        return;
    }

    BluezObexMessageAccess1 *tObjectMapProxy = session->getObjectMessageProxy();
    if (!tObjectMapProxy)
    {
        callback(BLUETOOTH_ERROR_FAIL,folderList);
        return;
    }
    bluez_obex_message_access1_call_list_folders(tObjectMapProxy ,buildGetFolderListParam(startOffset,maxCount),
                                                 NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(std::bind(&Bluez5ProfileMap::getFolderListCb,
                                                 this,tObjectMapProxy,callback,_1)));
}

void Bluez5ProfileMap::getFolderListCb(BluezObexMessageAccess1* tObjectMapProxy, BluetoothMapGetFoldersCallback callback, GAsyncResult *result)
{
    DEBUG("%s", __FUNCTION__);
    std::vector<std::string> folderList;
    GError *error = 0;
    GVariant *outFolderList = 0;
    bluez_obex_message_access1_call_list_folders_finish(tObjectMapProxy, &outFolderList, result, &error);
    if (error)
    {
        callback(BLUETOOTH_ERROR_FAIL,folderList);
        g_error_free(error);
        return;
    }
    parseGetFolderListResponse(outFolderList,folderList);
    callback(BLUETOOTH_ERROR_NONE,folderList);
}

void Bluez5ProfileMap::parseGetFolderListResponse(GVariant *outFolderList,std::vector<std::string> &folders)
{
    DEBUG("%s", __FUNCTION__);
    g_autoptr(GVariantIter) iter = NULL;
    g_autoptr(GVariantIter) iter1 = NULL;
    g_variant_get (outFolderList, "aa{sv}", &iter);
    while (g_variant_iter_loop (iter, "a{sv}", &iter1))
    {
        gchar *keyVar;
        GVariant *valueVar;
        while (g_variant_iter_loop (iter1, "{sv}", &keyVar, &valueVar))
            folders.push_back(g_variant_get_string(valueVar, NULL));
    }
}

GVariant * Bluez5ProfileMap::buildGetFolderListParam(const uint16_t &startOffset, const uint16_t &maxCount)
{
    DEBUG("%s", __FUNCTION__);
    GVariant *startIndexValue = g_variant_new_uint16(startOffset);
    GVariant *maxCountValue = g_variant_new_uint16(maxCount);

    GVariantBuilder *builder = 0;
    GVariant *params = 0;
    builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
    g_variant_builder_add (builder, "{sv}","Offset" , startIndexValue);
    g_variant_builder_add (builder, "{sv}","MaxCount" , maxCountValue);
    params = g_variant_builder_end(builder);
    g_variant_builder_unref(builder);
    return params;
}

void Bluez5ProfileMap::setFolder(const std::string &sessionKey, const std::string &sessionId, const std::string &folder, BluetoothResultCallback callback)
{
    DEBUG("%s", __FUNCTION__);
    const Bluez5ObexSession *session = findSession(sessionKey);
    if (!session)
    {
        callback(BLUETOOTH_ERROR_PARAM_INVALID);
        return;
    }

    BluezObexMessageAccess1 *tObjectMapProxy = session->getObjectMessageProxy();
    if (!tObjectMapProxy)
    {
        callback(BLUETOOTH_ERROR_FAIL);
        return;
    }

    auto setFolderCallback = [this, tObjectMapProxy, callback](GAsyncResult *result) {

    GError *error = 0;
    gboolean ret;
    ret = bluez_obex_message_access1_call_set_folder_finish(tObjectMapProxy, result, &error);
    if (error)
    {
        ERROR(MSGID_MAP_PROFILE_ERROR, 0, "Failed to set folder error:%s",error->message);
        if (strstr(error->message, "Not Found"))
        {
            callback(BLUETOOTH_ERROR_MAP_FOLDER_NOT_FOUND);
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
    bluez_obex_message_access1_call_set_folder(tObjectMapProxy , folder.c_str(),
                                                 NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(setFolderCallback));
}

void Bluez5ProfileMap::getMessageList(const std::string &sessionKey, const std::string &sessionId, const std::string &folder, const BluetoothMapPropertiesList &filters, BluetoothMapGetMessageListCallback callback)
{
    DEBUG("%s", __FUNCTION__);
    BluetoothMessageList messageList;
    const Bluez5ObexSession *session = findSession(sessionKey);
    if (!session)
    {
        callback(BLUETOOTH_ERROR_PARAM_INVALID,messageList);
        return;
    }

    BluezObexMessageAccess1 *tObjectMapProxy = session->getObjectMessageProxy();
    if (!tObjectMapProxy)
    {
        callback(BLUETOOTH_ERROR_FAIL,messageList);
        return;
    }

    bluez_obex_message_access1_call_list_messages(tObjectMapProxy ,folder.c_str(),buildGetMessageListParam(filters),
                                                 NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(std::bind(&Bluez5ProfileMap::getMessageListCb,
                                                 this,tObjectMapProxy,callback,_1)));
}

void Bluez5ProfileMap::getMessageListCb(BluezObexMessageAccess1* tObjectMapProxy, BluetoothMapGetMessageListCallback callback, GAsyncResult *result)
{
    DEBUG("%s", __FUNCTION__);
    BluetoothMessageList messageList;
    GError *error = nullptr;
    GVariant *outMessageList = nullptr;
    bluez_obex_message_access1_call_list_messages_finish(tObjectMapProxy, &outMessageList, result, &error);
    if (error)
    {
        ERROR(MSGID_MAP_PROFILE_ERROR, 0, "Failed to get message list due to :%s",error->message);
        callback(BLUETOOTH_ERROR_FAIL,messageList);
        g_error_free(error);
        return;
    }
    parseGetMessageListResponse(outMessageList,messageList);
    callback(BLUETOOTH_ERROR_NONE,messageList);
}

GVariant * Bluez5ProfileMap::buildGetMessageListParam(const BluetoothMapPropertiesList &filters)
{
    DEBUG("%s", __FUNCTION__);
    GVariantBuilder *builder = 0;
    GVariant *params = 0;
    builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
    for (auto filter:filters)
    {
        switch (filter.getType())
        {
            case BluetoothMapProperty::Type::STARTOFFSET:
                g_variant_builder_add (builder, "{sv}","Offset" ,g_variant_new_uint16(filter.getValue<uint16_t>()));
                break;
            case BluetoothMapProperty::Type::MAXCOUNT:
                g_variant_builder_add (builder, "{sv}","MaxCount" ,g_variant_new_uint16(filter.getValue<uint16_t>()));
                break;
            case BluetoothMapProperty::Type::SUBJECTLENGTH:
                g_variant_builder_add (builder, "{sv}","SubjectLength" ,g_variant_new_byte(filter.getValue<uint8_t>()));
                break;
            case BluetoothMapProperty::Type::PERIODBEGIN:
                g_variant_builder_add (builder, "{sv}","PeriodBegin" ,g_variant_new_string(filter.getValue<std::string>().c_str()));
                break;
            case BluetoothMapProperty::Type::PERIODEND:
                g_variant_builder_add (builder, "{sv}","PeriodEnd" ,g_variant_new_string(filter.getValue<std::string>().c_str()));
                break;
            case BluetoothMapProperty::Type::RECIPIENT:
                g_variant_builder_add (builder, "{sv}","Recipient" ,g_variant_new_string(filter.getValue<std::string>().c_str()));
                break;
            case BluetoothMapProperty::Type::SENDER:
                g_variant_builder_add (builder, "{sv}","Sender" ,g_variant_new_string(filter.getValue<std::string>().c_str()));
                break;
            case BluetoothMapProperty::Type::PRIORITY:
                g_variant_builder_add (builder, "{sv}","Priority" , g_variant_new_boolean(filter.getValue<bool>()));
                break;
            case BluetoothMapProperty::Type::READ:
                g_variant_builder_add (builder, "{sv}","Read" , g_variant_new_boolean(filter.getValue<bool>()));
                break;
            case BluetoothMapProperty::Type::MESSAGETYPES:
                {
                    GVariant *messageFilters = nullptr;
                    GVariantBuilder *builderMessageTypes = g_variant_builder_new (G_VARIANT_TYPE ("as"));
                    std::vector<std::string> messageTypes = filter.getValue<std::vector<std::string>>();
                    for(auto messageType : messageTypes)
                        g_variant_builder_add(builderMessageTypes, "s", messageType.c_str());
                    messageFilters = g_variant_builder_end(builderMessageTypes);
                    g_variant_builder_add (builder, "{sv}","Types" , messageFilters);
                    g_variant_builder_unref(builderMessageTypes);
                    break;
                }
            case BluetoothMapProperty::Type::FIELDS:
                {
                    GVariant *fieldFilters = nullptr;
                    GVariantBuilder *builderFields = g_variant_builder_new (G_VARIANT_TYPE ("as"));
                    std::vector<std::string> fields = filter.getValue<std::vector<std::string>>();
                    for(auto field : fields)
                        g_variant_builder_add(builderFields, "s", field.c_str());
                    fieldFilters = g_variant_builder_end(builderFields);
                    g_variant_builder_add (builder, "{sv}","Fields" , builderFields);
                    g_variant_builder_unref(builderFields);
                    break;
                }
            default:
                break;
        }
    }
    params = g_variant_builder_end(builder);
    g_variant_builder_unref(builder);
    return params;
}

void Bluez5ProfileMap::parseGetMessageListResponse(GVariant *outMessageList,BluetoothMessageList &messageList)
{
    DEBUG("%s", __FUNCTION__);

    g_autoptr(GVariantIter) iter1 = NULL;
    g_variant_get (outMessageList, "a{oa{sv}}", &iter1);

    const gchar *objectPath;
    g_autoptr(GVariantIter) iter2 = NULL;
    while (g_variant_iter_loop (iter1, "{oa{sv}}", &objectPath, &iter2))
    {
        const gchar *key;
        g_autoptr(GVariant) value = NULL;
        BluetoothMapPropertiesList messageProperties;
        while (g_variant_iter_loop (iter2, "{sv}", &key, &value))
        {
            std::string keyValue(key);
            addMessageProperties(keyValue,value,messageProperties);
        }
        std::string tObjectPath(objectPath);
        std::size_t found = tObjectPath.find("message");
        if (found != std::string::npos)
        {
            std::string messageHandle = tObjectPath.substr(found);
            messageList.push_back(std::make_pair(messageHandle,messageProperties));
        }
    }
}

void Bluez5ProfileMap::addMessageProperties(std::string& key , GVariant* value , BluetoothMapPropertiesList &messageProperties)
{
    auto it = propertyMap.find(key);
    if(it != propertyMap.end())
    {
        switch(it->second)
        {
            case BluetoothMapProperty::Type::FOLDER:
            case BluetoothMapProperty::Type::SUBJECT:
            case BluetoothMapProperty::Type::TIMESTAMP:
            case BluetoothMapProperty::Type::SENDER:
            case BluetoothMapProperty::Type::SENDERADDRESS:
            case BluetoothMapProperty::Type::REPLYTO:
            case BluetoothMapProperty::Type::RECIPIENT:
            case BluetoothMapProperty::Type::RECIPIENTADDRESS:
            case BluetoothMapProperty::Type::MESSAGETYPES:
            case BluetoothMapProperty::Type::STATUS:
            case BluetoothMapProperty::Type::EVENTTYPE:
            case BluetoothMapProperty::Type::OLDFOLDER:
                {
                    std::string parsedStringVal = g_variant_get_string(value, NULL);
                    messageProperties.push_back(BluetoothMapProperty(it->second,parsedStringVal));
                    break;
                }
            case BluetoothMapProperty::Type::SIZE:
            case BluetoothMapProperty::Type::ATTACHMENTSIZE:
                {
                    uint64_t parsedUint64Val = g_variant_get_uint64(value);
                    messageProperties.push_back(BluetoothMapProperty(it->second,parsedUint64Val));
                    break;
                }
            case BluetoothMapProperty::Type::TEXTTYPE:
            case BluetoothMapProperty::Type::PRIORITY:
            case BluetoothMapProperty::Type::READ:
            case BluetoothMapProperty::Type::SENT:
            case BluetoothMapProperty::Type::PROTECTED:
                {
                    bool parsedBoolVal = g_variant_get_boolean(value);
                    messageProperties.push_back(BluetoothMapProperty(it->second,parsedBoolVal));
                    break;
                }
            default:
                break;
        }
    }
}

void Bluez5ProfileMap::getMessage(const std::string &sessionKey, const std::string &messageHandle, bool attachment, const std::string &destinationFile, BluetoothResultCallback callback)
{
    DEBUG("%s", __FUNCTION__);
    const Bluez5ObexSession *session = findSession(sessionKey);
    if (!session)
    {
        callback(BLUETOOTH_ERROR_PARAM_INVALID);
        return;
    }

    std::string objectPath = session->getObjectPath() + "/" + messageHandle;

    BluezObexMessage1 *objectMessageProxy = createMessageHandleProxyUsingPath(objectPath);

    if (!objectMessageProxy)
    {
        callback(BLUETOOTH_ERROR_MAP_MESSAGE_HANDLE_NOT_FOUND);
        return;
    }

    auto getMessageCallback = [this, objectMessageProxy, callback](GAsyncResult *result) {

        GError *error = 0;
        gboolean ret;
        gchar *objectPath = 0;

        ret = bluez_obex_message1_call_get_finish(objectMessageProxy, &objectPath, NULL, result, &error);

        if(objectMessageProxy)
            g_object_unref(objectMessageProxy);

        if (error)
        {
            ERROR(MSGID_MAP_PROFILE_ERROR, 0, "Failed to get message error:%s",error->message);
            if (strstr(error->message, "UnknownObject"))
            {
                callback(BLUETOOTH_ERROR_MAP_MESSAGE_HANDLE_NOT_FOUND);
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
    bluez_obex_message1_call_get(objectMessageProxy , destinationFile.c_str(), attachment,
                                                 NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(getMessageCallback));

}

BluezObexMessage1* Bluez5ProfileMap::createMessageHandleProxyUsingPath(const std::string &objectPath)
{
    GError *error = 0;
    BluezObexMessage1 *objectMessageProxy = NULL;
    objectMessageProxy = bluez_obex_message1_proxy_new_for_bus_sync(BLUEZ5_OBEX_DBUS_BUS_TYPE, G_DBUS_PROXY_FLAGS_NONE,
                                                                        "org.bluez.obex", objectPath.c_str(), NULL, &error);
    if (error)
    {
        ERROR(MSGID_FAILED_TO_CREATE_OBEX_MESSAGE_PROXY, 0,
            "Failed to create dbus proxy for obex message on path %s",
            objectPath.c_str());
        g_error_free(error);
    }
    return objectMessageProxy;
}

void Bluez5ProfileMap::startTransfer(const std::string &objectPath, BluetoothResultCallback callback, Bluez5ObexTransfer::TransferType type)
{
      // NOTE: ownership of the transfer object is passed to updateActiveTransfer which
      // will delete it once there is nothing to left to do with it
      Bluez5ObexTransfer *transfer = new Bluez5ObexTransfer(std::string(objectPath), type);
      mTransfersMap.insert({std::string(objectPath), transfer});
      transfer->watch(std::bind(&Bluez5ProfileMap::updateActiveTransfer, this, objectPath, transfer, callback));
}

void Bluez5ProfileMap::startPushTransfer(const std::string &objectPath, BluetoothMapCallback callback, Bluez5ObexTransfer::TransferType type)
{
      // NOTE: ownership of the transfer object is passed to updateActiveTransfer which
      // will delete it once there is nothing to left to do with it
      Bluez5ObexTransfer *transfer = new Bluez5ObexTransfer(std::string(objectPath), type);
      mTransfersMap.insert({std::string(objectPath), transfer});
      transfer->watch(std::bind(&Bluez5ProfileMap::updatePushTransfer, this, objectPath, transfer, callback));
}

void Bluez5ProfileMap::removeTransfer(const std::string &objectPath)
{
    auto transferIter = mTransfersMap.find(objectPath);
    if (transferIter == mTransfersMap.end())
        return;

    Bluez5ObexTransfer *transfer = transferIter->second;
    mTransfersMap.erase(transferIter);
    delete transfer;
}

void Bluez5ProfileMap::updateActiveTransfer(const std::string &objectPath, Bluez5ObexTransfer *transfer, BluetoothResultCallback callback)
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

void Bluez5ProfileMap::updatePushTransfer(const std::string &objectPath, Bluez5ObexTransfer *transfer, BluetoothMapCallback callback)
{
    bool cleanup = false;

    if (transfer->getState() == Bluez5ObexTransfer::State::COMPLETE)
    {
        callback(BLUETOOTH_ERROR_NONE,transfer->getMessageHandle());
        cleanup = true;
    }
    else if (transfer->getState() == Bluez5ObexTransfer::State::ERROR)
    {
        DEBUG("File transfer failed");
        callback(BLUETOOTH_ERROR_FAIL,transfer->getMessageHandle());
        cleanup = true;
    }

    if (cleanup)
        removeTransfer(objectPath);
}

void Bluez5ProfileMap::setMessageStatus(const std::string &sessionKey, const std::string &messageHandle, const std::string &statusIndicator, bool statusValue, BluetoothResultCallback callback)
{
    DEBUG("%s", __FUNCTION__);
    const Bluez5ObexSession *session = findSession(sessionKey);
    if (!session)
    {
        callback(BLUETOOTH_ERROR_PARAM_INVALID);
        return;
    }

    std::string objectPath = session->getObjectPath() + "/" + messageHandle;

    BluezObexMessage1 *objectMessageProxy = createMessageHandleProxyUsingPath(objectPath);

    if (!objectMessageProxy)
    {
        callback(BLUETOOTH_ERROR_MAP_MESSAGE_HANDLE_NOT_FOUND);
        return;
    }

    if (statusIndicator == "read")
    {
        bluez_obex_message1_set_read(objectMessageProxy, statusValue);
        callback(BLUETOOTH_ERROR_NONE);
    }
    else if (statusIndicator == "delete")
    {
        bluez_obex_message1_set_deleted(objectMessageProxy, statusValue);
        callback(BLUETOOTH_ERROR_NONE);
    }
    else
    {
        callback(BLUETOOTH_ERROR_PARAM_INVALID);
    }

    if(objectMessageProxy)
        g_object_unref(objectMessageProxy);

}

void Bluez5ProfileMap::pushMessage(const std::string &sessionKey, const std::string &sourceFile, const std::string &folder, const std::string &charset, bool transparent, bool retry, BluetoothMapCallback callback)
{

    const Bluez5ObexSession *session = findSession(sessionKey);

    if (!session)
    {
        callback(BLUETOOTH_ERROR_PARAM_INVALID, "");
        return;
    }

    BluezObexMessageAccess1 *objectMessageProxy = session->getObjectMessageProxy();
    if (!objectMessageProxy)
    {
        callback(BLUETOOTH_ERROR_FAIL, "");
        return;
    }

   auto pushMessageCallback = [=](GAsyncResult *result) {

        GError *error = 0;
        gboolean ret;
        gchar *objectPath = 0;
        GVariant *outProperties;

        ret = bluez_obex_message_access1_call_push_message_finish(objectMessageProxy, &objectPath, &outProperties, result, &error);
        if (error)
        {
            ERROR(MSGID_MAP_PROFILE_ERROR, 0, "Failed to get message error:%s",error->message);
            callback(BLUETOOTH_ERROR_FAIL,"");
            g_error_free(error);
            return;
        }

		startPushTransfer(std::string(objectPath), callback, Bluez5ObexTransfer::TransferType::SENDING);
    };

    bluez_obex_message_access1_call_push_message(objectMessageProxy , sourceFile.c_str(),  folder.c_str(), buildMessageArgs(charset, transparent, retry),
                                                 NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(pushMessageCallback));
}

GVariant* Bluez5ProfileMap::buildMessageArgs(const std::string &charset, bool transparent, bool retry)
{
    DEBUG("%s", __FUNCTION__);
    GVariantBuilder *builder = 0;
    GVariant *params = 0;
    builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
    g_variant_builder_add (builder, "{sv}","Transparent" , g_variant_new_boolean(transparent));
    g_variant_builder_add (builder, "{sv}","Retry" , g_variant_new_boolean(retry));
    g_variant_builder_add (builder, "{sv}","Charset" ,g_variant_new_string(charset.c_str()));

    params = g_variant_builder_end(builder);
    g_variant_builder_unref(builder);
    return params;
}

void Bluez5ProfileMap::updateProperties(GVariant *changedProperties)
{
    bool notificationEvent = false;
    BluetoothMessageList messageList;
    std::string sessionId;

    for (int n = 0; n < g_variant_n_children(changedProperties); n++)
    {
        GVariant *propertyVar = g_variant_get_child_value(changedProperties, n);
        GVariant *keyVar = g_variant_get_child_value(propertyVar, 0);
        GVariant *valueVariant = g_variant_get_child_value(propertyVar, 1);
        GVariant *valueVar = g_variant_get_variant(valueVariant);
        std::string key = g_variant_get_string(keyVar, NULL);
        if (key == "Notification")
        {
            g_autoptr(GVariantIter) iter1 = NULL;
            g_autoptr(GVariant) value = NULL;
            g_variant_get (valueVar, "a{sv}", &iter1);

            const gchar *key;
            BluetoothMapPropertiesList properties;
            std::string ObjectPath;

            while (g_variant_iter_loop (iter1, "{sv}", &key, &value))
            {
                std::string keyValue(key);
                addMessageProperties(keyValue,value,properties);
                if (keyValue == "ObjectPath")
                {
                    notificationEvent = true;
                    ObjectPath = g_variant_get_string(value, NULL);
                }

            }
            std::size_t messageFound = ObjectPath.find("message");
            if (messageFound != std::string::npos)
            {
                std::string messageHandle = ObjectPath.substr(messageFound);
                messageList.push_back(std::make_pair(messageHandle,properties));
            }

            std::string session("session");
            std::size_t sessionFound = ObjectPath.find(session);
            if (sessionFound != std::string::npos)
            {
                sessionId = ObjectPath.substr(sessionFound, session.length()+1);
            }
        }
        g_variant_unref(keyVar);
        g_variant_unref(propertyVar);
        g_variant_unref(valueVariant);
        g_variant_unref(valueVar);
    }
    if(notificationEvent)
        getMapObserver()->messageNotificationEvent(convertAddressToLowerCase(mAdapter->getAddress()), sessionId, messageList);
}
