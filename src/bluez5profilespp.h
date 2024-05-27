// Copyright (c) 2018-2020 LG Electronics, Inc.
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

#ifndef BLUEZ5PROFILESPP_H
#define BLUEZ5PROFILESPP_H


#include "bluez5profilebase.h"

#include <fcntl.h>
#include <memory>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <sys/socket.h>

extern "C" {
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5Adapter;
class Bluez5ProfileSpp;

class Bluez5ProfileSpp : public Bluez5ProfileBase,
						 public BluetoothSppProfile
{
class SppDeviceInfo;
public:
	enum DeviceRole
	{
		CLIENT = 0,
		SERVER
	};

	Bluez5ProfileSpp(Bluez5Adapter *adapter);
	~Bluez5ProfileSpp();
	void getProperties(const std::string &address, BluetoothPropertiesResultCallback  callback);
	void getProperty(const std::string &address, BluetoothProperty::Type type,
	                         BluetoothPropertyResultCallback callback);
	void getChannelState(const std::string &address, const std::string &uuid, BluetoothChannelStateResultCallback callback);
	void connectUuid(const std::string &address, const std::string &uuid, BluetoothChannelResultCallback callback);
	void disconnectUuid(const BluetoothSppChannelId channelId, BluetoothResultCallback callback);
	void writeData(const BluetoothSppChannelId channelId, const uint8_t *data, const uint32_t size, BluetoothResultCallback callback);
	BluetoothError createChannel(const std::string &name, const std::string &uuid);
	BluetoothError removeChannel(const std::string &uuid);
	gboolean handleNewConnection (GDBusMethodInvocation *invocation, const gchar *device, const GVariant *fd,
								const GVariant *fd_props, SppDeviceInfo* deviceInfo);
	gboolean handleRelease();
	gboolean handleRxData(GIOChannel *io, GIOCondition condition, BluetoothSppChannelId id, const std::string &adapterAddress);
	gboolean handleRequestDisconnection (BluezProfile1 *interface, GDBusMethodInvocation *invocation, const gchar *device, SppDeviceInfo* deviceInfo);

	static gboolean onHandleNewConnection (BluezProfile1 *interface, GDBusMethodInvocation *invocation, const gchar *device, const GVariant *fd,
										   const GVariant *fd_props, gpointer user_data);
	static gboolean ioCallback(GIOChannel * io, GIOCondition condition, gpointer data);
	static gboolean onHandleRequestDisconnection (BluezProfile1 *interface, GDBusMethodInvocation *invocation, const gchar *device, gpointer user_data);
	static gboolean onHandleRelease (BluezProfile1 *interface, GDBusMethodInvocation *invocation, gpointer user_data);

private:
	class SppDeviceInfo
	{
	public:
		SppDeviceInfo(Bluez5ProfileSpp* sppProfile, BluetoothSppChannelId connectedChannelID, DeviceRole deviceRole, std::string name, std::string uuid)
			: mName(name)
			, mUuid (uuid)
			, mChannelId(connectedChannelID)
			, mDeviceRole(deviceRole)
			, mInterface(nullptr)
			, mSockfd(-1)
			, mChannel(nullptr)
			, mIoWatchId(0)
			, mSppProfile(sppProfile)
		{
		}

		std::string mAdapterAddress;
		std::string mDeviceAddress;
		std::string mName;
		std::string mUuid;

		BluetoothSppChannelId mChannelId;
		DeviceRole mDeviceRole;
		BluezProfile1 *mInterface;
		gint mSockfd;
		GIOChannel *mChannel;
		guint mIoWatchId;
		Bluez5ProfileSpp *mSppProfile;
	};
	typedef std::unique_ptr<SppDeviceInfo> spDeviceInfo;
	typedef std::unordered_map<BluetoothSppChannelId, spDeviceInfo> ConnectedDevice;

	BluetoothSppChannelId allocateChannelId();
	void deallocateChannelId(BluetoothSppChannelId channelId);
	void initialiseChannelIds();
	bool removeConnectedDevice(BluetoothSppChannelId channelId);

	SppDeviceInfo* getSppDevice(const BluetoothSppChannelId channelId);
	SppDeviceInfo* getSppDevice(const std::string &uuid);

	Bluez5Adapter *mAdapter;
	GDBusConnection *mConn;

	ConnectedDevice mConnectedDevices;
	std::unordered_map<BluetoothSppChannelId, bool> mChannelIdList;

public:
	int registerProfile(spDeviceInfo &deviceInfo, std::string objPath, BluezProfileManager1 *proxy);
	BluetoothError createSkeletonAndExport(std::string uuid, spDeviceInfo &deviceInfo);
};

#endif // BLUEZ5PROFILESPP_H
