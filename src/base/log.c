#include "log.h"
#include <stdarg.h>
#include "../ext/printf.h"

extern void console(char *, size_t len);

#define LINE_BUF_SIZE 65536
static char line_buf[LINE_BUF_SIZE];

void log_msg(const char *file, uint32_t line, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);

  size_t len = snprintf(line_buf, LINE_BUF_SIZE, "%s:%d: ", file, line);
  len += vsnprintf(&line_buf[len], LINE_BUF_SIZE - len, format, ap);
  console(line_buf, len);

  va_end(ap);
}
