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

#include <utils.h>
#include <utils_mesh.h>
#include <vector>
#include <algorithm>

std::string convertAddressToLowerCase(const std::string &input)
{
	std::string output;
	std::locale loc;
	for (std::string::size_type i=0; i<input.length(); ++i)
		output += std::tolower(input[i],loc);
	return output;
}

std::string convertAddressToUpperCase(const std::string &input)
{
	std::string output;
	std::locale loc;
	for (std::string::size_type i=0; i<input.length(); ++i)
		output += std::toupper(input[i],loc);
	return output;
}

std::string convertToLowerCase(const std::string &input)
{
	std::string output;
	output = convertAddressToLowerCase(input);
	return output;
}

std::string convertToUpperCase(const std::string &input)
{
	std::string output;
	output = convertAddressToUpperCase(input);
	return output;
}

std::vector<unsigned char>convertArrayByteGVariantToVector(GVariant *iter)
{
	GVariantIter *valueIter;
	guchar valueByte;
	std::vector<unsigned char> value;

	g_variant_get (iter,
                   "ay",
                   &valueIter);

	if (valueIter == nullptr)
	{
		//return empty vector
		return value;
	}

	while (g_variant_iter_loop(valueIter, "y", &valueByte)) {
		value.push_back(valueByte);
	}

	g_variant_iter_free(valueIter);
	return value;
}

std::vector<std::string>convertArrayStringGVariantToVector(GVariant *iter)
{
	GVariantIter *valueIter;
	char *valueByte;
	std::vector<std::string> value;

	g_variant_get (iter,
                   "as",
                   &valueIter);

	if (valueIter == nullptr)
	{
		return value;
	}

	while (g_variant_iter_loop(valueIter, "s", &valueByte)) {
		std::string s(valueByte);
		value.push_back(s);
	}

	g_variant_iter_free(valueIter);
	return value;
}

GVariant* convertVectorToArrayByteGVariant(const std::vector<unsigned char> &v)
{
	unsigned int vectorSize = v.size();
	guchar const *vectorData = v.data();

	GVariantBuilder *builder = g_variant_builder_new (G_VARIANT_TYPE ("ay"));
	if (vectorData != NULL)
	{
		for (unsigned int i = 0; i < vectorSize; i++)
			g_variant_builder_add(builder, "y", vectorData[i]);
	}
	GVariant *variantValue = g_variant_builder_end(builder);
	g_variant_builder_unref(builder);

	return variantValue;
}

void splitInPathAndName(const std::string &serviceObjectPath, std::string &path, std::string &name)
{
	std::size_t found = serviceObjectPath.find_last_of('/');
	if (found == std::string::npos)
	{
		WARNING(MSGID_GATT_PROFILE_ERROR, 0, "Failed, object path not correct");
		return;
	}
	path = serviceObjectPath.substr(0, found);
	name = serviceObjectPath.substr(found+1);
}

void objPathToDevAddress(const std::string &objectPath, std::string &devAddress)
{
	devAddress = objectPath;
	auto pos = devAddress.find("dev_");
	pos = (pos != std::string::npos)?(pos):(0);
	if (devAddress.size() > 4)
		devAddress = devAddress.substr(pos + 4);
	pos = devAddress.find("/");
	pos = (pos != std::string::npos)?(pos):(devAddress.size());
	devAddress = devAddress.substr(0, pos);
	if (!devAddress.empty()){
		std::replace(devAddress.begin(), devAddress.end(), '_', ':');
	}
}

bool meshOpcodeGet(const uint8_t *buf, uint16_t sz, uint32_t *opcode, int *n)
{
	if (!n || !opcode || sz < 1)
		return false;

	switch (buf[0] & 0xc0) {
	case 0x00:
	case 0x40:
		/* RFU */
		if (buf[0] == 0x7f)
			return false;

		*n = 1;
		*opcode = buf[0];
		break;

	case 0x80:
		if (sz < 2)
			return false;

		*n = 2;
		*opcode = get_be16(buf);
		break;

	case 0xc0:
		if (sz < 3)
			return false;

		*n = 3;
		*opcode = get_be16(buf + 1);
		*opcode |= buf[0] << 16;
		break;

	default:
		DEBUG("Bad opcode");
		return false;
	}
	return true;
}

uint16_t meshOpcodeSet(uint32_t opcode, uint8_t *buf)
{
	if (opcode <= 0x7e) {
		buf[0] = opcode;
		return 1;
	} else if (opcode >= 0x8000 && opcode <= 0xbfff) {
		put_be16(opcode, buf);
		return 2;
	} else if (opcode >= 0xc00000 && opcode <= 0xffffff) {
		buf[0] = (opcode >> 16) & 0xff;
		put_be16(opcode, buf + 1);
		return 3;
	} else {
		DEBUG("Illegal Opcode %x", opcode);
		return 0;
	}
}
