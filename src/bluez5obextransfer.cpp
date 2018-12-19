// Copyright (c) 2014-2018 LG Electronics, Inc.
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

#include <bluetooth-sil-api.h>

#include "bluez5obextransfer.h"
#include "bluez5obexsession.h"
#include "logging.h"
#include "asyncutils.h"
#include "bluez5busconfig.h"

Bluez5ObexTransfer::Bluez5ObexTransfer(const std::string &objectPath, TransferType type) :
	mObjectPath(objectPath),
	mTransferProxy(0),
	mPropertiesProxy(0),
	mBytesTransferred(0),
	mFileSize(0),
	mState(INACTIVE),
	mWatchCallback(nullptr),
	mTransferType(type)
{
	GError *error = 0;

	mTransferProxy = bluez_obex_transfer1_proxy_new_for_bus_sync(BLUEZ5_OBEX_DBUS_BUS_TYPE, G_DBUS_PROXY_FLAGS_NONE,
														"org.bluez.obex", mObjectPath.c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_FAILED_TO_CREATE_OBEX_TRANSFER_PROXY, 0,
			  "Failed to create dbus proxy for obex transfer on path %s",
			  mObjectPath.c_str());
		g_error_free(error);
		return;
	}

	mPropertiesProxy = free_desktop_dbus_properties_proxy_new_for_bus_sync(BLUEZ5_OBEX_DBUS_BUS_TYPE, G_DBUS_PROXY_FLAGS_NONE,
																		   "org.bluez.obex", objectPath.c_str(), NULL, &error);
	if (error)
	{
		ERROR(MSGID_FAILED_TO_CREATE_OBEX_TRANSFER_PROXY, 0, "Failed to create dbus proxy for obex transfer on path %s: %s",
			  objectPath.c_str(), error->message);
		g_error_free(error);
		return;
	}

	g_signal_connect(G_OBJECT(mPropertiesProxy), "properties-changed", G_CALLBACK(handlePropertiesChanged), this);

	auto fetchPropertiesCallback = [this](GAsyncResult *result) {
		GError *error = 0;
		gboolean ret;
		GVariant *properties = 0;

		ret = free_desktop_dbus_properties_call_get_all_finish(mPropertiesProxy, &properties, result, &error);
		if (error)
		{
			g_error_free(error);
			return;
		}

		updateFromProperties(properties);

		g_variant_unref(properties);
	};

	free_desktop_dbus_properties_call_get_all(mPropertiesProxy, "org.bluez.obex.Transfer1", NULL,
											  glibAsyncMethodWrapper, new GlibAsyncFunctionWrapper(fetchPropertiesCallback));

}

Bluez5ObexTransfer::~Bluez5ObexTransfer()
{
	// If we didn't completed our transfer yet and no error has occured we
	// have to notify our watcher that the transfer was aborted.
	if (mState != COMPLETE && mState != ERROR)
	{
		mState = ERROR;
		if (mWatchCallback)
			mWatchCallback();
	}

	g_object_unref(mTransferProxy);
	g_object_unref(mPropertiesProxy);
}

void Bluez5ObexTransfer::watch(Bluez5ObexTransferWatchCallback callback)
{
	mWatchCallback = callback;
}

void Bluez5ObexTransfer::notifyWatcherAboutChangedProperties()
{
	if (mWatchCallback)
		mWatchCallback();
}

bool Bluez5ObexTransfer::parsePropertyFromVariant(const std::string &key, GVariant *valueVar)
{
	bool changed = false;

	if (key == "Transferred")
	{
		mBytesTransferred = g_variant_get_uint64(valueVar);
		changed = true;
	}
	else if (key == "Size")
	{
		mFileSize = g_variant_get_uint64(valueVar);
	}
	else if (key == "Status")
	{
		std::string state = g_variant_get_string(valueVar, NULL);

		if (state == "queued")
			mState = QUEUED;
		else if (state == "active")
			mState = ACTIVE;
		else if (state == "suspended")
			mState = SUSPENDED;
		else if (state == "complete")
			mState = COMPLETE;
		else if (state == "error")
			mState = ERROR;

		changed = true;
	}
	else if (key == "Name")
	{
		mFileName = g_variant_get_string(valueVar, NULL);
	}
	else if(key == "Filename")
	{
		mFilePath = g_variant_get_string(valueVar, NULL);
	}

	if (mState == COMPLETE)
	{
		if (mTransferType == SENDING)
		{
		// When we're done with the transfer and the OBEX daemon has sent the
		// complete state property change signal it might happen for small files
		// that there is not increase for the number of bytes transferred. For
		// that case we set the number of bytes transferred here to the size of
		// transferred file to do the right on our abstraction level.
			if (mBytesTransferred != mFileSize)
				mBytesTransferred = mFileSize;
		}
	}

	return changed;
}

void Bluez5ObexTransfer::updateFromProperties(GVariant *properties)
{
	bool changed = false;

	for (int n = 0; n < g_variant_n_children(properties); n++)
	{
		GVariant *propertyVar = g_variant_get_child_value(properties, n);
		GVariant *keyVar = g_variant_get_child_value(propertyVar, 0);
		GVariant *valueVar = g_variant_get_child_value(propertyVar, 1);

		std::string key = g_variant_get_string(keyVar, NULL);

		changed |= parsePropertyFromVariant(key, g_variant_get_variant(valueVar));

		g_variant_unref(valueVar);
		g_variant_unref(keyVar);
		g_variant_unref(propertyVar);
	}

	if (changed)
		notifyWatcherAboutChangedProperties();
}

void Bluez5ObexTransfer::handlePropertiesChanged(BluezObexTransfer1 *, gchar *interface,  GVariant *changedProperties,
												   GVariant *invalidatedProperties, gpointer userData)
{
	auto transfer = static_cast<Bluez5ObexTransfer*>(userData);

	transfer->updateFromProperties(changedProperties);
}

void Bluez5ObexTransfer::cancel(BluetoothResultCallback callback)
{
	auto cancelCallback = [this, callback](GAsyncResult *result) {
		GError *error = 0;
		gboolean ret;

		ret = bluez_obex_transfer1_call_cancel_finish(mTransferProxy, result, &error);
		if (error)
		{
			g_error_free(error);
			callback(BLUETOOTH_ERROR_FAIL);
			return;
		}

		callback(BLUETOOTH_ERROR_NONE);
	};

	bluez_obex_transfer1_call_cancel(mTransferProxy, NULL, glibAsyncMethodWrapper,
									 new GlibAsyncFunctionWrapper(cancelCallback));
}

bool Bluez5ObexTransfer::isPartOfSession(Bluez5ObexSession *session)
{
	return mObjectPath.find(session->getObjectPath()) == 0;
}
