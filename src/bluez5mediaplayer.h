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

#ifndef BLUEZ5MEDIAPLAYER_H
#define BLUEZ5MEDIAPLAYER_H

#include <bluetooth-sil-api.h>
#include <map>
#include <string>

extern "C" {
#include "freedesktop-interface.h"
#include "bluez-interface.h"
}

class Bluez5ProfileAvcrp;
class Bluez5MediaFolder;

typedef gboolean (* bluezSendPassThroughCommand)(
		BluezMediaPlayer1 *proxy,
		GCancellable *cancellable,
		GError **error);

class Bluez5MediaPlayer
{
public:
	static const std::map<BluetoothAvrcpPassThroughKeyCode, bluezSendPassThroughCommand> mPassThroughCmd;
	static const std::map<std::string, BluetoothMediaPlayStatus::MediaPlayStatus> mPlayStatus;

	Bluez5MediaPlayer(Bluez5ProfileAvcrp *avrcp, GDBusObject *object);
	~Bluez5MediaPlayer();

	/* AVRCP CT player control APIs */
	BluetoothError sendPassThroughCommand(BluetoothAvrcpPassThroughKeyCode keyCode,
			BluetoothAvrcpPassThroughKeyStatus keyStatus);
	BluetoothError setPlayerApplicationSettingsProperties(
			const BluetoothPlayerApplicationSettingsPropertiesList &properties);

	/* AVRCP CT browse APIs called from Bluez5ProfileAvcrp */
	void getNumberOfItems(BluetoothAvrcpBrowseTotalNumberOfItemsCallback callback);
	void getFolderItems(uint32_t startIndex, uint32_t endIndex,
						BluetoothAvrcpBrowseFolderItemsCallback callback);
	BluetoothError changePath(const std::string &itemPath);
	BluetoothError playItem(const std::string &itemPath);

	/* AVRCP CT helper functions called from AVRCP profile class */
	std::string getPlayerObjPath() const { return mPlayerInfo.getPath(); }
	bool getAddressed() const { return mPlayerInfo.getAddressed(); }
	BluetoothPlayerInfo getPlayerInfo() const { return mPlayerInfo; }
	void getAllProperties();
	void setAddressed(bool addressed) { mPlayerInfo.setAddressed(addressed); }

private:
	Bluez5ProfileAvcrp *mAvrcp;
	BluezMediaPlayer1 *mPlayerInterface;
	FreeDesktopDBusProperties *mPropertiesProxy;
	BluetoothMediaPlayStatus mMediaPlayStatus;
	BluetoothPlayerInfo mPlayerInfo;
	Bluez5MediaFolder *mMediaFolder;

	static void handlePropertiesChanged(BluezMediaPlayer1 *transportInterface,
			gchar *, GVariant *changedProperties, GVariant *invalidatedProperties,
			gpointer userData);
	static void handleInterfaceAdded(GDBusObject *object,
			GDBusInterface *interface, gpointer userData);
	static void handleInterfaceRemoved(GDBusObject *object,
			GDBusInterface *interface, gpointer userData);
	void mediaPlayerPropertiesChanged(GVariant *changedProperties);

	/* Player application settings helper functions */
	BluetoothPlayerApplicationSettingsRepeat repeatStringToEnum(std::string repeatVal);
	BluetoothPlayerApplicationSettingsShuffle shuffleStringToEnum(std::string shuffleVal);
	BluetoothPlayerApplicationSettingsScan scanStringToEnum(std::string scanVal);
	BluetoothPlayerApplicationSettingsEqualizer equalizerStringToEnum(std::string equalizerVal);
	std::string equalizerEnumToString(BluetoothPlayerApplicationSettingsEqualizer equalizer);
	std::string repeatEnumToString(BluetoothPlayerApplicationSettingsRepeat repeat);
	std::string shuffleEnumToString(BluetoothPlayerApplicationSettingsShuffle shuffle);
	std::string scanEnumToString(BluetoothPlayerApplicationSettingsScan scan);
	bool updatePlayerProperties();
	BluetoothAvrcpPlayerType playerTypeStringToEnum(const std::string type);
};

#endif //BLUEZ5MEDIAPLAYER_H
