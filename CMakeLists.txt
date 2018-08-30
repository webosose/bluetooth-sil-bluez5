# Copyright (c) 2018 LG Electronics, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 2.8.7)

project(bluetooth-sil-bluez5 C CXX)

include(webOS/webOS)
webos_modules_init(1 0 0 QUALIFIER RC7)
webos_component(0 1 0)

# Enable C++11 support (still gcc 4.6 so can't use -std=c++11)
_webos_manipulate_flags(APPEND CXX ALL -std=c++0x)

find_program(GDBUS_CODEGEN_EXECUTABLE NAMES gdbus-codegen DOC "gdbus-codegen executable")
if(NOT GDBUS_CODEGEN_EXECUTABLE)
	message(FATAL_ERROR "Executable gdbus-codegen not found")
endif()

include(FindPkgConfig)

pkg_check_modules(SILAPI REQUIRED bluetooth-sil-api)
include_directories(${SILAPI_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${SILAPI})

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${GLIB2_CFLAGS_OTHER})

pkg_check_modules(GIO2 REQUIRED gio-2.0)
include_directories(${GIO2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${GIO2_CFLAGS})

pkg_check_modules(GIO-UNIX REQUIRED gio-unix-2.0)
include_directories(${GIO-UNIX_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${GIO-UNIX_CFLAGS_OTHER})

pkg_check_modules(PMLOG REQUIRED PmLogLib)
include_directories(${PMLOG_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${PMLOG_CFLAGS_OTHER})

set(CMAKE_SHARED_MODULE_PREFIX "")

webos_add_linker_options(ALL --no-undefined)

set(GDBUS_IF_DIR ${WEBOS_BINARY_CONFIGURED_DIR}/src)
file(MAKE_DIRECTORY ${GDBUS_IF_DIR})

execute_process(COMMAND ${GDBUS_CODEGEN_EXECUTABLE}
        --c-namespace Bluez
        --c-generate-object-manager
        --generate-c-code ${GDBUS_IF_DIR}/bluez-interface
        --interface-prefix org.bluez.
        ${CMAKE_CURRENT_SOURCE_DIR}/files/xml/bluez5.xml
        RESULT_VARIABLE codegen_failed)
if (codegen_failed)
    message(FATAL_ERROR "Error in generating code for bluez5 dbus interface using gdbus-codegen")
endif()

execute_process(COMMAND ${GDBUS_CODEGEN_EXECUTABLE}
        --c-namespace FreeDesktop
        --generate-c-code ${GDBUS_IF_DIR}/freedesktop-interface
        --interface-prefix org.freedesktop.
        ${CMAKE_CURRENT_SOURCE_DIR}/files/xml/freedesktop.xml
        RESULT_VARIABLE codegen_failed)
if (codegen_failed)
    message(FATAL_ERROR "Error in generating code for freedesktop dbus interface using gdbus-codegen")
endif()

include_directories(src ${GDBUS_IF_DIR})

file(GLOB SOURCES
     ${GDBUS_IF_DIR}/bluez-interface.c
     ${GDBUS_IF_DIR}/freedesktop-interface.c
     src/asyncutils.cpp
     src/dbusutils.cpp
     src/bluez5adapter.cpp
     src/bluez5sil.cpp
     src/bluez5device.cpp
     src/bluez5agent.cpp
     src/bluez5obexclient.cpp
     src/bluez5obexsession.cpp
     src/bluez5obextransfer.cpp
     src/bluez5profilebase.cpp
     src/bluez5profileftp.cpp
     src/bluez5advertise.cpp
     src/utils.cpp
     src/bluez5profilegatt.cpp
     src/bluez5profilespp.cpp
     src/bluez5gattremoteattribute.cpp
     src/bluez5obexprofilebase.cpp
     src/bluez5profileopp.cpp
     )

add_library(bluez5 MODULE ${SOURCES})
target_link_libraries(bluez5 ${GLIB2_LDFLAGS} ${PMLOG_LDFLAGS}
                             ${GIO2_LDFLAGS} ${GIO-UNIX_LDFLAGS})
install(TARGETS bluez5 DESTINATION ${WEBOS_INSTALL_LIBDIR}/bluetooth-sils)
