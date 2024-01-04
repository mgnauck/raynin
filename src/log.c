#include "log.h"
#include <stddef.h>
#include <stdarg.h>
#include "printf.h"

#define LINE_BUF_SIZE 1024

static char line_buf[LINE_BUF_SIZE];

extern void log_buf(char *addr, size_t len);

void log_msg(const char *file, uint32_t line, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);

  size_t len = snprintf(line_buf, LINE_BUF_SIZE, "%s:%d: ", file, line);
  len += vsnprintf(&line_buf[len], LINE_BUF_SIZE - len, format, ap);
  log_buf(line_buf, len);

  va_end(ap);
}
