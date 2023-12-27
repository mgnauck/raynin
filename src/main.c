#include "text.h"

  __attribute__((visibility("default")))
void init(void)
{
  print_str("init()");
}

__attribute__((visibility("default")))
void release(void)
{
  print_str("release()");
}
