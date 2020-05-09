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

#ifndef BLUEZ5PROFILEPBAP_H
#define BLUEZ5PROFILEPBAP_H

#include <string>
#include <map>

#include <bluetooth-sil-api.h>
#include <bluez5obexprofilebase.h>
#include "bluez5profilebase.h"

class Bluez5Adapter;

class Bluez5ProfilePbap : public Bluez5ObexProfileBase,
                        public BluetoothPbapProfile
{
public:
    Bluez5ProfilePbap(Bluez5Adapter *adapter);
    ~Bluez5ProfilePbap();
    void supplyAccessConfirmation(BluetoothPbapAccessRequestId accessRequestId, bool accept, BluetoothResultCallback callback);
    void setPhoneBook(const std::string &address, const std::string &repository, const std::string &object, BluetoothResultCallback callback);
    void getPhonebookSize(const std::string &address, BluetoothPbapGetSizeResultCallback callback);
    void vCardListing(const std::string &address, BluetoothPbapVCardListResultCallback callback);
    void getPhoneBookProperties(const std::string &address, BluetoothPbapApplicationParameterCallback callback);
    void getvCardFilters(const std::string &address, BluetoothPbapListFiltersResultCallback callback);
    void pullvCard(const std::string &address, const std::string &targetFile, const std::string &vCardHandle, const std::string &vCardVersion, BluetoothPbapVCardFilterList &vCardFilters, BluetoothResultCallback callback);
    void updateProperties(GVariant *changedProperties);
    void pullPhoneBook(const std::string &address,const std::string &destinationFile,const std::string &vCardVersion,BluetoothPbapVCardFilterList &vCardFilters, const uint16_t &startIndex, const uint16_t &maxCount, BluetoothGetPhoneBookResultCallback callback);
    void searchPhoneBook (const std::string &address, const std::string &searchOrder, const std::string &searchKey, const std::string &searchValue, BluetoothPbapVCardListResultCallback callback);
private:
    bool isObjectValid( const std::string &object);
    bool isRepositoryValid( const std::string &repository);
    bool isSearchOrderValid( const std::string &searchOrder);
    bool isSearchKeyValid( const std::string &searchKey);
    bool isVCardVersionValid( const std::string &vCardVersion);
    void initializePbapApplicationParameters();
    std::string convertToSupportedVcardVersion(const std::string &vCardVersion);
    void parseAllProperties(GVariant *propsVar);
    void addPropertyFromVariant(const std::string &key, GVariant *valueVar);
    void notifyUpdatedProperties();
    void updateVersion();
    std::string getDeviceAddress() const { return mDeviceAddress; }
    inline uint64_t nextTransferId() { return ++mTranfserIdCounter; }
    GVariant * setFilters(const std::string &vCardVersion, BluetoothPbapVCardFilterList &vCardFilters);
    GVariant * setSearchFilters(const std::string &searchOrder);
    void startTransfer( const std::string &objectPath, BluetoothResultCallback callback, Bluez5ObexTransfer::TransferType type);
    void startPhoneBookTransfer(const std::string &objectPath, const std::string &address ,Bluez5ObexTransfer::TransferType type);
    void removeTransfer(const std::string &objectPath);
    void updateActiveTransfer(const std::string &objectPath, Bluez5ObexTransfer *transfer, BluetoothResultCallback callback);
    void monitorTransfer(const std::string &objectPath, const std::string &address, Bluez5ObexTransfer *transfer);
    GVariant * setPhoneBookFilters(const std::string &vCardVersion, BluetoothPbapVCardFilterList &vCardFilters , uint16_t startOffset , uint16_t maxCount);
    void clearTransferMap();
    uint64_t mTranfserIdCounter;
    std::string mDeviceAddress;
    Bluez5Adapter *mAdapter;
    BluezObexPhonebookAccess1 *mObjectPhonebookProxy;
    std::map <std::string, Bluez5ObexTransfer*> mTransfersMap;
    std::map <std::string, std::string> mTransferStateMap;
    FreeDesktopDBusProperties *mPropertiesProxy;
    BluetoothPbapApplicationParameters pbapApplicationParameters;
};

#endif
