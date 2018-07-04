// Copyright (c) 2018 LG Electronics, Inc.
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

#ifndef BLUEZ_UTILS_H
#define BLUEZ_UTILS_H

#include <locale>
#include <string>
#include <vector>
#include <glib.h>

#include "logging.h"

#define UNUSED(expr) do { (void)(expr); } while (0)

std::string convertAddressToLowerCase(const std::string &input);
std::string convertAddressToUpperCase(const std::string &input);
std::vector<unsigned char>convertArrayByteGVariantToVector(GVariant *iter);
GVariant* convertVectorToArrayByteGVariant(const std::vector<unsigned char> &vectorValue);
void splitInPathAndName(const std::string &serviceObjectPath, std::string &path, std::string &name);

#endif // BLUEZ_UTILS_H
