#include "log.h"
#include <stddef.h>
#include <stdarg.h>
#include "printf.h"

#define LOG_LINE_BUFFER_SIZE 1024

static char log_line_buffer[LOG_LINE_BUFFER_SIZE];

extern void console_log_buf(char *addr, size_t len);

void log_msg(const char *file, uint32_t line, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);

  size_t len = snprintf(log_line_buffer, LOG_LINE_BUFFER_SIZE, "%s:%d: ", file, line);
  len += vsnprintf(&log_line_buffer[len], LOG_LINE_BUFFER_SIZE - len, format, ap);
  console_log_buf(log_line_buffer, len);

  va_end(ap);
}
