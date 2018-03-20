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
#include "rolling_file_logger.hpp"
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ostream>

using namespace dlib;
using namespace std;

RollingFileLogger::RollingFileLogger(std::string aName,
					 int aMaxBackupIndex,
					 size_t aMaxSize,
					 RollingSchedule aSchedule)
	: m_name(aName), m_maxBackupIndex(aMaxBackupIndex), m_maxSize(aMaxSize),
	  m_schedule(aSchedule), m_fd(0)
{
	m_fileLock = new dlib::mutex();

	m_fd = ::open(aName.c_str(), O_CREAT | O_APPEND | O_WRONLY, 0644);

	if (m_fd < 0)
	{
	cerr << "Cannot log open file " << aName << endl;
	cerr << "Exiting...";
	exit(1);
	}

	file f(aName);
	m_file = f;
	m_directory = get_parent_directory(m_file);
}

RollingFileLogger::~RollingFileLogger()
{
	delete m_fileLock;
	::close(m_fd);
}

int RollingFileLogger::getFileAge()
{
	struct stat buffer;

	int res = ::stat(m_file.full_name().c_str(), &buffer);

	if (res < 0)
	return 0;
	else
#ifdef __APPLE__
	return time(NULL) - buffer.st_ctimespec.tv_sec;

#else
	return time(NULL) - buffer.st_ctime;
#endif
}


const int DAY = 24 * 60 * 60;
void RollingFileLogger::write(const char *aMessage)
{
	size_t len = strlen(aMessage);

	if (m_schedule != NEVER)
	{
	int age = getFileAge();

	if ((m_schedule == DAILY && age > DAY) ||
		(m_schedule == WEEKLY && age > 7 * DAY))
		rollover(0);

	}
	else
	{
	file f(m_file.full_name());
	size_t size = f.size();

	if (size + len >= m_maxSize)
		rollover(size);
	}

	::write(m_fd, aMessage, len);
}

void RollingFileLogger::rollover(size_t aSize)
{
	dlib::auto_mutex M(*m_fileLock);

	// File was probable rolled over... threading race.
	if (m_schedule != NEVER)
	{
	int age = getFileAge();

	if ((m_schedule == DAILY && age < DAY) ||
		(m_schedule == WEEKLY && age < 7 * DAY))
		return;
	}
	else
	{
	file f(m_file.full_name());

	if (f.size() < aSize)
		return;
	}

	// Close the current file
	::close(m_fd);

	// Remove the last file
	std::string name = m_file.full_name() + "." + intToString(m_maxBackupIndex);

	if (file_exists(name))
	::remove(name.c_str());

	// Roll over old log files.
	for (int i = m_maxBackupIndex - 1; i >= 1; i--)
	{
	std::string from = m_file.full_name() + "." + intToString(i);

	if (file_exists(from))
	{
		name = m_file.full_name() + "." + intToString(i + 1);
		::rename(from.c_str(), name.c_str());
	}
	}

	name = m_file.full_name() + ".1";
	::rename(m_file.full_name().c_str(), name.c_str());

	// Open new log file
	m_fd = ::open(m_file.full_name().c_str(), O_CREAT | O_APPEND | O_WRONLY, 0644);

	if (m_fd < 0)
	{
	cerr << "Cannot log open file " << m_name << endl;
	cerr << "Exiting...";
	exit(1);
	}
}