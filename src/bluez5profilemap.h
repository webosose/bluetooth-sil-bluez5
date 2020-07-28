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

#ifndef BLUEZ5PROFILEMAP_H
#define BLUEZ5PROFILEMAP_H

#include <string>
#include <map>

#include <bluetooth-sil-api.h>
#include <bluez5obexprofilebase.h>
#include "bluez5profilebase.h"

class Bluez5Adapter;

class Bluez5ProfileMap : public Bluez5ObexProfileBase,
                        public BluetoothMapProfile
{
public:
    Bluez5ProfileMap(Bluez5Adapter *adapter);
    ~Bluez5ProfileMap();
    void connect(const std::string &address, const std::string &instanceName, BluetoothMapCallback callback) final;
    void disconnect(const std::string &address, const std::string &sessionId, BluetoothMapCallback callback) final;
    void notifySessionStatus(const std::string &address, bool createdOrRemoved) final;
    void getMessageFilters(const std::string &sessionKey, const std::string &sessionId, BluetoothMapListFiltersResultCallback callback);
    void getFolderList(const std::string &sessionKey, const std::string &sessionId, const uint16_t &startOffset, const uint16_t &maxCount, BluetoothMapGetFoldersCallback callback);
    void setFolder(const std::string &sessionKey, const std::string &sessionId, const std::string &folder, BluetoothResultCallback callback);
    void getMessageList(const std::string &sessionKey, const std::string &sessionId, const std::string &folder, const BluetoothMapPropertiesList &filters, BluetoothMapGetMessageListCallback callback);
    void getMessage(const std::string &sessionKey, const std::string &messageHandle, bool attachment, const std::string &destinationFile, BluetoothResultCallback callback);
    void setMessageStatus(const std::string &sessionKey, const std::string &messageHandle, const std::string &statusIndicator, bool statusValue, BluetoothResultCallback callback);
private:
    Bluez5Adapter *mAdapter;
    std::map <std::string, Bluez5ObexTransfer*> mTransfersMap;
    std::string convertSessionKey(const std::string &address);
    std::string getSessionIdFromSessionPath(const std::string &sessionPath);
    void createSession(const std::string &address, const std::string &instanceName, BluetoothMapCallback callback);
    void getFolderListCb(BluezObexMessageAccess1* tObjectMapProxy, BluetoothMapGetFoldersCallback callback, GAsyncResult *result);
    void parseGetFolderListResponse(GVariant *outFolderList,std::vector<std::string> &folders);
    GVariant * buildGetFolderListParam(const uint16_t &startOffset, const uint16_t &maxCount);
    void getMessageListCb(BluezObexMessageAccess1* tObjectMapProxy, BluetoothMapGetMessageListCallback callback, GAsyncResult *result);
    GVariant * buildGetMessageListParam(const BluetoothMapPropertiesList &filters);
    void parseGetMessageListResponse(GVariant *outMessageList,BluetoothMessageList &messageList);
    void addMessageProperties(std::string& key , GVariant* value , BluetoothMapPropertiesList &messageProperties);
    BluezObexMessage1* createMessageHandleProxyUsingPath(const std::string &objectPath);
    void removeTransfer(const std::string &objectPath);
    void startTransfer(const std::string &objectPath, BluetoothResultCallback callback, Bluez5ObexTransfer::TransferType type);
    void updateActiveTransfer(const std::string &objectPath, Bluez5ObexTransfer *transfer, BluetoothResultCallback callback);
};

#endif
