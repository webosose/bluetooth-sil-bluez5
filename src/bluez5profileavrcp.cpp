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
const std::map<BluetoothAvrcpPassThroughKeyCode, bluezSendPassThroughCommand> Bluez5ProfileAvcrp::mPassThroughCmd = {
	{KEY_CODE_PLAY, &bluez_media_player1_call_play_sync},
	{KEY_CODE_STOP, &bluez_media_player1_call_stop_sync},
	{KEY_CODE_PAUSE, &bluez_media_player1_call_pause_sync},
	{KEY_CODE_NEXT, &bluez_media_player1_call_next_sync},
	{KEY_CODE_PREVIOUS, &bluez_media_player1_call_previous_sync},
	{KEY_CODE_REWIND, &bluez_media_player1_call_rewind_sync},
	{KEY_CODE_FAST_FORWARD, &bluez_media_player1_call_fast_forward_sync}
};

const std::map<std::string, BluetoothMediaPlayStatus::MediaPlayStatus> Bluez5ProfileAvcrp::mPlayStatus = {
	{"stopped", BluetoothMediaPlayStatus::MediaPlayStatus::MEDIA_PLAYSTATUS_STOPPED},
	{"playing", BluetoothMediaPlayStatus::MediaPlayStatus::MEDIA_PLAYSTATUS_PLAYING},
	{"paused", BluetoothMediaPlayStatus::MediaPlayStatus::MEDIA_PLAYSTATUS_PAUSED},
	{"forward-seek", BluetoothMediaPlayStatus::MediaPlayStatus::MEDIA_PLAYSTATUS_FWD_SEEK},
	{"reverse-seek", BluetoothMediaPlayStatus::MediaPlayStatus::MEDIA_PLAYSTATUS_REV_SEEK},
	{"error", BluetoothMediaPlayStatus::MediaPlayStatus::MEDIA_PLAYSTATUS_ERROR}
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
mPlayerInterface(nullptr),
mPropertiesProxy(nullptr)
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
	if (mPlayerInterface)
	{
		g_object_unref(mPlayerInterface);
		mPlayerInterface = nullptr;
	}
	if (mPropertiesProxy)
	{
		g_object_unref(mPropertiesProxy);
		mPropertiesProxy = nullptr;
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

		auto mediaPlayerInterface = g_dbus_object_get_interface(object, "org.bluez.MediaPlayer1");
		if (mediaPlayerInterface)
		{
			DEBUG("MediaPlayer interface");
			auto adapterPath = avrcp->mAdapter->getObjectPath();
			if (objectPath.compare(0, adapterPath.length(), adapterPath))
				return;

			avrcp->mPlayerInterface = bluez_media_player1_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
					"org.bluez", objectPath.c_str(), NULL, &error);
			if (error)
			{
				ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "Not able to get player interface");
				g_error_free(error);
				return;
			}

			avrcp->mPropertiesProxy = free_desktop_dbus_properties_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
					G_DBUS_PROXY_FLAGS_NONE, "org.bluez", objectPath.c_str(), NULL, &error);
			if (error)
			{
				ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "Not able to get property interface");
				g_error_free(error);
				return;
			}

			g_signal_connect(G_OBJECT(avrcp->mPropertiesProxy), "properties-changed", G_CALLBACK(handlePropertiesChanged), avrcp);

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
		/* Unref the existing player reference before getting reference to newly added player object */
		if (avrcp->mPlayerInterface)
		{
			g_object_unref(avrcp->mPlayerInterface);
			avrcp->mPlayerInterface = nullptr;
		}
		if (avrcp->mPropertiesProxy)
		{
			g_object_unref(avrcp->mPropertiesProxy);
			avrcp->mPropertiesProxy = nullptr;
		}

		GError* error = 0;
		avrcp->mPlayerInterface = bluez_media_player1_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
			"org.bluez", objectPath.c_str(), NULL, &error);
		if (error)
		{
			DEBUG("Not able to get media player interface");
			g_error_free(error);
			return;
		}

		avrcp->mPropertiesProxy = free_desktop_dbus_properties_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
			"org.bluez", objectPath.c_str(), NULL, &error);
		if (error)
		{
			DEBUG("Not able to get property interface");
			g_error_free(error);
			return;
		}

		g_signal_connect(G_OBJECT(avrcp->mPropertiesProxy), "properties-changed", G_CALLBACK(handlePropertiesChanged), avrcp);
		g_object_unref(mediaPlayerInterface);
	}
}

void Bluez5ProfileAvcrp::handleObjectRemoved(GDBusObjectManager* objectManager, GDBusObject* object, void* userData)
{
	/* We are not unrefing the mPropertiesProxy and mPlayerInterface here.
	 * When new player gets added, object-added event is received first and
	 * then object-removed for the old one. Hence to avoid unrefing the reference
	 * to newly added player, unref the existing player reference whenever new player
	 * is added and get the reference to new player in handleObjectAdded function
	 */
}

void Bluez5ProfileAvcrp::handlePropertiesChanged(BluezMediaPlayer1* transportInterface, gchar* interface, GVariant* changedProperties,
	GVariant* invalidatedProperties, gpointer userData)
{
	DEBUG("MediaPlayer1 properties changed");
	auto avrcp = static_cast<Bluez5ProfileAvcrp *>(userData);

	for (int n = 0; n < g_variant_n_children(changedProperties); n++)
	{
		GVariant* propertyVar = g_variant_get_child_value(changedProperties, n);
		GVariant* keyVar = g_variant_get_child_value(propertyVar, 0);
		GVariant* valueVar = g_variant_get_child_value(propertyVar, 1);

		std::string key = g_variant_get_string(keyVar, NULL);

		avrcp->parsePropertyFromVariant(key, g_variant_get_variant(valueVar));

		g_variant_unref(valueVar);
		g_variant_unref(keyVar);
		g_variant_unref(propertyVar);
	}
}

BluetoothPlayerApplicationSettingsRepeat Bluez5ProfileAvcrp::repeatStringToEnum(std::string repeat)
{
	if ("off" == repeat)
		return BluetoothPlayerApplicationSettingsRepeat::REPEAT_OFF;
	else if ("singletrack" == repeat)
		return BluetoothPlayerApplicationSettingsRepeat::REPEAT_SINGLE_TRACK;
	else if ("alltracks" == repeat)
		return BluetoothPlayerApplicationSettingsRepeat::REPEAT_ALL_TRACKS;
	else if ("group" == repeat)
		return BluetoothPlayerApplicationSettingsRepeat::REPEAT_GROUP;
	else
		return BluetoothPlayerApplicationSettingsRepeat::REPEAT_UNKNOWN;
}

BluetoothPlayerApplicationSettingsShuffle Bluez5ProfileAvcrp::shuffleStringToEnum(std::string shuffle)
{
	if ("off" == shuffle)
		return BluetoothPlayerApplicationSettingsShuffle::SHUFFLE_OFF;
	else if ("alltracks" == shuffle)
		return BluetoothPlayerApplicationSettingsShuffle::SHUFFLE_ALL_TRACKS;
	else if ("group" == shuffle)
		return BluetoothPlayerApplicationSettingsShuffle::SHUFFLE_GROUP;
	else
		return BluetoothPlayerApplicationSettingsShuffle::SHUFFLE_UNKNOWN;
}

BluetoothPlayerApplicationSettingsScan Bluez5ProfileAvcrp::scanStringToEnum(std::string scan)
{
	if ("off" == scan)
		return BluetoothPlayerApplicationSettingsScan::SCAN_OFF;
	else if ("alltracks" == scan)
		return BluetoothPlayerApplicationSettingsScan::SCAN_ALL_TRACKS;
	else if ("group" == scan)
		return BluetoothPlayerApplicationSettingsScan::SCAN_GROUP;
	else
		return BluetoothPlayerApplicationSettingsScan::SCAN_UNKNOWN;
}

BluetoothPlayerApplicationSettingsEqualizer Bluez5ProfileAvcrp::equalizerStringToEnum(std::string equalizer)
{
	if ("off" == equalizer)
		return BluetoothPlayerApplicationSettingsEqualizer::EQUALIZER_OFF;
	else if ("on" == equalizer)
		return BluetoothPlayerApplicationSettingsEqualizer::EQUALIZER_ON;
	else
		return BluetoothPlayerApplicationSettingsEqualizer::EQUALIZER_UNKNOWN;
}

void Bluez5ProfileAvcrp::parsePropertyFromVariant(const std::string& key, GVariant* valueVar)
{
	BluetoothPlayerApplicationSettingsPropertiesList applicationSettings;
	if ("Position" == key)
	{
		mMediaPlayStatus.setPosition(g_variant_get_uint32(valueVar));
		DEBUG("Position: %ld", mMediaPlayStatus.getPosition());
		if (!mConnectedDeviceAddress.empty())
		{
			getAvrcpObserver()->mediaPlayStatusReceived(mMediaPlayStatus,
					convertAddressToLowerCase(mAdapter->getAddress()),
					convertAddressToLowerCase(mConnectedDeviceAddress));
		}

	}
	else if ("Status" == key)
	{
		auto playStatusIt = mPlayStatus.find(g_variant_get_string(valueVar, NULL));
		if (playStatusIt != mPlayStatus.end())
		{
			mMediaPlayStatus.setStatus(playStatusIt->second);
		}
		DEBUG("Play status: %d", mMediaPlayStatus.getStatus());
		if (!mConnectedDeviceAddress.empty())
		{
			getAvrcpObserver()->mediaPlayStatusReceived(mMediaPlayStatus,
					convertAddressToLowerCase(mAdapter->getAddress()),
					convertAddressToLowerCase(mConnectedDeviceAddress));
		}
	}
	else if ("Track" == key)
	{
		GVariantIter* iter;
		g_variant_get(valueVar, "a{sv}", &iter);
		GVariant* valueTrack;
		gchar* keyVar;
		BluetoothMediaMetaData mediaMetadata;

		while (g_variant_iter_loop(iter, "{sv}", &keyVar, &valueTrack))
		{
			std::string keyTrack(keyVar);
			DEBUG("Key: %s", keyTrack.c_str());
			if ("Duration" == keyTrack)
			{
				mMediaPlayStatus.setDuration(g_variant_get_uint32(valueTrack));
				mediaMetadata.setDuration(mMediaPlayStatus.getDuration());
				DEBUG("Duration: %d", mMediaPlayStatus.getDuration());
				if (!mConnectedDeviceAddress.empty())
				{
					getAvrcpObserver()->mediaPlayStatusReceived(mMediaPlayStatus,
							convertAddressToLowerCase(mAdapter->getAddress()),
							convertAddressToLowerCase(mConnectedDeviceAddress));
				}
			}
			else if ("Title" == keyTrack)
			{
				mediaMetadata.setTitle(g_variant_get_string(valueTrack, NULL));
			}
			else if ("Album" == keyTrack)
			{
				mediaMetadata.setAlbum(g_variant_get_string(valueTrack, NULL));
			}
			else if ("Artist" == keyTrack)
			{
				mediaMetadata.setArtist(g_variant_get_string(valueTrack, NULL));
			}
			else if ("Genre" == keyTrack)
			{
				mediaMetadata.setGenre(g_variant_get_string(valueTrack, NULL));
			}
			else if ("NumberOfTracks" == keyTrack)
			{
				mediaMetadata.setTrackCount(g_variant_get_uint32(valueTrack));
			}
			else if ("TrackNumber" == keyTrack)
			{
				mediaMetadata.setTrackNumber(g_variant_get_uint32(valueTrack));
			}
		}
		if (!mConnectedDeviceAddress.empty())
		{
			getAvrcpObserver()->mediaDataReceived(mediaMetadata,
					convertAddressToLowerCase(mAdapter->getAddress()),
					convertAddressToLowerCase(mConnectedDeviceAddress));
		}
	}
	else if ("Equalizer" == key)
	{
		BluetoothPlayerApplicationSettingsEqualizer equalizer =
			equalizerStringToEnum(g_variant_get_string(valueVar, NULL));
		applicationSettings.push_back(BluetoothPlayerApplicationSettingsProperty(
			BluetoothPlayerApplicationSettingsProperty::Type::EQUALIZER, equalizer));
		if (!mConnectedDeviceAddress.empty())
		{
			getAvrcpObserver()->playerApplicationSettingsReceived(applicationSettings,
					convertAddressToLowerCase(mAdapter->getAddress()),
					convertAddressToLowerCase(mConnectedDeviceAddress));
		}
	}
	else if ("Repeat" == key)
	{
		BluetoothPlayerApplicationSettingsRepeat repeat =
			repeatStringToEnum(g_variant_get_string(valueVar, NULL));
		applicationSettings.push_back(BluetoothPlayerApplicationSettingsProperty(
			BluetoothPlayerApplicationSettingsProperty::Type::REPEAT, repeat));
		if (!mConnectedDeviceAddress.empty())
		{
			getAvrcpObserver()->playerApplicationSettingsReceived(applicationSettings,
					convertAddressToLowerCase(mAdapter->getAddress()),
					convertAddressToLowerCase(mConnectedDeviceAddress));
		}

	}
	else if ("Shuffle" == key)
	{
		BluetoothPlayerApplicationSettingsShuffle shuffle =
			shuffleStringToEnum(g_variant_get_string(valueVar, NULL));
		applicationSettings.push_back(BluetoothPlayerApplicationSettingsProperty(
			BluetoothPlayerApplicationSettingsProperty::Type::SHUFFLE, shuffle));
		if (!mConnectedDeviceAddress.empty())
		{
			getAvrcpObserver()->playerApplicationSettingsReceived(applicationSettings,
					convertAddressToLowerCase(mAdapter->getAddress()),
					convertAddressToLowerCase(mConnectedDeviceAddress));
		}
	}
	else if ("Scan" == key)
	{
		BluetoothPlayerApplicationSettingsScan scan =
			scanStringToEnum(g_variant_get_string(valueVar, NULL));
		applicationSettings.push_back(BluetoothPlayerApplicationSettingsProperty(
			BluetoothPlayerApplicationSettingsProperty::Type::SCAN, scan));
		if (!mConnectedDeviceAddress.empty())
		{
			getAvrcpObserver()->playerApplicationSettingsReceived(applicationSettings,
					convertAddressToLowerCase(mAdapter->getAddress()),
					convertAddressToLowerCase(mConnectedDeviceAddress));
		}
	}
	else
	{
		DEBUG("Key: %s", key.c_str());
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
	getAvrcpObserver()->mediaPlayStatusRequested(generateMediaPlayStatusRequestId(), address);
}

void Bluez5ProfileAvcrp::mediaMetaDataRequested(const std::string &address)
{
	getAvrcpObserver()->mediaMetaDataRequested(generateMetaDataRequestId(), address);
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

void Bluez5ProfileAvcrp::updateRemoteFeatures(uint8_t features, const std::string &role, const std::string &address)
{
	Bluez5Device *device = mAdapter->findDevice(address);
	if (!device)
	{
		return;
	}

	BluetoothAvrcpRemoteFeatures remoteFeatures = FEATURE_NONE;
	if(features & REMOTE_DEVICE_AVRCP_FEATURE_BROWSE)
		remoteFeatures = FEATURE_BROWSE;
	else if(features & REMOTE_DEVICE_AVRCP_FEATURE_ABSOLUTE_VOLUME)
		remoteFeatures = FEATURE_ABSOLUTE_VOLUME;
	else if(features & REMOTE_DEVICE_AVRCP_FEATURE_METADATA)
		remoteFeatures = FEATURE_METADATA;

	if (mConnected && mConnectedDeviceAddress == address)
		getAvrcpObserver()->remoteFeaturesReceived(remoteFeatures, convertAddressToLowerCase(mAdapter->getAddress()),convertAddressToLowerCase(address), role);
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

	if (mConnectedTarget)
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
		getAvrcpObserver()->passThroughCommandReceived(keyCode, keyStatus, convertAddressToLowerCase(mAdapter->getAddress()), convertAddressToLowerCase(address));
}

BluetoothError Bluez5ProfileAvcrp::setAbsoluteVolume(const std::string &address, int volume)
{
	Bluez5ProfileA2dp *a2dp = dynamic_cast<Bluez5ProfileA2dp*> (mAdapter->getProfile(BLUETOOTH_PROFILE_ID_A2DP));
	if (a2dp)
		bluez_media_transport1_set_volume(a2dp->getMediaTransport(), (uint8_t)volume);
	return BLUETOOTH_ERROR_NONE;
}

BluetoothError Bluez5ProfileAvcrp::sendPassThroughCommand(const std::string& address, BluetoothAvrcpPassThroughKeyCode keyCode,
	BluetoothAvrcpPassThroughKeyStatus keyStatus)
{
	DEBUG("AVRCP: sendPassThroughCommand");
	Bluez5Device* device = mAdapter->findDevice(address);
	if (!device)
	{
		DEBUG("AVRCP: device not found.");
		return BLUETOOTH_ERROR_UNKNOWN_DEVICE_ADDR;
	}
	GError* error = 0;
	/* process the command if avrcp is connected in CT role.  */
	if (mConnectedController)
	{
		auto passThroughCmdIt = mPassThroughCmd.find(keyCode);
		if (passThroughCmdIt != mPassThroughCmd.end())
		{
			passThroughCmdIt->second(mPlayerInterface, NULL, &error);
			if (error)
			{
				g_error_free(error);
				return BLUETOOTH_ERROR_FAIL;
			}
		}
		else
		{
			DEBUG("AVRCP: Keycode: %d, Unsupported");
			return BLUETOOTH_ERROR_UNSUPPORTED;
		}
	}
	else
	{
		DEBUG("AVRCP: Not connected as controller");
		return BLUETOOTH_ERROR_NOT_ALLOWED;
	}
	return BLUETOOTH_ERROR_NONE;

}
