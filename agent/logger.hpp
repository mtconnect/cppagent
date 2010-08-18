
#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <stdarg.h>

#define LOGGER_BUFFER_SIZE 1024

class Logger {
public:
  virtual void error(const char *aFormat, ...);
  virtual void warning(const char *aFormat, ...);
  virtual void info(const char *aFormat, ...);
  
protected:
  const char *format(char *aBuffer, int aLen, const char *aFormat, va_list args);
  const char *timestamp(char *aBuffer, int aLen);
};

extern Logger *gLogger;

#endif