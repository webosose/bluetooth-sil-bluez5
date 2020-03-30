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
    void getPhoneBookProperties(const std::string &address, BluetoothPropertiesResultCallback callback);
    void getvCardFilters(const std::string &address, BluetoothPbapListFiltersResultCallback callback);
    static void handlePropertiesChanged(Bluez5ProfilePbap *, gchar *interface, GVariant *changedProperties, GVariant *invalidatedProperties, gpointer userData);
private:
    bool isObjectValid( const std::string object);
    bool isRepositoryValid( const std::string repository);
    void setErrorProperties();
    void parseAllProperties(BluetoothPropertiesList& properties, GVariant *propsVar);
    void addPropertyFromVariant(BluetoothPropertiesList& properties, const std::string &key, GVariant *valueVar);
    void notifyUpdatedProperties();
    void updateVersion();
    std::string getDeviceAddress() const { return mDeviceAddress; }
    std::string mDeviceAddress;
    BluezObexPhonebookAccess1 *mObjectPhonebookProxy;
    FreeDesktopDBusProperties *mPropertiesProxy;
    std::string mFolder;
    std::string mPrimaryCounter;
    std::string mSecondaryCounter;
    std::string mDatabaseIdentifier;
    bool mFixedImageSize;
    unsigned long mHandleId;
};

#endif
