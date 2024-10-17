
#include "stream.h"
#include "../base/walloc.h"
#include "../base/string.h"

Stream *streamAllocate(uint8_t *data)
{
    Stream *stream = (Stream *)malloc(sizeof(Stream));

    stream->data = data;
    stream->offset = 0;

    return stream;
}

uint8_t streamReadUInt8(Stream *stream)
{
    return *(stream->data + stream->offset++);
}

/*
// LARGER
uint16_t streamReadUInt16(Stream *stream)
{
    return (uint16_t)(streamReadUInt8(stream) |
                      (streamReadUInt8(stream) << 8));
}
*/
// SMALLER
uint16_t streamReadUInt16(Stream *stream)
{
    uint16_t value;
    memcpy(&value, stream->data + stream->offset, sizeof(uint16_t));
    stream->offset += sizeof(uint16_t);
    return value;
}

uint32_t streamReadUInt32(Stream *stream)
{
    uint32_t value;
    memcpy(&value, stream->data + stream->offset, sizeof(uint32_t));
    stream->offset += sizeof(uint32_t);
    return value;
}

float streamReadFloat32(Stream *stream)
{
    float value;
    memcpy(&value, stream->data + stream->offset, sizeof(float));
    stream->offset += sizeof(float);
    return value;
}

void streamRead(Stream *stream, uint8_t *data, uint32_t length)
{
    memcpy(data, stream->data + stream->offset, length);
    stream->offset += length;
}
