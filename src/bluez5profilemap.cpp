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

void Bluez5ProfileMap::disconnect(const std::string &address, const std::string &sessionId, BluetoothMapCallback callback)
{
	DEBUG("Disconnecting with device %s on sessionId %s ", address.c_str(), sessionId.c_str());
	std::string sessionKey = findSessionKey(sessionId);
	DEBUG("Disconnecting with sessionKey %s  ", sessionKey.c_str());
	std::string instanceName;
	if(!sessionKey.empty())
	{
		std::size_t pos = sessionKey.find("_");

		if(pos != std::string::npos)
			instanceName = sessionKey.substr(pos+1);
		removeSession(sessionKey);
		DEBUG("Disconnecting with instanceName %s  ", instanceName.c_str());
		notifySessionStatus(sessionKey, false);
		callback(BLUETOOTH_ERROR_NONE,instanceName);
	}
	else
	{
		callback(BLUETOOTH_ERROR_FAIL,"");
	}

}

std::string Bluez5ProfileMap::findSessionKey(const std::string &sessionId)
{
	std::string temp;
	auto sessionIter = mSessionToAddressMap.find(sessionId);
	if (sessionIter == mSessionToAddressMap.end())
		return temp;

	return sessionIter->second;
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
		storeSessionToAddress(sessionId,sessionkey);
		notifySessionStatus(sessionkey, true);

		callback(BLUETOOTH_ERROR_NONE,sessionId);
	};

	obexClient->createSession(Bluez5ObexSession::Type::MAP, address, sessionCreateCallback, instanceName);
}

std::string Bluez5ProfileMap::getSessionIdFromSessionPath(const std::string &sessionPath)
{
	std::size_t idPos = sessionPath.find_last_of("/");
	return ( idPos == std::string::npos ? "" : sessionPath.substr(idPos + 1));
}

void Bluez5ProfileMap::storeSessionToAddress(const std::string &sessionId, const std::string &sessionkey)
{
	mSessionToAddressMap.insert(std::pair<std::string, std::string>(sessionId,sessionkey));
}

void Bluez5ProfileMap::removeSessionToAddress(const std::string &sessionkey)
{
	auto itr = mSessionToAddressMap.begin();
	while(itr != mSessionToAddressMap.end())
	{
		if(itr->second == sessionkey)
		{
			itr = mSessionToAddressMap.erase(itr);
			break;
		}
		itr++;
	}
}

void Bluez5ProfileMap::notifySessionStatus(const std::string &sessionkey, bool createdOrRemoved)
{
	if(!createdOrRemoved)
		removeSessionToAddress(sessionkey);
	Bluez5ObexProfileBase::notifySessionStatus(sessionkey, createdOrRemoved);
	//To be done for getStatus
}

