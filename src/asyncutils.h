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

#ifndef ASYNCUTILS_H
#define ASYNCUTILS_H

#include <glib.h>
#include <gio/gio.h>
#include <functional>

typedef std::function<void(GAsyncResult *result)> GlibAsyncFunction;
typedef std::function<bool(void)> GlibSourceFunction;

class GlibAsyncFunctionWrapper
{
public:
	GlibAsyncFunctionWrapper(GlibAsyncFunction func) : mFunc(func) { }

	void call(GAsyncResult *result)
	{
		mFunc(result);
	}

private:
	GlibAsyncFunction mFunc;
};

class GlibSourceFunctionWrapper
{
public:
	GlibSourceFunctionWrapper(GlibSourceFunction func) : mFunc(func) { }

	gboolean call(void)
	{
		return (gboolean) mFunc();
	}

private:
	GlibSourceFunction mFunc;
};

void glibAsyncMethodWrapper(GObject *sourceObject, GAsyncResult *result, gpointer user_data);
gboolean glibSourceMethodWrapper(gpointer user_data);

#endif // ASYNCUTILS_H
