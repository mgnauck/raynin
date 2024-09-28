#ifndef LOG_H
#define LOG_H

#include <stdint.h>

#define SILENT
#ifndef SILENT
  #define logc(...) log_msg(__FILE__, __LINE__, __VA_ARGS__)
#else
  #define logc(...) ((void)0)
#endif

void log_msg(const char *file, uint32_t line, const char *fmt, ...);

#endif
