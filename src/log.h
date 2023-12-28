#ifndef LOG_H
#define LOG_H

#include <stdint.h>

#ifndef SILENT
  #define log(...) log_msg(__FILE__, __LINE__, __VA_ARGS__)
#else
  #define log(...)
#endif

void log_msg(const char *file, uint32_t line, const char *fmt, ...);

#endif
