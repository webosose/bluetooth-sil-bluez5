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

#include "bluez5mediaplayer.h"
#include "bluez5profileavrcp.h"
#include "bluez5mediafolder.h"
#include "logging.h"
#include <utils.h>

const std::map<BluetoothAvrcpPassThroughKeyCode, bluezSendPassThroughCommand> Bluez5MediaPlayer::mPassThroughCmd = {
	{KEY_CODE_PLAY, &bluez_media_player1_call_play_sync},
	{KEY_CODE_STOP, &bluez_media_player1_call_stop_sync},
	{KEY_CODE_PAUSE, &bluez_media_player1_call_pause_sync},
	{KEY_CODE_NEXT, &bluez_media_player1_call_next_sync},
	{KEY_CODE_PREVIOUS, &bluez_media_player1_call_previous_sync},
	{KEY_CODE_REWIND, &bluez_media_player1_call_rewind_sync},
	{KEY_CODE_FAST_FORWARD, &bluez_media_player1_call_fast_forward_sync}
};

const std::map<std::string, BluetoothMediaPlayStatus::MediaPlayStatus> Bluez5MediaPlayer::mPlayStatus = {
	{"stopped", BluetoothMediaPlayStatus::MediaPlayStatus::MEDIA_PLAYSTATUS_STOPPED},
	{"playing", BluetoothMediaPlayStatus::MediaPlayStatus::MEDIA_PLAYSTATUS_PLAYING},
	{"paused", BluetoothMediaPlayStatus::MediaPlayStatus::MEDIA_PLAYSTATUS_PAUSED},
	{"forward-seek", BluetoothMediaPlayStatus::MediaPlayStatus::MEDIA_PLAYSTATUS_FWD_SEEK},
	{"reverse-seek", BluetoothMediaPlayStatus::MediaPlayStatus::MEDIA_PLAYSTATUS_REV_SEEK},
	{"error", BluetoothMediaPlayStatus::MediaPlayStatus::MEDIA_PLAYSTATUS_ERROR}
};


Bluez5MediaPlayer::Bluez5MediaPlayer(Bluez5ProfileAvcrp *avrcp,
		GDBusObject* object) :
	mAvrcp(avrcp),
	mPlayerInterface(nullptr),
	mPropertiesProxy(nullptr),
	mMediaFolder(nullptr)
{
	mPlayerInfo.setPath(g_dbus_object_get_object_path(object));
	DEBUG("Bluez5MediaPlayer:: playrObjPath: %s", mPlayerInfo.getPath().c_str());
	GError *error = 0;
	mPlayerInterface = bluez_media_player1_proxy_new_for_bus_sync(
		G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, "org.bluez",
		mPlayerInfo.getPath().c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_AVRCP_PROFILE_ERROR, 0, "Not able to get player interface");
		g_error_free(error);
		return;
	}

	mPropertiesProxy = free_desktop_dbus_properties_proxy_new_for_bus_sync(
		G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, "org.bluez",
		mPlayerInfo.getPath().c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_AVRCP_PROFILE_ERROR, 0, "Not able to get property interface");
		g_error_free(error);
		return;
	}

	g_signal_connect(G_OBJECT(mPropertiesProxy), "properties-changed",
					 G_CALLBACK(handlePropertiesChanged), this);
	g_signal_connect(object, "interface-added",
					 G_CALLBACK(handleInterfaceAdded), this);
	g_signal_connect(object, "interface-removed",
					 G_CALLBACK(handleInterfaceRemoved), this);
}

Bluez5MediaPlayer::~Bluez5MediaPlayer()
{
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
	if (mMediaFolder)
	{
		delete mMediaFolder;
		mMediaFolder = nullptr;
	}
}

void Bluez5MediaPlayer::getAllProperties()
{
	GVariant *propsVar;
	free_desktop_dbus_properties_call_get_all_sync(
		mPropertiesProxy,
		"org.bluez.MediaPlayer1", &propsVar, NULL, NULL);
	mediaPlayerPropertiesChanged(propsVar);
	g_variant_unref(propsVar);
}

void Bluez5MediaPlayer::handlePropertiesChanged(
	BluezMediaPlayer1 *playerInterface,
	gchar *interface, GVariant *changedProperties,
	GVariant *invalidatedProperties, gpointer userData)
{
	std::string interfaceName = interface;
	auto mediaPlayer = static_cast<Bluez5MediaPlayer *>(userData);

	mediaPlayer->mediaPlayerPropertiesChanged(changedProperties);
}

void Bluez5MediaPlayer::handleInterfaceAdded(
	GDBusObject *object, GDBusInterface *interface, gpointer userData)
{
	DEBUG("Bluez5MediaPlayer:: handleInterfaceAdded");
	Bluez5MediaPlayer *mediaPlayer = static_cast<Bluez5MediaPlayer *>(userData);
	std::string objectPath = g_dbus_object_get_object_path(object);

	auto mediaFolderInterface = g_dbus_object_get_interface(object, "org.bluez.MediaFolder1");
	if (mediaFolderInterface)
	{
		DEBUG("MediaFolder interface added");
		mediaPlayer->mMediaFolder = new Bluez5MediaFolder(mediaPlayer->mAvrcp, objectPath);
	}
}

void Bluez5MediaPlayer::handleInterfaceRemoved(
	GDBusObject *object, GDBusInterface *interface, gpointer userData)
{
	DEBUG("Bluez5MediaPlayer:: handleInterfaceRemoved");
	Bluez5MediaPlayer *mediaPlayer = static_cast<Bluez5MediaPlayer *>(userData);
	std::string objectPath = g_dbus_object_get_object_path(object);

	auto mediaFolderInterface = g_dbus_object_get_interface(object, "org.bluez.MediaFolder1");
	if (mediaFolderInterface)
	{
		DEBUG("Bluez5MediaPlayer:: Deleting MediaFolder");
		delete mediaPlayer->mMediaFolder;
		mediaPlayer->mMediaFolder = nullptr;
	}
}

void Bluez5MediaPlayer::mediaPlayerPropertiesChanged(
	GVariant *changedProperties)
{
	BluetoothPlayerApplicationSettingsPropertiesList applicationSettings;
	for (int n = 0; n < g_variant_n_children(changedProperties); n++)
	{
		GVariant *propertyVar = g_variant_get_child_value(changedProperties, n);
		GVariant *keyVar = g_variant_get_child_value(propertyVar, 0);
		GVariant *valueVariant = g_variant_get_child_value(propertyVar, 1);
		GVariant *valueVar = g_variant_get_variant(valueVariant);

		std::string key = g_variant_get_string(keyVar, NULL);
		if ("Position" == key)
		{
			uint32_t position = g_variant_get_uint32(valueVar);
			if (mMediaPlayStatus.getPosition() != position)
			{
				mMediaPlayStatus.setPosition(position);
				DEBUG("Bluez5MediaPlayer::Position: %ld", mMediaPlayStatus.getPosition());
				if (!mAvrcp->getConnectedDeviceAddress().empty())
				{
					mAvrcp->getAvrcpObserver()->mediaPlayStatusReceived(
						mMediaPlayStatus,
						convertAddressToLowerCase(mAvrcp->getAdapterAddress()),
						convertAddressToLowerCase(mAvrcp->getConnectedDeviceAddress()));
				}
			}
		}
		else if ("Status" == key)
		{
			auto playStatusIt = mPlayStatus.find(g_variant_get_string(valueVar, NULL));
			if (playStatusIt != mPlayStatus.end())
			{
				if (playStatusIt->second != mMediaPlayStatus.getStatus())
				{
					mMediaPlayStatus.setStatus(playStatusIt->second);
					if (!mAvrcp->getConnectedDeviceAddress().empty())
					{
						mAvrcp->getAvrcpObserver()->mediaPlayStatusReceived(
							mMediaPlayStatus,
							convertAddressToLowerCase(mAvrcp->getAdapterAddress()),
							convertAddressToLowerCase(mAvrcp->getConnectedDeviceAddress()));
					}
				}
				DEBUG("Bluez5MediaPlayer::Play status: %d", mMediaPlayStatus.getStatus());
			}
		}
		else if ("Track" == key)
		{
			GVariantIter *iter;
			g_variant_get(valueVar, "a{sv}", &iter);
			GVariant *valueTrack;
			gchar *keyVar;
			BluetoothMediaMetaData mediaMetadata;

			while (g_variant_iter_loop(iter, "{sv}", &keyVar, &valueTrack))
			{
				std::string keyTrack(keyVar);
				DEBUG("Bluez5MediaPlayer:: Track Key: %s", keyTrack.c_str());
				if ("Duration" == keyTrack)
				{
					uint32_t duration = g_variant_get_uint32(valueTrack);
					if (mMediaPlayStatus.getDuration() != duration)
					{
						mMediaPlayStatus.setDuration(duration);
						if (!mAvrcp->getConnectedDeviceAddress().empty())
						{
							mAvrcp->getAvrcpObserver()->mediaPlayStatusReceived(
								mMediaPlayStatus,
								convertAddressToLowerCase(mAvrcp->getAdapterAddress()),
								convertAddressToLowerCase(mAvrcp->getConnectedDeviceAddress()));
						}
					}
					mediaMetadata.setDuration(duration);
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
			if (!mAvrcp->getConnectedDeviceAddress().empty())
			{
				mAvrcp->getAvrcpObserver()->mediaDataReceived(
					mediaMetadata,
					convertAddressToLowerCase(mAvrcp->getAdapterAddress()),
					convertAddressToLowerCase(mAvrcp->getConnectedDeviceAddress()));
			}
		}
		else if ("Equalizer" == key)
		{
			BluetoothPlayerApplicationSettingsEqualizer equalizer =
				equalizerStringToEnum(g_variant_get_string(valueVar, NULL));
			applicationSettings.push_back(BluetoothPlayerApplicationSettingsProperty(
				BluetoothPlayerApplicationSettingsProperty::Type::EQUALIZER, equalizer));
			if (!mAvrcp->getConnectedDeviceAddress().empty())
			{
				mAvrcp->getAvrcpObserver()->playerApplicationSettingsReceived(
					applicationSettings,
					convertAddressToLowerCase(mAvrcp->getAdapterAddress()),
					convertAddressToLowerCase(mAvrcp->getConnectedDeviceAddress()));
			}
		}
		else if ("Repeat" == key)
		{
			BluetoothPlayerApplicationSettingsRepeat repeat =
				repeatStringToEnum(g_variant_get_string(valueVar, NULL));
			applicationSettings.push_back(BluetoothPlayerApplicationSettingsProperty(
				BluetoothPlayerApplicationSettingsProperty::Type::REPEAT, repeat));
			if (!mAvrcp->getConnectedDeviceAddress().empty())
			{
				mAvrcp->getAvrcpObserver()->playerApplicationSettingsReceived(
					applicationSettings,
					convertAddressToLowerCase(mAvrcp->getAdapterAddress()),
					convertAddressToLowerCase(mAvrcp->getConnectedDeviceAddress()));
			}
		}
		else if ("Shuffle" == key)
		{
			BluetoothPlayerApplicationSettingsShuffle shuffle =
				shuffleStringToEnum(g_variant_get_string(valueVar, NULL));
			applicationSettings.push_back(BluetoothPlayerApplicationSettingsProperty(
				BluetoothPlayerApplicationSettingsProperty::Type::SHUFFLE, shuffle));
			if (!mAvrcp->getConnectedDeviceAddress().empty())
			{
				mAvrcp->getAvrcpObserver()->playerApplicationSettingsReceived(
					applicationSettings,
					convertAddressToLowerCase(mAvrcp->getAdapterAddress()),
					convertAddressToLowerCase(mAvrcp->getConnectedDeviceAddress()));
			}
		}
		else if ("Scan" == key)
		{
			BluetoothPlayerApplicationSettingsScan scan =
				scanStringToEnum(g_variant_get_string(valueVar, NULL));
			applicationSettings.push_back(BluetoothPlayerApplicationSettingsProperty(
				BluetoothPlayerApplicationSettingsProperty::Type::SCAN, scan));
			if (!mAvrcp->getConnectedDeviceAddress().empty())
			{
				mAvrcp->getAvrcpObserver()->playerApplicationSettingsReceived(
					applicationSettings,
					convertAddressToLowerCase(mAvrcp->getAdapterAddress()),
					convertAddressToLowerCase(mAvrcp->getConnectedDeviceAddress()));
			}
		}
		else if ("Name" == key || "Type" == key || "Browsable" == key ||
				 "Searchable" == key || "Playlist" == key)
		{
			DEBUG("updatePlayerProperties for: %s", key.c_str());
			bool changed = updatePlayerProperties();
			if (changed)
			{
				DEBUG("Updating player info");
				mAvrcp->updatePlayerInfo();
			}
		}
		else
		{
			DEBUG("Bluez5MediaPlayer::Key: %s", key.c_str());
		}
	}
}

BluetoothError Bluez5MediaPlayer::sendPassThroughCommand(
	BluetoothAvrcpPassThroughKeyCode keyCode,
	BluetoothAvrcpPassThroughKeyStatus keyStatus)
{
	DEBUG("Bluez5MediaPlayer: sendPassThroughCommand");
	GError *error = 0;
	/* process the command if avrcp is connected in CT role.  */

	auto passThroughCmdIt = mPassThroughCmd.find(keyCode);
	if (passThroughCmdIt != mPassThroughCmd.end())
	{
		passThroughCmdIt->second(mPlayerInterface, NULL, &error);
		if (error)
		{
			ERROR(MSGID_AVRCP_PROFILE_ERROR, 0, "PassThrough Cmd failed. Error: %s", error->message);
			g_error_free(error);
			return BLUETOOTH_ERROR_FAIL;
		}
	}
	else
	{
		ERROR(MSGID_AVRCP_PROFILE_ERROR, 0, "AVRCP: Keycode unsupported");
		return BLUETOOTH_ERROR_UNSUPPORTED;
	}

	return BLUETOOTH_ERROR_NONE;
}

std::string Bluez5MediaPlayer::equalizerEnumToString(
	BluetoothPlayerApplicationSettingsEqualizer equalizer)
{
	if (BluetoothPlayerApplicationSettingsEqualizer::EQUALIZER_OFF == equalizer)
		return "off";
	else if (BluetoothPlayerApplicationSettingsEqualizer::EQUALIZER_ON == equalizer)
		return "on";
	else
		return "unknown";
}

std::string Bluez5MediaPlayer::repeatEnumToString(
	BluetoothPlayerApplicationSettingsRepeat repeat)
{
	if (BluetoothPlayerApplicationSettingsRepeat::REPEAT_OFF == repeat)
		return "off";
	else if (BluetoothPlayerApplicationSettingsRepeat::REPEAT_SINGLE_TRACK == repeat)
		return "singletrack";
	else if (BluetoothPlayerApplicationSettingsRepeat::REPEAT_ALL_TRACKS == repeat)
		return "alltracks";
	else if (BluetoothPlayerApplicationSettingsRepeat::REPEAT_GROUP == repeat)
		return "group";
	else
		return "unknown";
}

std::string Bluez5MediaPlayer::shuffleEnumToString(
	BluetoothPlayerApplicationSettingsShuffle shuffle)
{
	if (BluetoothPlayerApplicationSettingsShuffle::SHUFFLE_OFF == shuffle)
		return "off";
	else if (BluetoothPlayerApplicationSettingsShuffle::SHUFFLE_ALL_TRACKS == shuffle)
		return "alltracks";
	else if (BluetoothPlayerApplicationSettingsShuffle::SHUFFLE_GROUP == shuffle)
		return "group";
	else
		return "unknown";
}

std::string Bluez5MediaPlayer::scanEnumToString(
	BluetoothPlayerApplicationSettingsScan scan)
{
	if (BluetoothPlayerApplicationSettingsScan::SCAN_OFF == scan)
		return "off";
	else if (BluetoothPlayerApplicationSettingsScan::SCAN_ALL_TRACKS == scan)
		return "alltracks";
	else if (BluetoothPlayerApplicationSettingsScan::SCAN_GROUP == scan)
		return "group";
	else
		return "unknown";
}

BluetoothPlayerApplicationSettingsRepeat Bluez5MediaPlayer::repeatStringToEnum(
	std::string repeat)
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

BluetoothPlayerApplicationSettingsShuffle Bluez5MediaPlayer::shuffleStringToEnum(
	std::string shuffle)
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

BluetoothPlayerApplicationSettingsScan Bluez5MediaPlayer::scanStringToEnum(
	std::string scan)
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

BluetoothPlayerApplicationSettingsEqualizer Bluez5MediaPlayer::equalizerStringToEnum(
	std::string equalizer)
{
	if ("off" == equalizer)
		return BluetoothPlayerApplicationSettingsEqualizer::EQUALIZER_OFF;
	else if ("on" == equalizer)
		return BluetoothPlayerApplicationSettingsEqualizer::EQUALIZER_ON;
	else
		return BluetoothPlayerApplicationSettingsEqualizer::EQUALIZER_UNKNOWN;
}

BluetoothError Bluez5MediaPlayer::setPlayerApplicationSettingsProperties(
	const BluetoothPlayerApplicationSettingsPropertiesList &properties)
{
	GError *error = 0;
	std::string property;
	std::string value;
	for (auto prop : properties)
	{
		switch (prop.getType())
		{
			case BluetoothPlayerApplicationSettingsProperty::Type::EQUALIZER:
				{
					property = "Equalizer";
					value = equalizerEnumToString(
						prop.getValue<BluetoothPlayerApplicationSettingsEqualizer>());
					break;
				}
			case BluetoothPlayerApplicationSettingsProperty::Type::REPEAT:
				{
					property = "Repeat";
					value = repeatEnumToString(
						prop.getValue<BluetoothPlayerApplicationSettingsRepeat>());
					break;
				}
			case BluetoothPlayerApplicationSettingsProperty::Type::SHUFFLE:
				{
					property = "Shuffle";
					value = shuffleEnumToString(
						prop.getValue<BluetoothPlayerApplicationSettingsShuffle>());
					break;
				}
			case BluetoothPlayerApplicationSettingsProperty::Type::SCAN:
				{
					property = "Scan";
					value = scanEnumToString(
						prop.getValue<BluetoothPlayerApplicationSettingsScan>());
					break;
				}
		}
		GVariant *var = g_variant_new_string(value.c_str());
		free_desktop_dbus_properties_call_set_sync(mPropertiesProxy,
				"org.bluez.MediaPlayer1", property.c_str(),
				g_variant_new_variant(var), NULL, &error);
		if (error)
		{
			DEBUG ("%s: error is %s for prop: %s, value: %s",
					__func__, error->message, property.c_str(), value.c_str());
			g_error_free(error);
			return BLUETOOTH_ERROR_FAIL;
		}
	}
	return BLUETOOTH_ERROR_NONE;
}

bool Bluez5MediaPlayer::updatePlayerProperties()
{
	bool changed = false;
	GError *error = 0;

	if (mPropertiesProxy)
	{
		DEBUG("Getting the player properties");
		GVariant *changedProperties;
		free_desktop_dbus_properties_call_get_all_sync(
			mPropertiesProxy,
			"org.bluez.MediaPlayer1", &changedProperties, NULL, NULL);
		for (int n = 0; n < g_variant_n_children(changedProperties); n++)
		{
			GVariant *propertyVar = g_variant_get_child_value(changedProperties, n);
			GVariant *keyVar = g_variant_get_child_value(propertyVar, 0);
			GVariant *propVar = g_variant_get_child_value(propertyVar, 1);

			std::string key = g_variant_get_string(keyVar, NULL);
			if ("Name" == key)
			{
				std::string value = g_variant_get_string(
					g_variant_get_variant(propVar), NULL);
				if (mPlayerInfo.getName() != value)
				{
					mPlayerInfo.setName(value);
					changed = true;
					DEBUG("Name: %s", value.c_str());
				}
			}
			else if ("Type" == key)
			{
				std::string value = g_variant_get_string(
					g_variant_get_variant(propVar), NULL);
				BluetoothAvrcpPlayerType type = playerTypeStringToEnum(value);
				if (mPlayerInfo.getType() != type)
				{
					mPlayerInfo.setType(type);
					changed = true;
					DEBUG("type: %s", value.c_str());
				}
			}
			else if ("Playlist" == key)
			{
				std::string value = g_variant_get_string(
					g_variant_get_variant(propVar), NULL);

				size_t pos = value.find("player");
				if (pos != std::string::npos)
				{
					value.erase(0, pos);
				}
				if (mPlayerInfo.getPlayListPath() != value)
				{

					mPlayerInfo.setPlayListPath(value);
					changed = true;
					DEBUG("playlist path: %s", value.c_str());
				}
			}
			else if ("Browsable" == key)
			{
				bool browsable = g_variant_get_boolean(
					g_variant_get_variant(propVar));
				if (mPlayerInfo.getBrowsable() != browsable)
				{
					mPlayerInfo.setBrowsable(browsable);
					changed = true;
					DEBUG("Browsable: %d", browsable);
				}
			}
			else if ("Searchable" == key)
			{
				bool searchable = g_variant_get_boolean(
					g_variant_get_variant(propVar));
				if (mPlayerInfo.getSearchable() != searchable)
				{
					mPlayerInfo.setSearchable(searchable);
					changed = true;
					DEBUG("searchable: %s", searchable);
				}
			}
		}
		g_variant_unref(changedProperties);
	}

	return changed;
}

BluetoothAvrcpPlayerType Bluez5MediaPlayer::playerTypeStringToEnum(const std::string type)
{
	if ("Audio" == type)
		return BluetoothAvrcpPlayerType::PLAYER_TYPE_AUDIO;
	if ("Video" == type)
		return BluetoothAvrcpPlayerType::PLAYER_TYPE_VIDEO;
	if ("Audio Broadcasting" == type)
		return BluetoothAvrcpPlayerType::PLAYER_TYPE_AUDIO_BROADCAST;
	if ("Video Broadcasting" == type)
		return BluetoothAvrcpPlayerType::PLAYER_TYPE_VIDEO_BROADCAST;
	return BluetoothAvrcpPlayerType::PLAYER_TYPE_AUDIO;
}

void Bluez5MediaPlayer::getNumberOfItems(BluetoothAvrcpBrowseTotalNumberOfItemsCallback callback)
{
	if (!mMediaFolder)
	{
		ERROR(MSGID_AVRCP_PROFILE_ERROR, 0,
			  "MediaFolder interface is not created. Browsing not supported");
		callback(BLUETOOTH_ERROR_NOT_ALLOWED, 0);
		return;
	}

	mMediaFolder->getNumberOfItems(callback);
}

void Bluez5MediaPlayer::getFolderItems(uint32_t startIndex, uint32_t endIndex,
									   BluetoothAvrcpBrowseFolderItemsCallback callback)
{
	BluetoothFolderItemList itemList;
	if (!mMediaFolder)
	{
		ERROR(MSGID_AVRCP_PROFILE_ERROR, 0,
			  "MediaFolder interface is not created. Browsing not supported");
		callback(BLUETOOTH_ERROR_NOT_ALLOWED, itemList);
		return;
	}

	mMediaFolder->getFolderItems(startIndex, endIndex, callback);
}

BluetoothError Bluez5MediaPlayer::changePath(const std::string &itemPath)
{
	if (!mMediaFolder)
	{
		ERROR(MSGID_AVRCP_PROFILE_ERROR, 0,
			  "MediaFolder interface is not created. Browsing not supported");
		return BLUETOOTH_ERROR_NOT_ALLOWED;
	}

	std::string playerPath = mPlayerInfo.getPath();
	size_t pos = playerPath.find("player");
	if (pos != std::string::npos)
	{
		playerPath.erase(pos, strlen("player") + 1);
	}
	std::string finalItemPath = playerPath + itemPath;
	DEBUG("ItemPath : %s", finalItemPath.c_str());
	return mMediaFolder->changePath(finalItemPath);
}

BluetoothError Bluez5MediaPlayer::playItem(const std::string &itemPath)
{
	if (!mMediaFolder)
	{
		ERROR(MSGID_AVRCP_PROFILE_ERROR, 0,
			  "MediaFolder interface is not created. Browsing not supported");
		return BLUETOOTH_ERROR_NOT_ALLOWED;
	}

	std::string playerPath = mPlayerInfo.getPath();
	size_t pos = playerPath.find("player");
	if (pos != std::string::npos)
	{
		playerPath.erase(pos, strlen("player") + 1);
	}
	std::string finalItemPath = playerPath + itemPath;
	DEBUG("ItemPath : %s", finalItemPath.c_str());
	return mMediaFolder->playItem(finalItemPath);
}