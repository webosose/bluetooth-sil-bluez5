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

#include "bluez5mediafolder.h"
#include "bluez5profileavrcp.h"
#include "utils.h"
#include "logging.h"

Bluez5MediaFolder::Bluez5MediaFolder(Bluez5ProfileAvcrp *avrcp,
		const std::string &playerPath) :
	mAvrcp(avrcp),
	mFolderInterface(nullptr),
	mPropertiesProxy(nullptr)
{
	mPlayerObjPath = playerPath;
	GError *error = 0;
	DEBUG("Bluez5MediaFolder:: mPlayerObjPath: %s", mPlayerObjPath.c_str());
	mFolderInterface = bluez_media_folder1_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
			"org.bluez", mPlayerObjPath.c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_AVRCP_PROFILE_ERROR, 0, "Not able to get media folder interface: %s", error->message);
		g_error_free(error);
		return;
	}
	mPropertiesProxy = free_desktop_dbus_properties_proxy_new_for_bus_sync(
			G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, "org.bluez",
			mPlayerObjPath.c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "Not able to get property interface: %s", error->message);
		g_error_free(error);
		return;
	}

	g_signal_connect(G_OBJECT(mPropertiesProxy), "properties-changed",
			G_CALLBACK(handlePropertiesChanged), this);
	GVariant *propsVar;
	free_desktop_dbus_properties_call_get_all_sync(
			mPropertiesProxy,
			"org.bluez.MediaFolder1", &propsVar, NULL, NULL);
	mediaFolderPropertiesChanged(propsVar);
	g_variant_unref(propsVar);
}

Bluez5MediaFolder::~Bluez5MediaFolder()
{
	if (mFolderInterface)
	{
		g_object_unref(mFolderInterface);
		mFolderInterface = nullptr;
	}
	if (mPropertiesProxy)
	{
		g_object_unref(mPropertiesProxy);
		mPropertiesProxy = nullptr;
	}
}

void Bluez5MediaFolder::handlePropertiesChanged(
		BluezMediaFolder1 *folderInterface,
		gchar *interface, GVariant *changedProperties,
		GVariant *invalidatedProperties, gpointer userData)
{
	std::string interfaceName = interface;
	auto mediaFolder = static_cast<Bluez5MediaFolder *>(userData);

	if ("org.bluez.MediaFolder1" == interfaceName)
	{
		DEBUG("Bluez5MediaFolder::Media folder properties changed");
		mediaFolder->mediaFolderPropertiesChanged(changedProperties);
	}
}

void Bluez5MediaFolder::mediaFolderPropertiesChanged(GVariant* changedProperties)
{
	DEBUG("mediaFolderPropertiesChanged");
	for (int n = 0; n < g_variant_n_children(changedProperties); n++)
	{
		GVariant *propertyVar = g_variant_get_child_value(changedProperties, n);
		GVariant *keyVar = g_variant_get_child_value(propertyVar, 0);
		GVariant *valueVar = g_variant_get_child_value(propertyVar, 1);

		std::string key = g_variant_get_string(keyVar, NULL);
		if ("Name" == key)
		{
			std::string currentFolder = g_variant_get_string(g_variant_get_variant(valueVar), NULL);
			DEBUG("Bluez5MediaFolder:: CurrentFolder: %s", currentFolder.c_str());
			mAvrcp->getAvrcpObserver()->currentFolderReceived(
					currentFolder,
					convertAddressToLowerCase(mAvrcp->getAdapterAddress()),
					convertAddressToLowerCase(mAvrcp->getConnectedDeviceAddress()));
		}
		g_variant_unref(valueVar);
		g_variant_unref(keyVar);
		g_variant_unref(propertyVar);
	}
}

void Bluez5MediaFolder::getNumberOfItems(BluetoothAvrcpBrowseTotalNumberOfItemsCallback callback)
{
	GError *error = 0;
	GVariant *var;
	free_desktop_dbus_properties_call_get_sync(
		mPropertiesProxy,
		"org.bluez.MediaFolder1", "NumberOfItems",
		&var, NULL, &error);
	if (error)
	{
		ERROR(MSGID_PROFILE_MANAGER_ERROR, 0, "get numberOfItems failed: %s", error->message);
		g_error_free(error);
		callback(BLUETOOTH_ERROR_FAIL, 0);
		return;
	}

	GVariant *realPropVar = g_variant_get_variant(var);
	uint32_t numOfItems = g_variant_get_uint32(realPropVar);
	DEBUG("Bluez5MediaFolder: Number of items: %d", numOfItems);

	callback(BLUETOOTH_ERROR_NONE, numOfItems);

	return;
}
