#include "sync.h"
#include <stddef.h>
#include "../base/math.h"
#include "../base/log.h"
#include "../base/walloc.h"

float get_row_rate(uint16_t bpm, uint8_t rows_per_beat)
{
  return rows_per_beat * bpm / 60.0f;
}

// Blend modes like rocket
float blend_linear(const event *e0, const event *e1, float row)
{
  float a = (row - (float)e0->row) / ((float)e1->row - (float)e0->row);
  return e0->value + (e1->value - e0->value) * a;
}

float blend_smooth(const event *e0, const event *e1, float row)
{
  // Smoothstep
  float a = (row - (float)e0->row) / ((float)e1->row - (float)e0->row);
  a = a * a * (3.0f - 2.0f * a);
  return e0->value + (e1->value - e0->value) * a;
}

float blend_ramp(const event *e0, const event *e1, float row)
{
  float a = (row - (float)e0->row) / ((float)e1->row - (float)e0->row);
  return e0->value + (e1->value - e0->value) * a * a;
}

void sync_init_track(track *t, uint16_t max_event_cnt, uint16_t bpm, uint8_t rows_per_beat)
{
  t->events = malloc(max_event_cnt * sizeof(*t->events));
  t->event_cnt = 0;
  t->row_rate = get_row_rate(bpm, rows_per_beat);
  t->max_row = 0;
}

void sync_release_track(track *t)
{
  free(t->events);
}

void sync_add_event(track *t, uint16_t row, uint8_t id, float value, blend_type type)
{
  t->events[t->event_cnt++] = (event){ row, id, value, type };

  // Track latest row
  if(t->max_row < row)
    t->max_row = row;
}

bool sync_is_finished(const track *t, float time)
{
  return t->max_row < time * t->row_rate || t->event_cnt == 0;
}

bool sync_is_active(const track *t, uint8_t id, float time)
{
  uint16_t row = (uint16_t)floorf(time * t->row_rate);
  for(uint32_t i=0; i<t->event_cnt; i++) {
    const event *c = &t->events[i];
    if(c->id == id && c->row <= row)
      return true;
  }
  return false;
}

float sync_get_value(const track *t, uint8_t id, float time)
{
  float row_time = time * t->row_rate;
  uint16_t row = (uint16_t)floorf(time * t->row_rate);
  const event *e0 = NULL;
  const event *e1 = NULL;

  for(uint32_t i=0; i<t->event_cnt; i++) {
    const event *c = &t->events[i];
    if(c->id == id) {
      if(c->row <= row) {
        // First one before current row time
        e0 = c;
        continue;
      }
      if(e0 && c->row > row) {
        // Direct next one after current row time
        e1 = c;
        break;
      }
    }
  }

  if(!e0)
    return EVENT_INACTIVE;

  //if(e0->id == 5)
  //  logc("%3.6f: Found e0 at row %i", row_time, e0->row);

  if(!e1)
    return e0->value;
 
  //if(e1->id == 5)
  //  logc("%3.6f: Found e1 at row %i", row_time, e1->row);

  switch(e0->type) {
    case BT_STEP:
      return e0->value;
    case BT_LINEAR:
      return blend_linear(e0, e1, row_time);
    case BT_SMOOTH:
      return blend_smooth(e0, e1, row_time);
    case BT_RAMP:
      return blend_ramp(e0, e1, row_time);
    default: {
      logc("### ERROR Sync: Unknown blend type");
      return EVENT_ERROR;
    }
  }
}
