#ifndef TEXT_H
#define TEXT_H

#include <stddef.h>
#include <stdint.h>

extern size_t global_str_buf_pos;

void print_buf(size_t len);

size_t push_str(const char *str, size_t ofs);
size_t push_int(int n, size_t ofs);
size_t push_float(float f, uint8_t precision, size_t ofs);

void print_str(const char *str);
void print_int(int n);
void print_float(float f, uint8_t precision);

void text_begin();
void text_str(const char *str);
void text_int(int n);
void text_float(float f);
void text_float_p(float f, uint8_t precision);
void text_end();

#endif
