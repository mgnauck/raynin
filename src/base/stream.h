
#ifndef STREAM_H
#define STREAM_H

#include <stddef.h>
#include <stdint.h>
#include "base.h"

typedef struct
{
    uint8_t *data;
    uint32_t offset;
} Stream;

Stream *streamAllocate(uint8_t *data);
uint8_t streamReadUInt8(Stream *stream);
uint16_t streamReadUInt16(Stream *stream);
uint32_t streamReadUInt32(Stream *stream);
float streamReadFloat32(Stream *stream);
void streamRead(Stream *stream, uint8_t *data, uint32_t length);

#endif
