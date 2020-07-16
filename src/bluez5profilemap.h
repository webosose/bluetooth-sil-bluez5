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
    void getFolderList(const std::string &sessionKey, const std::string &sessionId, const uint16_t &startOffset, const uint16_t &maxCount, BluetoothMapGetFoldersCallback callback);
private:
    Bluez5Adapter *mAdapter;
    std::string convertSessionKey(const std::string &address);
    std::string getSessionIdFromSessionPath(const std::string &sessionPath);
    void createSession(const std::string &address, const std::string &instanceName, BluetoothMapCallback callback);
    void getMessageFilters(const std::string &sessionKey, const std::string &sessionId, BluetoothMapListFiltersResultCallback callback);
    void getFolderListCb(BluezObexMessageAccess1* tObjectMapProxy, BluetoothMapGetFoldersCallback callback, GAsyncResult *result);
    void parseGetFolderListResponse(GVariant *outFolderList,std::vector<std::string> &folders);
    GVariant * buildGetFolderListParam(const uint16_t &startOffset, const uint16_t &maxCount);
};

#endif
