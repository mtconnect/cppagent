

#include "logger.hpp"

#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif
#include <sys/stat.h>
#include <stdio.h>
#include <iostream>

#include <stdlib.h>
#include <string.h>

#ifndef WIN32
#define vsprintf_s(b, l, f, e) vsprintf(b, f, e)
#endif

Logger *gLogger = NULL;

void Logger::error(const char *aFormat, ...)
{
  char buffer[LOGGER_BUFFER_SIZE];
  char ts[32];
  va_list args;
  va_start (args, aFormat);
  fprintf(stderr, "%s - Error: %s\n", timestamp(ts, 32), format(buffer, LOGGER_BUFFER_SIZE, aFormat, args));
  va_end (args);
}

void Logger::warning(const char *aFormat, ...)
{
  char buffer[LOGGER_BUFFER_SIZE];
  char ts[32];
  va_list args;
  va_start (args, aFormat);
  fprintf(stderr, "%s - Warning: %s\n", timestamp(ts, 32), format(buffer, LOGGER_BUFFER_SIZE, aFormat, args));
  va_end (args);
}

void Logger::info(const char *aFormat, ...)
{
  char buffer[LOGGER_BUFFER_SIZE];
  char ts[32];
  va_list args;
  va_start (args, aFormat);
  fprintf(stderr, "%s - Info: %s\n", timestamp(ts, 32), format(buffer, LOGGER_BUFFER_SIZE, aFormat, args));
  va_end (args);
}

const char *Logger::format(char *aBuffer, int aLen, const char *aFormat, va_list args)
{
  vsprintf_s(aBuffer, aLen, aFormat, args);
  aBuffer[aLen - 1] = '\0';
  return aBuffer;
}

const char *Logger::timestamp(char *aBuffer, int aLen)
{
#ifdef WIN32
  SYSTEMTIME st;
  GetSystemTime(&st);
  sprintf_s(aBuffer, aLen, "%4d-%02d-%02dT%02d:%02d:%02d.%04dZ", st.wYear, st.wMonth, st.wDay, st.wHour, 
          st.wMinute, st.wSecond, st.wMilliseconds);
#else
  struct timeval tv;
  struct timezone tz;

  gettimeofday(&tv, &tz);

  strftime(aBuffer, 64, "%Y-%m-%dT%H:%M:%S", gmtime(&tv.tv_sec));
  sprintf(aBuffer + strlen(aBuffer), ".%06dZ", tv.tv_usec);
#endif
  
  return aBuffer;
}