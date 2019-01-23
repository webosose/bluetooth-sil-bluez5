// Copyright (c) 2014-2019 LG Electronics, Inc.
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


#ifndef LOGGING_H
#define LOGGING_H

#include <PmLogLib.h>

extern PmLogContext bluez5LogContext;

#define CRITICAL(msgid, kvcount, ...) \
	PmLogCritical(bluez5LogContext, msgid, kvcount, ##__VA_ARGS__)

#define ERROR(msgid, kvcount, ...) \
	PmLogError(bluez5LogContext, msgid, kvcount,##__VA_ARGS__)

#define WARNING(msgid, kvcount, ...) \
	PmLogWarning(bluez5LogContext, msgid, kvcount, ##__VA_ARGS__)

#define INFO(msgid, kvcount, ...) \
	PmLogInfo(bluez5LogContext, msgid, kvcount, ##__VA_ARGS__)

#define DEBUG(...) \
	PmLogDebug(bluez5LogContext, ##__VA_ARGS__)

#define MSGID_DBUS_NOT_AVAILABLE                       "DBUS_NOT_AVAILABLE"
#define MSGID_OBJECT_MANAGER_CREATION_FAILED           "OBJECT_MANAGER_CREATION_FAILED"
#define MSGID_ALREADY_CONNECTED                        "ALREADY_CONNECTED"
#define MSGID_PROXY_ALREADY_EXISTS                     "PROXY_ALREADY_EXISTS"
#define MSGID_FAILED_TO_CREATE_ADAPTER_PROXY           "FAILED_TO_CREATE_ADAPTER_PROXY"
#define MSGID_FAILED_TO_CREATE_AGENT_MGR_PROXY         "FAILED_TO_CREATE_AGENT_MGR_PROXY"
#define MSGID_MULTIPLE_AGENT_MGR                       "MULTIPLE_AGENT_MGR"
#define MSGID_AGENT_INIT_ERROR                         "AGENT_INIT_ERROR"
#define MSGID_AGENT_NOT_READY                          "AGENT_NOT_READY"
#define MSGID_FAILED_TO_GET_SYSTEM_BUS                 "FAILED_TO_GET_SYSTEM_BUS"
#define MSGID_FAILED_TO_CREATE_OBEX_CLIENT_PROXY       "FAILED_TO_CREATE_OBEX_CLT_PROXY"
#define MSGID_FAILED_TO_CREATE_OBEX_AGENT_MGR_PROXY    "FAILED_TO_CREATE_OBEX_AGENT_MGR_PROXY"
#define MSGID_FAILED_TO_CREATE_OBEX_SESSION_PROXY      "FAILED_TO_CREATE_OBEX_SN_PROXY"
#define MSGID_FAILED_TO_REMOVE_OBEX_SESSION_PROXY      "FAILED_TO_REMOVE_OBEX_SN_PROXY"
#define MSGID_FAILED_TO_CREATE_OBEX_FILE_TRANSFER_PROXY     "FAILED_TO_CREATE_OBEX_FT_PROXY"
#define MSGID_FAILED_TO_CREATE_OBEX_TRANSFER_PROXY     "FAILED_TO_CREATE_OBEX_TRANS_PROXY"
#define MSGID_FAILED_TO_CREATE_OBEX_PUSH_PROXY         "FAILED_TO_CREATE_OBEX_PUSH_PROXY"
#define MSGID_PAIRING_CANCEL_ERROR                     "PAIRING_CANCEL_ERROR"
#define MSGID_PAIRING_IO_CAPABILITY_ERROR              "PAIRING_IO_CAPABILITY_ERROR"
#define MSGID_PAIRING_IO_CAPABILITY_STRING_ERROR       "PAIRING_IO_CAPABILITY_STRING_ERROR"
#define MSGID_PROFILE_MANAGER_ERROR                    "PROFILE_MANAGER_ERROR"
#define MSGID_GATT_PROFILE_ERROR                       "GATT_PROFILE_ERROR"
#define MSGID_BLE_ADVERTIMENT_ERROR                    "BLE_ADVERTIMENT_ERROR"
#define MSGID_MEDIA_PLAYER_ERROR                    "MEDIA_PLAYER_ERROR"
#define MSGID_MEDIA_CONTROL_ERROR                      "MEDIA_CONTROL_ERROR"

#endif // LOGGING_H
