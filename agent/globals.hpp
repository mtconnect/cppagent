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

#ifndef GLOBALS_HPP
#define GLOBALS_HPP

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif
#if _MSC_VER > 1500
#include <stdint.h>
#else
#endif
#ifndef UINT64_MAX
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFull
#endif

#include <ctime>
#include <string>
#include <map>
#include <sstream>
#include <fstream>
#include <string>

#ifdef _WINDOWS
typedef unsigned __int64 uint64_t;
#define strtoull _strtoui64
#else
#include <stdint.h>
#include <sys/resource.h>
#endif

/***** CONSTANTS *****/

/* Port number to put server on */
const unsigned int SERVER_PORT = 8080;

/* Size of sliding buffer */
const unsigned int DEFAULT_SLIDING_BUFFER_SIZE = 131072;

/* Size of buffer exponent: 2^SLIDING_BUFFER_EXP */
const unsigned int DEFAULT_SLIDING_BUFFER_EXP = 17;
const unsigned int DEFAULT_MAX_ASSETS = 1024;

/* Message for when enumerations do not exist in an array/enumeration */
const int ENUM_MISS = -1;

/* Time format */
enum TimeFormat
{
  HUM_READ,
  GMT,
  GMT_UV_SEC,
  LOCAL
};

/***** METHODS *****/
std::string int64ToString(uint64_t i);

std::string intToString(unsigned int i);

/* Convert a float to string */
std::string floatToString(double f);

/* Convert a string to the same string with all upper case letters */
std::string toUpperCase(std::string& text);

/* Check if each char in a string is a positive integer */
bool isNonNegativeInteger(const std::string& s);

/* Get the current time formatted */
std::string getCurrentTime(TimeFormat format);

/* time_t to the ms */
uint64_t getCurrentTimeInMicros();

/* Get the relative time from using an uint64 offset in ms to time_t as a web time */
std::string getRelativeTimeString(uint64_t aTime);

/* Get the current time in number of seconds as an integer */
unsigned int getCurrentTimeInSec();

uint64_t parseTimeMicro(std::string &aTime);

/* Replace illegal XML characters with the correct corresponding characters */
void replaceIllegalCharacters(std::string& data);

/* Return enumeration values according to a string name and array */
int getEnumeration(
  const std::string& name,
  const std::string *array,
  unsigned int size
);

std::string addNamespace(const std::string aPath, const std::string aPrefix);

bool isMTConnectUrn(const char *aUrn);

// Get memory size of process in k
long getMemorySize();

#ifdef _WINDOWS
#include <io.h>
typedef long volatile AtomicInt;
#else
#ifdef MACOSX
typedef _Atomic_word AtomicInt;
#else
typedef int AtomicInt;
#endif
#endif


#endif

