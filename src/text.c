#include "text.h"
#include <stdint.h>
#include "mathutil.h"

extern void console_log_buf(char *addr, size_t len);

#define GLOBAL_STR_BUF_SIZE 2048

static char global_str_buf[GLOBAL_STR_BUF_SIZE];
size_t global_str_buf_pos = 0;

static const char digits[10] = "0123456789";

void print_buf(size_t len)
{
  console_log_buf(global_str_buf, len);
}

size_t push_str(const char *str, size_t ofs)
{
  if(ofs >= GLOBAL_STR_BUF_SIZE)
    return 0;

  const char *p = str;
  size_t len = GLOBAL_STR_BUF_SIZE;
  for(size_t i=ofs; i<len; i++) {
    if(*p == '\0') {
      len = ofs + (size_t)(p - str);
      break;
    } else
      global_str_buf[i] = *p++;
  }

  return len;
}

uint8_t get_num_size(int n, int base)
{  
  uint8_t size = ((n < 0) ? 1 : 0);
  do {
    n /= base;
    size++;
  } while(n);
  return size;
}

size_t push_int(int n, size_t ofs)
{ 
  uint8_t size = get_num_size(n, 10);
  if(ofs + size >= GLOBAL_STR_BUF_SIZE)
    return 0;

  char *p = &global_str_buf[ofs + size];
  int an = n < 0 ? -n : n;
  do {
    *(--p) = digits[an % 10];
    an /= 10;
  } while(an);

  if(n < 0)
    *(--p) = '-';

  return ofs + size;
}

size_t push_float(float f, uint8_t precision, size_t ofs)
{
  float prec = powf(10, precision);
  int n = (int)(f * prec);
  uint8_t num_size = get_num_size(n, 10);
  uint8_t prec_size = get_num_size((int)prec, 10);

  if(ofs + num_size + 1 >= GLOBAL_STR_BUF_SIZE)
    return 0;

  char *p = &global_str_buf[ofs + num_size + 1];
  int an = n < 0 ? -n : n;
  do {
    if(&global_str_buf[ofs + num_size + 1] - p + 1 == prec_size)
      *(--p) = '.';
    *(--p) = digits[an % 10];
    an /= 10;
  } while(an);

  if(n < 0)
    *(--p) = '-';

  return ofs + num_size + 1;
}

void print_str(const char *str)
{
  print_buf(push_str(str, 0));
}

void print_int(int number)
{
  print_buf(push_int(number, 0));
}

void print_float(float f, uint8_t precision)
{
  print_buf(push_float(f, precision, 0));
}

void text_begin()
{
  global_str_buf_pos = 0;
}

void text_str(const char *str)
{
  global_str_buf_pos = push_str(str, global_str_buf_pos);
}

void text_int(int n)
{
  global_str_buf_pos = push_int(n, global_str_buf_pos);
}

void text_float(float f)
{
  global_str_buf_pos = push_float(f, 6, global_str_buf_pos);
}

void text_float_p(float f, uint8_t precision)
{
  global_str_buf_pos = push_float(f, precision, global_str_buf_pos);
}

void text_end()
{
  print_buf(global_str_buf_pos);
}
