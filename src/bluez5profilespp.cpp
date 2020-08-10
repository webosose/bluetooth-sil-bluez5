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

#include "bluez5profilespp.h"
#include "logging.h"
#include "bluez5adapter.h"
#include "bluez5agent.h"
#include "bluez5profilespp.h"
#include "utils.h"
#include "asyncutils.h"

const std::string BLUETOOTH_PROFILE_SPP_UUID = "00001101-0000-1000-8000-00805f9b34fb";
const std::string BASE_OBJ_PATH = "/bluetooth/profile/serial_port/";

Bluez5ProfileSpp::Bluez5ProfileSpp(Bluez5Adapter *adapter):
	Bluez5ProfileBase(adapter, BLUETOOTH_PROFILE_SPP_UUID), mAdapter(adapter), mConn(0)
{
	GError *error = nullptr;
	mConn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (error)
	{
		ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "Failed to connect on system bus %s", error->message);
		g_error_free(error);
	}
	initialiseChannelIds();
}

Bluez5ProfileSpp::~Bluez5ProfileSpp()
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
}

void Bluez5ProfileSpp::initialiseChannelIds()
{
	mChannelIdList = {
		{22, false},
		{6 , false},
	};
}

int Bluez5ProfileSpp::registerProfile(spDeviceInfo &deviceInfo, std::string objPath, BluezProfileManager1 *proxy)
{
	GVariant *profileVariant;
	GVariantBuilder profileBuilder;
	GError *error = nullptr;

	if (!g_variant_is_object_path(objPath.c_str())) {
		ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "ObjectPath validation failed");
		return BLUETOOTH_ERROR_FAIL;
	}

	g_variant_builder_init(&profileBuilder, G_VARIANT_TYPE("(osa{sv})"));

	g_variant_builder_add (&profileBuilder, "o", objPath.c_str());

	g_variant_builder_add (&profileBuilder, "s", (deviceInfo->mUuid).c_str());

	g_variant_builder_open(&profileBuilder, G_VARIANT_TYPE("a{sv}"));

	g_variant_builder_open(&profileBuilder, G_VARIANT_TYPE("{sv}"));
	g_variant_builder_add (&profileBuilder, "s", "Channel");
	g_variant_builder_add (&profileBuilder, "v", g_variant_new_uint16(deviceInfo->mChannelId));
	g_variant_builder_close(&profileBuilder);

	g_variant_builder_open(&profileBuilder, G_VARIANT_TYPE("{sv}"));
	g_variant_builder_add (&profileBuilder, "s", "Service");
	g_variant_builder_add (&profileBuilder, "v",
			g_variant_new_string((deviceInfo->mUuid).c_str()));
	g_variant_builder_close(&profileBuilder);

	g_variant_builder_open(&profileBuilder, G_VARIANT_TYPE("{sv}"));
	g_variant_builder_add (&profileBuilder, "s", "RequireAuthorization");
	g_variant_builder_add (&profileBuilder, "v", g_variant_new_boolean(FALSE));
	g_variant_builder_close(&profileBuilder);

	g_variant_builder_open(&profileBuilder, G_VARIANT_TYPE("{sv}"));
	g_variant_builder_add (&profileBuilder, "s", "RequireAuthentication");
	g_variant_builder_add (&profileBuilder, "v", g_variant_new_boolean(FALSE));
	g_variant_builder_close(&profileBuilder);

	g_variant_builder_open(&profileBuilder, G_VARIANT_TYPE("{sv}"));
	g_variant_builder_add (&profileBuilder, "s", "Name");
	g_variant_builder_add (&profileBuilder, "v",
			g_variant_new_string((deviceInfo->mName).c_str()));
	g_variant_builder_close(&profileBuilder);

	g_variant_builder_open(&profileBuilder, G_VARIANT_TYPE("{sv}"));
	g_variant_builder_add (&profileBuilder, "s", "Role");

	if (deviceInfo->mDeviceRole) {
		g_variant_builder_add (&profileBuilder, "v",
				g_variant_new_string("server"));
	} else {
		g_variant_builder_add (&profileBuilder, "v",
				g_variant_new_string("client"));
	}

	g_variant_builder_close(&profileBuilder);

	g_variant_builder_close(&profileBuilder);
	profileVariant = g_variant_builder_end(&profileBuilder);

	GVariant *ret = g_dbus_proxy_call_sync (G_DBUS_PROXY(proxy),
				"RegisterProfile",
				profileVariant,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);
	if (error)
	{
		ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "Failed to register profileManager due to  %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_FAIL;
	}

	if (ret) g_variant_unref(ret);
	return BLUETOOTH_ERROR_NONE;
}

void Bluez5ProfileSpp::getProperties(const std::string &address, BluetoothPropertiesResultCallback callback)
{
	UNUSED(address);
	UNUSED(callback);
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
}

void Bluez5ProfileSpp::getProperty(const std::string &address, BluetoothProperty::Type type,
	                         BluetoothPropertyResultCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	BluetoothProperty prop(type);
	Bluez5Device *device = mAdapter->findDevice(address);
	if (!device)
	{
		callback(BLUETOOTH_ERROR_PARAM_INVALID, prop);
		return;
	}
	bool adapterConnectionStatus = device->getConnected();
	prop.setValue<bool>(adapterConnectionStatus);
	callback(BLUETOOTH_ERROR_NONE, prop);
}

void Bluez5ProfileSpp::getChannelState(const std::string &address, const std::string &uuid, BluetoothChannelStateResultCallback callback)
{
	auto deviceIterator = mConnectedDevices.begin();
	std::string lowerCaseAddress = convertAddressToLowerCase(address);

	while (deviceIterator != mConnectedDevices.end())
	{
		if ((deviceIterator->second->mDeviceAddress == lowerCaseAddress) && (deviceIterator->second->mUuid == uuid))
		{
			callback(BLUETOOTH_ERROR_NONE, true);
			return;
		}

		++deviceIterator;
	}

	callback(BLUETOOTH_ERROR_NONE, false);
}

gboolean Bluez5ProfileSpp::onHandleNewConnection (BluezProfile1 *interface,
			GDBusMethodInvocation *invocation,
			const gchar           *device,
			const GVariant        *fd,
			const GVariant        *fd_props,
			gpointer              user_data)
{
	UNUSED(interface);
	SppDeviceInfo* sppDevice = static_cast<SppDeviceInfo*>(user_data);
	return sppDevice->mSppProfile->handleNewConnection(invocation, device, fd, fd_props, sppDevice);
}

gboolean Bluez5ProfileSpp::onHandleRequestDisconnection (BluezProfile1 *interface,
			GDBusMethodInvocation *invocation,
			const gchar           *device,
			gpointer              user_data)
{
	SppDeviceInfo* devieInfo = static_cast<SppDeviceInfo*>(user_data);
	return devieInfo->mSppProfile->handleRequestDisconnection(interface, invocation, device, devieInfo);
}

gboolean Bluez5ProfileSpp::onHandleRelease (BluezProfile1 *interface,
			GDBusMethodInvocation *invocation,
			gpointer              user_data)
{
	UNUSED(interface);
	UNUSED(user_data);

	SppDeviceInfo* devieInfo = static_cast<SppDeviceInfo*>(user_data);
	devieInfo->mSppProfile->handleRelease();

	// finished with method call; no reply sent
	g_dbus_method_invocation_return_value(invocation, NULL);
	return TRUE;
}

gboolean Bluez5ProfileSpp::handleNewConnection (GDBusMethodInvocation *invocation, const gchar *device, const GVariant *fd,
							const GVariant *fd_props, SppDeviceInfo* devieInfo)
{
	GDBusMessage *message;
	GError *error = nullptr;
	GUnixFDList *fd_list;

	UNUSED(fd);
	UNUSED(fd_props);
	message = g_dbus_method_invocation_get_message (invocation);
	fd_list = g_dbus_message_get_unix_fd_list (message);

	devieInfo->mSockfd = g_unix_fd_list_get (fd_list, 0, &error);

	if (error)
	{
		ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "Failed to handle_new_connection due to  %s", error->message);
		g_error_free(error);
		return FALSE;
	}

	// finished with method call; no reply sent
	g_dbus_method_invocation_return_value(invocation, NULL);

	Bluez5Device *bluezDevice = mAdapter->findDeviceByObjectPath(device);
	std::string deviceAddress;
	if (bluezDevice)
	{
		deviceAddress = bluezDevice->getAddress();
		devieInfo->mDeviceAddress = bluezDevice->getAddress();
	}

	devieInfo->mAdapterAddress = mAdapter->getAddress();

	getSppObserver()->channelStateChanged(mAdapter->getAddress(), deviceAddress, devieInfo->mUuid, devieInfo->mChannelId, true);

	devieInfo->mChannel = g_io_channel_unix_new (devieInfo->mSockfd);

	g_io_channel_set_encoding(devieInfo->mChannel, NULL, &error);
	if (error)
	{
		ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "Failed to set encoding due to  %s", error->message);
		g_error_free(error);
	}

	devieInfo->mIoWatchId = g_io_add_watch (devieInfo->mChannel, G_IO_IN, ioCallback, devieInfo);

	DEBUG("devieInfo->mIoWatchId = %d", devieInfo->mIoWatchId);
	return TRUE;
}

gboolean Bluez5ProfileSpp::handleRequestDisconnection (BluezProfile1 *interface, GDBusMethodInvocation *invocation, const gchar *device, SppDeviceInfo* devieInfo)
{
	GError *error = nullptr;

	UNUSED(device);
	UNUSED(interface);

	getSppObserver()->channelStateChanged(devieInfo->mAdapterAddress, devieInfo->mDeviceAddress, devieInfo->mUuid, devieInfo->mChannelId, false);

	if (devieInfo->mChannel)
	{
		if (devieInfo->mIoWatchId)
			g_source_remove(devieInfo->mIoWatchId);
		g_io_channel_shutdown(devieInfo->mChannel, TRUE, &error);
		g_io_channel_unref (devieInfo->mChannel);
		if (error)
		{
			ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "Failed to shutdown channel %s",error->message);
			g_error_free(error);
		}
		devieInfo->mChannel = nullptr;
	}

	if (devieInfo->mSockfd > 0)
	{
		close(devieInfo->mSockfd);
		devieInfo->mSockfd = -1;
	}

	// finished with method call; no reply sent
	g_dbus_method_invocation_return_value(invocation, NULL);

	if ((devieInfo->mDeviceRole == CLIENT) && devieInfo->mInterface)
	{
		std::string objPath = BASE_OBJ_PATH + std::to_string(devieInfo->mChannelId);
		auto unRegisterCallback = [this](GAsyncResult *result) {
			GError *error = nullptr;
			gboolean ret;

			ret = bluez_profile_manager1_call_unregister_profile_finish(mAdapter->getProfileManager(), result, &error);
			if (error)
			{
				g_error_free(error);
				return;
			}
		};
		bluez_profile_manager1_call_unregister_profile(mAdapter->getProfileManager(), objPath.c_str(), NULL, glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(unRegisterCallback));
		g_object_unref(devieInfo->mInterface);
		removeConnectedDevice(devieInfo->mChannelId);
		deallocateChannelId(devieInfo->mChannelId);
	}

	return TRUE;
}

gboolean Bluez5ProfileSpp::handleRelease()
{
	auto deviceIterator = mConnectedDevices.begin();

	while (deviceIterator != mConnectedDevices.end())
	{
		GError *error = nullptr;
		g_io_channel_shutdown(deviceIterator->second->mChannel, TRUE, &error);
		if (error)
		{
			ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "Failed to release profile on system bus %s",error->message);
			g_error_free(error);
		}
		g_io_channel_unref (deviceIterator->second->mChannel);
		g_object_unref(deviceIterator->second->mInterface);
		++deviceIterator;
	}

	mConnectedDevices.clear();
	return TRUE;
}

gboolean Bluez5ProfileSpp::ioCallback(GIOChannel * io, GIOCondition condition, gpointer data)
{
	SppDeviceInfo* sppDevice = static_cast<SppDeviceInfo*>(data);

	return sppDevice->mSppProfile->handleRxData(io, condition, sppDevice->mChannelId, sppDevice->mAdapterAddress);
}

gboolean Bluez5ProfileSpp::handleRxData(GIOChannel *io, GIOCondition condition, BluetoothSppChannelId channelId, const std::string &adapterAddress)
{
	char buf[1024] = { 0 };
	gsize bytesRead = 0;
	GError *error = nullptr;
	UNUSED(condition);

	do
	{
		GIOStatus status = g_io_channel_read_chars (io, buf, sizeof(buf), &bytesRead, &error);

		if (status == G_IO_STATUS_NORMAL && bytesRead > 0)
		{
			getSppObserver()->dataReceived(channelId, adapterAddress, (const uint8_t *)buf, bytesRead);
			DEBUG("received [%s] bytes_read[%lu]", buf, bytesRead);
		}

		if (error)
		{
			ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "Failed to read data due to %s",error->message);
			g_error_free(error);
		}

	} while (bytesRead > 0);

	return TRUE;
}


void Bluez5ProfileSpp::connectUuid(const std::string &address, const std::string &uuid, BluetoothChannelResultCallback callback)
{
	Bluez5Device *device = mAdapter->findDevice(address);
	if (!device)
	{
		DEBUG("Could not find device with address %s while trying to connect", address.c_str());
		callback(BLUETOOTH_ERROR_NOT_READY, BLUETOOTH_SPP_CHANNEL_ID_INVALID);
		return;
	}

	BluetoothSppChannelId channelId = allocateChannelId();
	if (!channelId)
	{
		ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "Failed to allocate Channel Id");
		callback(BLUETOOTH_ERROR_FAIL, BLUETOOTH_SPP_CHANNEL_ID_INVALID);
		return;
	}

	SppDeviceInfo * sppDevInfo = new (std::nothrow) SppDeviceInfo(this, channelId, CLIENT, "SerialPort", uuid);
	if (!sppDevInfo)
	{
		//Remove ChannelId used
		deallocateChannelId(channelId);
		ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "Failed to allocate memory for sppDevice");
		callback(BLUETOOTH_ERROR_NOMEM, BLUETOOTH_SPP_CHANNEL_ID_INVALID);
		return;
	}

	spDeviceInfo channelInfo(sppDevInfo);
	BluetoothError error = createSkeletonAndExport(uuid, channelInfo);

	if (error != BLUETOOTH_ERROR_NONE)
	{
		callback(BLUETOOTH_ERROR_NOT_READY, BLUETOOTH_SPP_CHANNEL_ID_INVALID);
		return;
	}

	BluezProfile1 *mInterface = channelInfo->mInterface;
	auto ConnectedCallback = [this, callback, channelId, mInterface](BluetoothError error)
	{
		if (error == BLUETOOTH_ERROR_NONE)
		{
			callback(BLUETOOTH_ERROR_NONE, channelId);
		}
		else
		{
			std::string objPath = BASE_OBJ_PATH + std::to_string(channelId);
			bluez_profile_manager1_call_unregister_profile_sync(mAdapter->getProfileManager(), objPath.c_str(), NULL, NULL);
			if (mInterface)
				g_object_unref(mInterface);
			deallocateChannelId(channelId);
			callback(BLUETOOTH_ERROR_NOT_READY, BLUETOOTH_SPP_CHANNEL_ID_INVALID);
		}
	};

	this->mConnectedDevices[channelInfo->mChannelId] = std::move(channelInfo);
	device->connect(uuid, ConnectedCallback);
}

void Bluez5ProfileSpp::disconnectUuid(const BluetoothSppChannelId channelId, BluetoothResultCallback callback)
{
	auto deviceIterator = mConnectedDevices.find(channelId);
	if (deviceIterator == mConnectedDevices.end())
	{
		callback(BLUETOOTH_ERROR_FAIL);
		return;
	}

	SppDeviceInfo *sppConnectionInfo = (deviceIterator->second).get();

	if (!sppConnectionInfo)
	{
		callback(BLUETOOTH_ERROR_FAIL);
		return;
	}

	Bluez5Device *device = mAdapter->findDevice(sppConnectionInfo->mDeviceAddress);
	if (!device)
	{
		DEBUG("Could not find device with address %s while trying to connect", sppConnectionInfo->mDeviceAddress.c_str());
		callback(BLUETOOTH_ERROR_NOT_READY);
		return;
	}

	auto disConnectedCallback = [this, callback, channelId, sppConnectionInfo](BluetoothError error)
	{
		if (error == BLUETOOTH_ERROR_NONE)
		{
			callback(BLUETOOTH_ERROR_NONE);
		}
		else
		{
			callback(BLUETOOTH_ERROR_NOT_READY);
		}
	};

	device->disconnect(sppConnectionInfo->mUuid, disConnectedCallback);
}

void Bluez5ProfileSpp::writeData(const BluetoothSppChannelId channelId, const uint8_t *data, const uint32_t size, BluetoothResultCallback callback)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	UNUSED(data);
	UNUSED(size);
	auto deviceIterator = mConnectedDevices.find(channelId);
	if (deviceIterator == mConnectedDevices.end())
	{
		callback(BLUETOOTH_ERROR_FAIL);
		return;
	}

	SppDeviceInfo *sppConnectionInfo = (deviceIterator->second).get();
	int status = write(sppConnectionInfo->mSockfd, data, size);

	if (status < 0) {
		perror("client: write to socket failed!\n");
	}

	callback(BLUETOOTH_ERROR_NONE);
}

BluetoothError Bluez5ProfileSpp::createChannel(const std::string &name, const std::string &uuid)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	BluetoothSppChannelId channelId = allocateChannelId();
	if (!channelId)
	{
		ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "Failed to get channelId");
		return BLUETOOTH_ERROR_FAIL;
	}

	SppDeviceInfo * sppDevInfo = new (std::nothrow) SppDeviceInfo(this, channelId, SERVER, name, uuid);

	if (!sppDevInfo)
	{
		deallocateChannelId(channelId);
		ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "Failed to allocate memory for sppDevInfo");
		return BLUETOOTH_ERROR_NOMEM;
	}

	spDeviceInfo channelInfo(sppDevInfo);

	BluetoothError error = createSkeletonAndExport(uuid, channelInfo);

	if (error != BLUETOOTH_ERROR_NONE)
	{
		return error;
	}

	mConnectedDevices[channelId] = std::move(channelInfo);
	return BLUETOOTH_ERROR_NONE;
}

BluetoothError Bluez5ProfileSpp::removeChannel(const std::string &uuid)
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);

	SppDeviceInfo *sppConnectionInfo = getSppDevice(uuid);

	if (!sppConnectionInfo)
	{
		return BLUETOOTH_ERROR_FAIL;
	}

	if (sppConnectionInfo->mInterface)
	{
		std::string objPath = BASE_OBJ_PATH + std::to_string(sppConnectionInfo->mChannelId);
		bluez_profile_manager1_call_unregister_profile_sync(mAdapter->getProfileManager(), objPath.c_str(), NULL, NULL);
		g_object_unref(sppConnectionInfo->mInterface);
		getSppObserver()->channelStateChanged(sppConnectionInfo->mAdapterAddress, sppConnectionInfo->mDeviceAddress, uuid, sppConnectionInfo->mChannelId, false);
		removeConnectedDevice(sppConnectionInfo->mChannelId);
		deallocateChannelId(sppConnectionInfo->mChannelId);
	}

	return BLUETOOTH_ERROR_NONE;
}

BluetoothSppChannelId Bluez5ProfileSpp::allocateChannelId()
{
	BluetoothSppChannelId retChId = 0;
	for (auto& ids : mChannelIdList)
	{
		if (!ids.second)
		{
			retChId = ids.first;
			ids.second = true;
			break;
		}
	}

	return retChId;
}

void Bluez5ProfileSpp::deallocateChannelId(BluetoothSppChannelId channelId)
{
	 if(mChannelIdList[channelId])
		mChannelIdList[channelId] = false;
}

bool Bluez5ProfileSpp::removeConnectedDevice(BluetoothSppChannelId channelId)
{
	return mConnectedDevices.erase(channelId);
}

Bluez5ProfileSpp::SppDeviceInfo* Bluez5ProfileSpp::getSppDevice(const BluetoothSppChannelId channelId)
{
	SppDeviceInfo* deviceInfo = nullptr;

	auto deviceIterator = mConnectedDevices.find(channelId);
	if (deviceIterator != mConnectedDevices.end())
		deviceInfo = (deviceIterator->second).get();

	return deviceInfo;
}

Bluez5ProfileSpp::SppDeviceInfo* Bluez5ProfileSpp::getSppDevice(const std::string& uuid)
{
	SppDeviceInfo* deviceInfo = nullptr;

	auto matchUuid = [&uuid](std::pair<const unsigned char, std::unique_ptr<Bluez5ProfileSpp::SppDeviceInfo>>& device)
	{
		if((device.second)->mUuid == uuid)
			return true;

		return false;
	};

	ConnectedDevice::iterator deviceIterator = std::find_if(mConnectedDevices.begin(), mConnectedDevices.end(), matchUuid);

	if (deviceIterator != mConnectedDevices.end())
		deviceInfo = (deviceIterator->second).get();

	return deviceInfo;
}

BluetoothError Bluez5ProfileSpp::createSkeletonAndExport(std::string uuid, spDeviceInfo &channelInfo)
{
	GError *error = nullptr;
	UNUSED(uuid);
	std::string objPath = BASE_OBJ_PATH + std::to_string(channelInfo->mChannelId);

	if (registerProfile(channelInfo, objPath, mAdapter->getProfileManager()))
	{
		deallocateChannelId(channelInfo->mChannelId);
		ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "Failed to register to profile to manager");
		return BLUETOOTH_ERROR_NOT_READY;
	}

	channelInfo->mInterface = bluez_profile1_skeleton_new();

	g_signal_connect(channelInfo->mInterface,
		"handle_new_connection",
		G_CALLBACK (onHandleNewConnection),
		channelInfo.get());

	g_signal_connect(channelInfo->mInterface,
		"handle_request_disconnection",
		G_CALLBACK (onHandleRequestDisconnection),
		channelInfo.get());

	g_signal_connect(channelInfo->mInterface,
		"handle_release",
		G_CALLBACK (onHandleRelease),
		channelInfo.get());


	if (mConn && !g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (channelInfo->mInterface),
						mConn,
						objPath.c_str(),
						&error))
	{
		deallocateChannelId(channelInfo->mChannelId);
		ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "Failed to export profile on system bus");
		if (error)
		{
			ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "Error message %s", error->message);
			g_error_free(error);
		}
		return BLUETOOTH_ERROR_NOT_READY;
	}

	return BLUETOOTH_ERROR_NONE;
}
