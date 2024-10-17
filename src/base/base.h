
#ifndef __BASE_H
#define __BASE_H

#define JS_IMPORT(name, func) __attribute__((import_module("env"), import_name(name))) func
#define JS_EXPORT __attribute__((visibility("default")))

#endif
