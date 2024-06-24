#include "log.h"
#include <stdarg.h>
#include "settings.h"

#ifdef NATIVE_BUILD
#include <SDL.h>
#else
#include "ext/printf.h"
extern void log_buf(char *addr, uint32_t len);
#endif

#define LINE_BUF_SIZE 65536
static char line_buf[LINE_BUF_SIZE];

void log_msg(const char *file, uint32_t line, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);

  size_t len = snprintf(line_buf, LINE_BUF_SIZE, "%s:%d: ", file, line);
  len += vsnprintf(&line_buf[len], LINE_BUF_SIZE - len, format, ap);
#ifndef NATIVE_BUILD
  log_buf(line_buf, len);
#else
  SDL_Log("%s", line_buf);
#endif

  va_end(ap);
}
