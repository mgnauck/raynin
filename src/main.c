#include "log.h"

__attribute__((visibility("default")))
void init(void)
{
  log("init()");
}

__attribute__((visibility("default")))
void release(void)
{
  log("release()");
}
