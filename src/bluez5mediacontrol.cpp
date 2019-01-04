// Copyright (c) 2019 LG Electronics, Inc.
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

#include "bluez5mediacontrol.h"
#include "bluez5adapter.h"
#include "logging.h"
#include <iostream>
#include "bluez5profileavrcp.h"

Bluez5MediaControl::Bluez5MediaControl(Bluez5Adapter *adapter, const std::string &objectPath):
	mAdapter(adapter),
	mObjectPath(objectPath),
	mInterface(nullptr),
	mState(false)
{
	GError *error = 0;
	mInterface = bluez_media_control1_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
																				"org.bluez", mObjectPath.c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_MEDIA_CONTROL_ERROR, 0, "Failed to create dbus proxy for device on path %s: %s",
			  mObjectPath.c_str(), error->message);
		g_error_free(error);
		return;
	}

	mState = bluez_media_control1_get_connected(mInterface);

	FreeDesktopDBusProperties *propertiesProxy = free_desktop_dbus_properties_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
																		   "org.bluez", mObjectPath.c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_MEDIA_CONTROL_ERROR, 0, "Failed to create prop dbus proxy for device on path %s: %s",
			  objectPath.c_str(), error->message);
		g_error_free(error);
		return;
	}

	g_signal_connect(G_OBJECT(propertiesProxy), "properties-changed", G_CALLBACK(handlePropertiesChanged), this);

}

Bluez5MediaControl::~Bluez5MediaControl()
{
}

std::string Bluez5MediaControl::getObjectPath() const
{
	return mObjectPath;
}

bool Bluez5MediaControl::getConnectionStatus() const
{
	return mState;
}

bool Bluez5MediaControl::fastForwad()
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	return true;
}

bool Bluez5MediaControl::next()
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	return true;
}

bool Bluez5MediaControl::pause()
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	return true;
}

bool Bluez5MediaControl::play()
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	return true;
}

bool Bluez5MediaControl::previous()
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	return true;
}

bool Bluez5MediaControl::rewind()
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	return true;
}

bool Bluez5MediaControl::volumeDown()
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	return true;
}

bool Bluez5MediaControl::volumeUp()
{
	DEBUG("%s::%s",__FILE__,__FUNCTION__);
	return true;
}

void Bluez5MediaControl::handlePropertiesChanged(BluezMediaControl1 *controlInterface, gchar *interface,  GVariant *changedProperties,
												   GVariant *invalidatedProperties, gpointer userData)
{
	Bluez5MediaControl *pThis = static_cast<Bluez5MediaControl*>(userData);

	if (strcmp(interface, "org.bluez.MediaControl1") == 0)
	{
		for (int n = 0; n < g_variant_n_children(changedProperties); n++)
		{
			GVariant *propertyVar = g_variant_get_child_value(changedProperties, n);
			GVariant *keyVar = g_variant_get_child_value(propertyVar, 0);
			GVariant *valueVar = g_variant_get_child_value(propertyVar, 1);

			std::string key = g_variant_get_string(keyVar, NULL);

			if (key == "Connected")
			{
				bool isConnected = g_variant_get_boolean(g_variant_get_variant(valueVar));
				DEBUG("AVCRP State %d", isConnected);
				std::cout << "Media Control" << interface << ":" << isConnected <<std::endl;
				g_variant_unref(valueVar);
				g_variant_unref(keyVar);
				g_variant_unref(propertyVar);
				break;
			}

			g_variant_unref(valueVar);
			g_variant_unref(keyVar);
			g_variant_unref(propertyVar);
		}
	}
}
