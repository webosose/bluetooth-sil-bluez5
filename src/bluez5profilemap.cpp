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

const std::string BLUETOOTH_PROFILE_MAS_UUID = "00001132-0000-1000-8000-00805f9b34fb";
using namespace std::placeholders;

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

	obexClient->createSession(Bluez5ObexSession::Type::MAP, address, sessionCreateCallback, instanceName);
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
