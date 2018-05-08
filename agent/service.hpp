//
// Copyright Copyright 2012, System Insights, Inc.
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
#pragma once

#include <string>
#include "dlib/logger.h"

#define NAME_LEN 80

class MTConnectService
{
public:
	MTConnectService();
	virtual int main(int aArgc, const char *aArgv[]);
	virtual void initialize(int aArgc, const char *aArgv[]) = 0;

	void setName(const std::string &aName) { m_name = aName; }
	virtual void stop() = 0;
	virtual void start() = 0;
	const std::string &name() { return m_name; }

protected:
	std::string m_name;
	std::string m_configFile;
	std::string m_pidFile;
	bool m_isService;
	bool m_isDebug;

	void install();
	void remove();

#ifndef _WINDOWS
	void daemonize();
#endif

};
