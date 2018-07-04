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
#include <vector>

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

GVariant* convertVectorToArrayByteGVariant(const std::vector<unsigned char> &vectorValue)
{
	gsize size;
	guint i;
	unsigned int vectorSize = vectorValue.size();
	guchar const *vectorData = vectorValue.data();

	GVariantBuilder *builder = g_variant_builder_new (G_VARIANT_TYPE ("ay"));
	if (vectorData != NULL)
	{
		for (i = 0; i < vectorSize; i++)
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
