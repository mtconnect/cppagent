/*
 * Copyright Copyright 2012, System Insights, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

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
  : mName(aName), mMaxBackupIndex(aMaxBackupIndex), mMaxSize(aMaxSize),
    mSchedule(aSchedule), mFd(0)
{
  mFileLock = new dlib::mutex();
  
  mFd = ::open(aName.c_str(), O_CREAT | O_APPEND | O_WRONLY, 0644);
  if (mFd < 0)
  {
    cerr << "Cannot log open file " << aName << endl;
    cerr << "Exiting...";
    exit(1);
  }
  file f(aName);
  mFile = f;
  mDirectory = get_parent_directory(mFile);
}

RollingFileLogger::~RollingFileLogger()
{
  delete mFileLock;
  ::close(mFd);
}

int RollingFileLogger::getFileAge()
{
  struct stat buffer;

  int res = ::stat(mFile.full_name().c_str(), &buffer);
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
  
  if (mSchedule != NEVER) {
    int age = getFileAge();
    if ((mSchedule == DAILY && age > DAY) ||
        (mSchedule == WEEKLY && age > 7 * DAY))
      rollover(0);
    
  } else {
    file f(mFile.full_name());
    size_t size = f.size();
    if (size + len >= mMaxSize)
      rollover(size);
  }
  
  ::write(mFd, aMessage, len);
}

void RollingFileLogger::rollover(size_t aSize)
{
  dlib::auto_mutex M(*mFileLock);

  // File was probable rolled over... threading race.
  if (mSchedule != NEVER) {
    int age = getFileAge();
    if ((mSchedule == DAILY && age < DAY) ||
        (mSchedule == WEEKLY && age < 7 * DAY))
      return;
  } else {
    file f(mFile.full_name());
    if (f.size() < aSize)
      return;
  }

  // Close the current file
  ::close(mFd);
  
  // Remove the last file
  std::string name = mFile.full_name() + "." + intToString(mMaxBackupIndex);
  if (file_exists(name))
    ::remove(name.c_str());
  
  // Roll over old log files.
  for (int i = mMaxBackupIndex - 1; i >= 1; i--)
  {
    std::string from = mFile.full_name() + "." + intToString(i);
    if (file_exists(from))
    {
      name = mFile.full_name() + "." + intToString(i + 1);
      ::rename(from.c_str(), name.c_str());
    }
  }
  
  name = mFile.full_name() + ".1";
  ::rename(mFile.full_name().c_str(), name.c_str());
  
  // Open new log file
  mFd = ::open(mFile.full_name().c_str(), O_CREAT | O_APPEND | O_WRONLY, 0644);
  if (mFd < 0)
  {
    cerr << "Cannot log open file " << mName << endl;
    cerr << "Exiting...";
    exit(1);
  }
}