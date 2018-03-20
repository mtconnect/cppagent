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

#include "globals.hpp"
#include <string>
#include <streambuf>
#include <mutex>
#include <dlib/threads.h>
#include <dlib/dir_nav.h>

class RollingFileLogger
{
public:
	enum RollingSchedule
	{
		DAILY,
		WEEKLY,
		NEVER
	};

	// Default the rolling logger to change create a new file every 10M of data
	RollingFileLogger(
		std::string filename,
		int maxBackupIndex = 9,
		size_t maxSize = 10 * 1024 * 1024,
		RollingSchedule schedule = NEVER);

	~RollingFileLogger();

	void write(const char *message);

	int getMaxSize() const {
		return m_maxSize; }

protected:
	void rollover(size_t aSize);
	int getFileAge();

private:
	std::mutex m_fileLock;

	std::string m_name;
	dlib::directory m_directory;
	dlib::file m_file;

	int m_maxBackupIndex;
	size_t m_maxSize;
	RollingSchedule m_schedule;

	int m_fd;
};
