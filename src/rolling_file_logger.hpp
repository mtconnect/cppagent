//
// Copyright Copyright 2009-2021, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
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

#include <dlib/dir_nav.h>
#include <dlib/threads.h>

#include <mutex>
#include <streambuf>
#include <string>

namespace mtconnect
{
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
    RollingFileLogger(std::string filename, int maxBackupIndex = 9,
                      uint64_t maxSize = 10ull * 1024ull * 1024ull,
                      RollingSchedule schedule = NEVER);

    ~RollingFileLogger();

    void write(const char *message);

    uint64_t getMaxSize() const { return m_maxSize; }

  protected:
    void rollover(uint64_t size);
    std::time_t getFileAge();

  private:
    std::mutex m_fileLock;

    std::string m_name;
    dlib::directory m_directory;
    dlib::file m_file;

    int m_maxBackupIndex;
    uint64_t m_maxSize;
    RollingSchedule m_schedule;

    int m_fd;
  };
}  // namespace mtconnect
