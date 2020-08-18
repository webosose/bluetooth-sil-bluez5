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

#include "bluez5profileavrcp.h"
#include "bluez5adapter.h"
#include "logging.h"
#include "utils.h"
#include "bluez5mediacontrol.h"
#include "bluez5mprisplayer.h"
#include "bluez5profilea2dp.h"
#include "bluez5mediaplayer.h"
#include <string>


const std::string BLUETOOTH_PROFILE_AVRCP_TARGET_UUID = "0000110c-0000-1000-8000-00805f9b34fb";
const std::string BLUETOOTH_PROFILE_AVRCP_REMOTE_UUID = "0000110e-0000-1000-8000-00805f9b34fb";

const std::map <std::string, BluetoothAvrcpPassThroughKeyCode> Bluez5ProfileAvcrp::mKeyMap = {
	{ "POWER",		KEY_CODE_POWER },
	{ "VOLUME UP",	KEY_CODE_VOLUME_UP },
	{ "VOLUME DOWN",KEY_CODE_VOLUME_DOWN },
	{ "MUTE",		KEY_CODE_MUTE },
	{ "PLAY",		KEY_CODE_PLAY},
	{ "STOP",		KEY_CODE_STOP },
	{ "PAUSE",		KEY_CODE_PAUSE},
	{ "FORWARD",	KEY_CODE_NEXT},
	{ "BACKWARD",	KEY_CODE_PREVIOUS },
	{ "REWIND",		KEY_CODE_REWIND},
	{ "FAST FORWARD",KEY_CODE_FAST_FORWARD}
};

Bluez5ProfileAvcrp::Bluez5ProfileAvcrp(Bluez5Adapter* adapter):
Bluez5ProfileBase(adapter, BLUETOOTH_PROFILE_AVRCP_REMOTE_UUID),
mMetaDataRequestId(0),
mMediaPlayStatusRequestId(0),
mConnected(false),
mConnectedDeviceAddress(""),
mConnectedController(false),
mConnectedTarget(false),
mObjectManager(nullptr),
mAddressedMediaPlayer(nullptr)
{
	g_bus_watch_name(G_BUS_TYPE_SYSTEM, "org.bluez", G_BUS_NAME_WATCHER_FLAGS_NONE,
		handleBluezServiceStarted, handleBluezServiceStopped, this, NULL);
}

Bluez5ProfileAvcrp::~Bluez5ProfileAvcrp()
{
	if (mObjectManager)
	{
		g_object_unref(mObjectManager);
		mObjectManager = nullptr;
	}
}

void Bluez5ProfileAvcrp::connect(const std::string& address, BluetoothResultCallback callback)
{
	auto connectCallback = [this, address, callback](BluetoothError error) {
		if (error != BLUETOOTH_ERROR_NONE)
		{
			DEBUG("AVRCP connect failed");
			callback(error);
			return;
		}
		DEBUG("AVRCP connected succesfully");
		callback(BLUETOOTH_ERROR_NONE);
	};

	if (!mConnected)
	{
		Bluez5ProfileBase::connect(address, connectCallback);
	}
	else
		connectCallback(BLUETOOTH_ERROR_DEVICE_ALREADY_CONNECTED);
}

void Bluez5ProfileAvcrp::handleBluezServiceStarted(GDBusConnection* conn, const gchar* name,
	const gchar* nameOwner, gpointer user_data)
{
	DEBUG("handleBluezServiceStarted");
	Bluez5ProfileAvcrp* avrcp = static_cast<Bluez5ProfileAvcrp*>(user_data);

	GError* error = 0;
	avrcp->mObjectManager = g_dbus_object_manager_client_new_sync(conn, G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
		"org.bluez", "/", NULL, NULL, NULL, NULL, &error);
	if (error)
	{
		ERROR(MSGID_OBJECT_MANAGER_CREATION_FAILED, 0, "Failed to create object manager: %s", error->message);
		g_error_free(error);
		return;
	}

	g_signal_connect(avrcp->mObjectManager, "object-added", G_CALLBACK(handleObjectAdded), avrcp);
	g_signal_connect(avrcp->mObjectManager, "object-removed", G_CALLBACK(handleObjectRemoved), avrcp);

	GList* objects = g_dbus_object_manager_get_objects(avrcp->mObjectManager);
	for (int n = 0; n < g_list_length(objects); n++)
	{
		auto object = static_cast<GDBusObject*>(g_list_nth(objects, n)->data);
		std::string objectPath = g_dbus_object_get_object_path(object);
		auto adapterPath = avrcp->mAdapter->getObjectPath();
		if (objectPath.compare(0, adapterPath.length(), adapterPath))
			continue;

		auto mediaPlayerInterface = g_dbus_object_get_interface(object, "org.bluez.MediaPlayer1");
		if (mediaPlayerInterface)
		{
			DEBUG("MediaPlayer interface");
			avrcp->addMediaPlayer(object);
			g_object_unref(mediaPlayerInterface);
		}
		g_object_unref(object);
	}
	g_list_free(objects);
}

void Bluez5ProfileAvcrp::handleBluezServiceStopped(GDBusConnection* conn, const gchar* name,
	gpointer user_data)
{
}

void Bluez5ProfileAvcrp::handleObjectAdded(GDBusObjectManager* objectManager, GDBusObject* object, void* userData)
{
	DEBUG("handleObjectAdded");
	Bluez5ProfileAvcrp* avrcp = static_cast<Bluez5ProfileAvcrp*>(userData);

	std::string objectPath = g_dbus_object_get_object_path(object);

	auto adapterPath = avrcp->mAdapter->getObjectPath();
	if (objectPath.compare(0, adapterPath.length(), adapterPath))
		return;

	auto mediaPlayerInterface = g_dbus_object_get_interface(object, "org.bluez.MediaPlayer1");
	if (mediaPlayerInterface)
	{
		DEBUG("Added: %s", objectPath.c_str());
		avrcp->addMediaPlayer(object);
		g_object_unref(mediaPlayerInterface);
	}
}

void Bluez5ProfileAvcrp::handleObjectRemoved(GDBusObjectManager* objectManager, GDBusObject* object, void* userData)
{
	Bluez5ProfileAvcrp* avrcp = static_cast<Bluez5ProfileAvcrp*>(userData);
	/* We are not unrefing the mPropertiesProxy and mPlayerInterface here.
	 * When new player gets added, object-added event is received first and
	 * then object-removed for the old one. Hence to avoid unrefing the reference
	 * to newly added player, unref the existing player reference whenever new player
	 * is added and get the reference to new player in handleObjectAdded function
	 */
	std::string objectPath = g_dbus_object_get_object_path(object);
	auto adapterPath = avrcp->mAdapter->getObjectPath();
	if (objectPath.compare(0, adapterPath.length(), adapterPath))
		return;
	auto mediaPlayerInterface = g_dbus_object_get_interface(object, "org.bluez.MediaPlayer1");
	if (mediaPlayerInterface)
	{
		avrcp->removeMediaPlayer(objectPath);
		g_object_unref(mediaPlayerInterface);
	}
}

void Bluez5ProfileAvcrp::disconnect(const std::string& address, BluetoothResultCallback callback)
{
	auto disConnectCallback = [this, address, callback](BluetoothError error) {
		if (error != BLUETOOTH_ERROR_NONE)
		{
			DEBUG("AVRCP disconnect failed");
			callback(error);
			return;
		}
		DEBUG("AVRCP disconnected succesfully");
		callback(BLUETOOTH_ERROR_NONE);
	};
	Bluez5ProfileBase::disconnect(address, disConnectCallback);
}

void Bluez5ProfileAvcrp::enable(const std::string &uuid, BluetoothResultCallback callback)
{
	DEBUG("enable: %s", uuid.c_str());
	mAdapter->notifyAvrcpRoleChange(uuid);
	if (BLUETOOTH_PROFILE_AVRCP_TARGET_UUID == uuid)
	{
		mUuid = BLUETOOTH_PROFILE_AVRCP_REMOTE_UUID;
	}
	else
	{
		mUuid = BLUETOOTH_PROFILE_AVRCP_TARGET_UUID;
	}
	callback(BLUETOOTH_ERROR_NONE);
}

void Bluez5ProfileAvcrp::disable(const std::string &uuid, BluetoothResultCallback callback)
{
	callback(BLUETOOTH_ERROR_UNSUPPORTED);
}

void Bluez5ProfileAvcrp::getProperties(const std::string &address, BluetoothPropertiesResultCallback  callback)
{
}

void Bluez5ProfileAvcrp::getProperty(const std::string &address, BluetoothProperty::Type type, BluetoothPropertyResultCallback callback)
{
	BluetoothProperty prop(type);
	Bluez5Device *device = mAdapter->findDevice(address);
	if (!device)
	{
		callback(BLUETOOTH_ERROR_PARAM_INVALID, prop);
		return;
	}

	bool connected = (device->getAddress() == mConnectedDeviceAddress);
	prop.setValue<bool>(connected);
	callback(BLUETOOTH_ERROR_NONE, prop);
}

void Bluez5ProfileAvcrp::supplyMediaMetaData(BluetoothAvrcpRequestId requestId, const BluetoothMediaMetaData &metaData, BluetoothResultCallback callback)
{
	mAdapter->getPlayer()->setMediaMetaData(metaData);
	callback(BLUETOOTH_ERROR_NONE);
}

void Bluez5ProfileAvcrp::supplyMediaPlayStatus(BluetoothAvrcpRequestId requestId, const BluetoothMediaPlayStatus &playStatus, BluetoothResultCallback callback)
{
	mAdapter->getPlayer()->setMediaPlayStatus(playStatus);
	if (playStatus.getStatus() == BluetoothMediaPlayStatus::MEDIA_PLAYSTATUS_STOPPED)
		mAdapter->getPlayer()->setMediaPosition(0);
	else
		mAdapter->getPlayer()->setMediaPosition(playStatus.getPosition());
	mAdapter->getPlayer()->setMediaDuration(playStatus.getDuration());
	callback(BLUETOOTH_ERROR_NONE);
}

void Bluez5ProfileAvcrp::mediaPlayStatusRequested(const std::string &address)
{
	getAvrcpObserver()->mediaPlayStatusRequested(generateMediaPlayStatusRequestId(),
		convertAddressToLowerCase(mAdapter->getAddress()), address);
}

void Bluez5ProfileAvcrp::mediaMetaDataRequested(const std::string &address)
{
	getAvrcpObserver()->mediaMetaDataRequested(generateMetaDataRequestId(),
		convertAddressToLowerCase(mAdapter->getAddress()), address);
}

void Bluez5ProfileAvcrp::updateConnectionStatus(const std::string &address, bool status, const std::string &uuid)
{
	DEBUG("AVRCP isConnected %d:%s, mConnected:%d", status, uuid.c_str(), mConnected);
	BluetoothPropertiesList properties;

	if (BLUETOOTH_PROFILE_AVRCP_TARGET_UUID == uuid)
		mConnectedController = status;
	else
		mConnectedTarget = status;
	if (status)
	{
		if (!mConnected)
		{
			DEBUG("AVRCP: Notifying upper layer avrcp connected");
			/* Inform upper layer if previous status is not connected and current status is
			 * at least one of the roles is connected. */
			mConnected = status;
			properties.push_back(BluetoothProperty(BluetoothProperty::Type::CONNECTED, status));
			getObserver()->propertiesChanged(convertAddressToLowerCase(mAdapter->getAddress()),
					convertAddressToLowerCase(address), properties);
			mConnectedDeviceAddress = address;
		}
	}
	else
	{
		if (mConnected && !mConnectedController && !mConnectedTarget)
		{
			mConnectedDeviceAddress = "";

			DEBUG("AVRCP: Notifying upper layer avrcp disconnected");
			/* Inform upper layer if previous status is connected and current status is
			   both the roles are not connected. */
			mConnected = status;
			properties.push_back(BluetoothProperty(BluetoothProperty::Type::CONNECTED, status));
			getObserver()->propertiesChanged(convertAddressToLowerCase(mAdapter->getAddress()),
					convertAddressToLowerCase(address), properties);
		}
	}

}

void Bluez5ProfileAvcrp::updateSupportedNotificationEvents(uint16_t notificationEvents, const std::string& address)
{
	DEBUG("notificationEvents: %x\n", notificationEvents);
	BluetoothAvrcpSupportedNotificationEventList eventsList;
	if (notificationEvents & (1 << EVENT_STATUS_CHANGED))
	{
		eventsList.push_back(EVENT_STATUS_CHANGED);
	}
	if (notificationEvents & (1 << EVENT_TRACK_CHANGED))
	{
		eventsList.push_back(EVENT_TRACK_CHANGED);
	}
	if (notificationEvents & (1 << EVENT_TRACK_REACHED_END))
	{
		eventsList.push_back(EVENT_TRACK_REACHED_END);
	}
	if (notificationEvents & (1 << EVENT_TRACK_REACHED_START))
	{
		eventsList.push_back(EVENT_TRACK_REACHED_START);
	}
	if (notificationEvents & (1 << EVENT_PLAYBACK_POS_CHANGED))
	{
		eventsList.push_back(EVENT_PLAYBACK_POS_CHANGED);
	}
	if (notificationEvents & (1 << EVENT_BATTERY_STATUS_CHANGED))
	{
		eventsList.push_back(EVENT_BATTERY_STATUS_CHANGED);
	}
	if (notificationEvents & (1 << EVENT_SYSTEM_STATUS_CHANGED))
	{
		eventsList.push_back(EVENT_SYSTEM_STATUS_CHANGED);
	}
	if (notificationEvents & (1 << EVENT_PLAYER_APPLICATION_SETTING_CHANGED))
	{
		eventsList.push_back(EVENT_PLAYER_APPLICATION_SETTING_CHANGED);
	}
	if (notificationEvents & (1 << EVENT_NOW_PLAYING_CHANGED))
	{
		eventsList.push_back(EVENT_NOW_PLAYING_CHANGED);
	}
	if (notificationEvents & (1 << EVENT_AVAILABLE_PLAYERS_CHANGED))
	{
		eventsList.push_back(EVENT_AVAILABLE_PLAYERS_CHANGED);
	}
	if (notificationEvents & (1 << EVENT_ADDRESSED_PLAYERS_CHANGED))
	{
		eventsList.push_back(EVENT_ADDRESSED_PLAYERS_CHANGED);
	}
	if (notificationEvents & (1 << EVENT_UIDS_CHANGED))
	{
		eventsList.push_back(EVENT_UIDS_CHANGED);
	}
	if (notificationEvents & (1 << EVENT_VOLUME_CHANGED))
	{
		eventsList.push_back(EVENT_VOLUME_CHANGED);
	}
	getAvrcpObserver()->supportedNotificationEventsReceived(eventsList, convertAddressToLowerCase(mAdapter->getAddress()),
		convertAddressToLowerCase(address));
}

void Bluez5ProfileAvcrp::updateRemoteFeatures(uint8_t features, const std::string &role, const std::string &address)
{
	Bluez5Device *device = mAdapter->findDevice(address);
	if (!device)
	{
		return;
	}

	BluetoothAvrcpRemoteFeatures remoteFeatures = FEATURE_NONE;
	if (features & REMOTE_DEVICE_AVRCP_FEATURE_BROWSE)
	{
		remoteFeatures = FEATURE_BROWSE;
		if (mConnected && mConnectedDeviceAddress == address)
			getAvrcpObserver()->remoteFeaturesReceived(remoteFeatures,
				convertAddressToLowerCase(mAdapter->getAddress()),
				convertAddressToLowerCase(address), role);
	}
	if (features & REMOTE_DEVICE_AVRCP_FEATURE_ABSOLUTE_VOLUME)
	{
		remoteFeatures = FEATURE_ABSOLUTE_VOLUME;
		if (mConnected && mConnectedDeviceAddress == address)
			getAvrcpObserver()->remoteFeaturesReceived(remoteFeatures,
				convertAddressToLowerCase(mAdapter->getAddress()),
				convertAddressToLowerCase(address), role);

	}
	if (features & REMOTE_DEVICE_AVRCP_FEATURE_METADATA)
	{
		remoteFeatures = FEATURE_METADATA;
		if (mConnected && mConnectedDeviceAddress == address)
			getAvrcpObserver()->remoteFeaturesReceived(remoteFeatures,
				convertAddressToLowerCase(mAdapter->getAddress()),
				convertAddressToLowerCase(address), role);
	}
}

void Bluez5ProfileAvcrp::updateVolume(const std::string &address, int volume)
{
	DEBUG("updateVolume %d", volume);

	Bluez5Device *device = mAdapter->findDevice(address);
	if (!device)
	{
		DEBUG("Bluez5ProfileAvcrp::updateVolume not handled");
		return;
	}

	if (mConnected)
		getAvrcpObserver()->volumeChanged(volume, convertAddressToLowerCase(mAdapter->getAddress()), convertAddressToLowerCase(address));
}

void Bluez5ProfileAvcrp::recievePassThroughCommand(std::string address, std::string key, std::string state)
{
	DEBUG("Bluez5ProfileAvcrp::recievePassThroughCommand %s %s", key.c_str(), state.c_str());
	BluetoothAvrcpPassThroughKeyStatus keyStatus = KEY_STATUS_UNKNOWN;
	BluetoothAvrcpPassThroughKeyCode keyCode = KEY_CODE_UNKNOWN;

	if (state == "pressed")
		keyStatus = KEY_STATUS_PRESSED;
	else if (state == "released")
		keyStatus = KEY_STATUS_RELEASED;

	auto keyCodeIt = mKeyMap.find(key);
	if (keyCodeIt != mKeyMap.end())
		keyCode = keyCodeIt->second;

	Bluez5Device *device = mAdapter->findDevice(address);
	if (!device)
	{
		DEBUG("Bluez5ProfileAvcrp::recievePassThroughCommand not handled");
		return;
	}
	if (mConnectedTarget)
	{
		getAvrcpObserver()->passThroughCommandReceived(keyCode, keyStatus, convertAddressToLowerCase(mAdapter->getAddress()), convertAddressToLowerCase(address));

		//TODO: Remove setMediaPlayStatus() call from this code block, after
		//proper implementation in media application
		BluetoothMediaPlayStatus status;
		if (key == "PLAY")
		{
			status.setStatus(BluetoothMediaPlayStatus::MEDIA_PLAYSTATUS_PLAYING);
			mAdapter->getPlayer()->setMediaPlayStatus(status);
		}
		else if (key == "PAUSE")
		{
			status.setStatus(BluetoothMediaPlayStatus::MEDIA_PLAYSTATUS_PAUSED);
			mAdapter->getPlayer()->setMediaPlayStatus(status);
		}
		else if (key == "STOP")
		{
			status.setStatus(BluetoothMediaPlayStatus::MEDIA_PLAYSTATUS_STOPPED);
			mAdapter->getPlayer()->setMediaPlayStatus(status);
		}
	}
}

BluetoothError Bluez5ProfileAvcrp::setAbsoluteVolume(const std::string &address, int volume)
{
	Bluez5ProfileA2dp *a2dp = dynamic_cast<Bluez5ProfileA2dp*> (mAdapter->getProfile(BLUETOOTH_PROFILE_ID_A2DP));
	if (a2dp)
		bluez_media_transport1_set_volume(a2dp->getMediaTransport(), (uint8_t)volume);
	return BLUETOOTH_ERROR_NONE;
}

void Bluez5ProfileAvcrp::setPlayerApplicationSettingsProperties(
	const BluetoothPlayerApplicationSettingsPropertiesList &properties,
	BluetoothResultCallback callback)
{
	if (mConnectedController)
	{
		if (mAddressedMediaPlayer)
		{
			BluetoothError btError =
				mAddressedMediaPlayer->setPlayerApplicationSettingsProperties(properties);
			callback(btError);
		}
		else
		{
			ERROR(MSGID_AVRCP_PROFILE_ERROR, 0, "Addressed player is not there");
			callback(BLUETOOTH_ERROR_NOT_ALLOWED);
		}
	}
	else
	{
		ERROR(MSGID_AVRCP_PROFILE_ERROR, 0, "Not connected as controller");
		callback(BLUETOOTH_ERROR_NOT_ALLOWED);
	}
}

BluetoothError Bluez5ProfileAvcrp::sendPassThroughCommand(
	const std::string &address,
	BluetoothAvrcpPassThroughKeyCode keyCode,
	BluetoothAvrcpPassThroughKeyStatus keyStatus)
{
	DEBUG("AVRCP: sendPassThroughCommand");
	Bluez5Device* device = mAdapter->findDevice(address);
	if (!device)
	{
		DEBUG("AVRCP: device not found.");
		return BLUETOOTH_ERROR_UNKNOWN_DEVICE_ADDR;
	}
	/* process the command if avrcp is connected in CT role.  */
	if (mConnectedController)
	{
		if (mAddressedMediaPlayer)
		{
			return mAddressedMediaPlayer->sendPassThroughCommand(keyCode, keyStatus);
		}
		else
		{
			ERROR(MSGID_AVRCP_PROFILE_ERROR, 0, "Addressed player is not there");
			return BLUETOOTH_ERROR_NOT_ALLOWED;
		}
	}
	else
	{
		DEBUG("AVRCP: Not connected as controller");
		return BLUETOOTH_ERROR_NOT_ALLOWED;
	}
	return BLUETOOTH_ERROR_NONE;

}

const std::string Bluez5ProfileAvcrp::getDeviceObjPath()
{
	Bluez5Device *device = mAdapter->findDevice(mConnectedDeviceAddress);
	std::string deviceObjPath = "";
	if (device)
	{
		deviceObjPath = device->getObjectPath();
	}
	else
	{
		DEBUG("device does not exist");
	}
	return deviceObjPath;
}

void Bluez5ProfileAvcrp::addMediaPlayer(GDBusObject *object)
{
	std::string objectPath = g_dbus_object_get_object_path(object);
	Bluez5MediaPlayer *mediaPlayer = new Bluez5MediaPlayer(this, object);
	mMediaPlayerList.push_back(mediaPlayer);
	addressedPlayerChanged(objectPath);
	mediaPlayer->getAllProperties();
}

void Bluez5ProfileAvcrp::removeMediaPlayer(const std::string &playerPath)
{
	for (auto player = mMediaPlayerList.begin();
		 player != mMediaPlayerList.end(); ++player)
	{
		if ((*player)->getPlayerObjPath() == playerPath)
		{
			if ((*player)->getAddressed())
			{
				mAddressedMediaPlayer = nullptr;
			}
			auto playerToRemove = *player;
			mMediaPlayerList.erase(player);
			delete playerToRemove;
			break;
		}
	}
	updatePlayerInfo();
}

std::string Bluez5ProfileAvcrp::getAdapterAddress()
{
	 return mAdapter->getAddress();
}

void Bluez5ProfileAvcrp::addressedPlayerChanged(const std::string &playerPath)
{
	for (auto player = mMediaPlayerList.begin();
		 player != mMediaPlayerList.end(); ++player)
	{
		if ((*player)->getPlayerObjPath() == playerPath)
		{
			(*player)->setAddressed(true);
			mAddressedMediaPlayer = *player;
		}
		else
		{
			(*player)->setAddressed(false);
		}
	}

	updatePlayerInfo();
	getAvrcpObserver()->currentFolderReceived(
		"",
		convertAddressToLowerCase(mAdapter->getAddress()),
		convertAddressToLowerCase(mConnectedDeviceAddress));
}

void Bluez5ProfileAvcrp::updatePlayerInfo()
{
	bool changed = false;
	bool playerExists = false;

	BluetothPlayerInfoList playerInfoList;
	for (auto player : mMediaPlayerList)
	{
		playerInfoList.push_back(player->getPlayerInfo());
	}
	DEBUG("Calling observer API for playerInfo");
	getAvrcpObserver()->playerInfoReceived(
		playerInfoList,
		convertAddressToLowerCase(mAdapter->getAddress()),
		convertAddressToLowerCase(mConnectedDeviceAddress));
}

void Bluez5ProfileAvcrp::getNumberOfItems(BluetoothAvrcpBrowseTotalNumberOfItemsCallback callback)
{
	if (callback)
	{
		if (!mConnectedController || !mAddressedMediaPlayer)
		{
			ERROR(MSGID_AVRCP_PROFILE_ERROR, 0,
				  "Not connected as controller/addressed player not there");
			callback(BLUETOOTH_ERROR_NOT_ALLOWED, 0);
			return;
		}
		mAddressedMediaPlayer->getNumberOfItems(callback);
	}
	return;
}

void Bluez5ProfileAvcrp::getFolderItems(uint32_t startIndex, uint32_t endIndex,
										BluetoothAvrcpBrowseFolderItemsCallback callback)
{
	if (callback)
	{
		BluetoothFolderItemList itemList;

		if (!mConnectedController || !mAddressedMediaPlayer)
		{
			ERROR(MSGID_AVRCP_PROFILE_ERROR, 0,
				  "Not connected as controller/addressed player not there");
			callback(BLUETOOTH_ERROR_NOT_ALLOWED, itemList);
			return;
		}
		mAddressedMediaPlayer->getFolderItems(startIndex, endIndex, callback);
	}
	return;
}

BluetoothError Bluez5ProfileAvcrp::changePath(const std::string &itemPath)
{
	if (!mConnectedController || !mAddressedMediaPlayer)
	{
		ERROR(MSGID_AVRCP_PROFILE_ERROR, 0,
			  "Not connected as controller/addressed player not there");
		return BLUETOOTH_ERROR_NOT_ALLOWED;
	}

	return mAddressedMediaPlayer->changePath(itemPath);
}

BluetoothError Bluez5ProfileAvcrp::playItem(const std::string &itemPath)
{
	if (!mConnectedController || !mAddressedMediaPlayer)
	{
		ERROR(MSGID_AVRCP_PROFILE_ERROR, 0,
			  "Not connected as controller/addressed player not there");
		return BLUETOOTH_ERROR_NOT_ALLOWED;
	}

	return mAddressedMediaPlayer->playItem(itemPath);
}

BluetoothError Bluez5ProfileAvcrp::addToNowPlaying(const std::string &itemPath)
{
	if (!mConnectedController || !mAddressedMediaPlayer)
	{
		ERROR(MSGID_AVRCP_PROFILE_ERROR, 0,
			  "Not connected as controller/addressed player not there");
		return BLUETOOTH_ERROR_NOT_ALLOWED;
	}

	return mAddressedMediaPlayer->addToNowPlaying(itemPath);
}