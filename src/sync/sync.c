#include "sync.h"
#include <stddef.h>
#include "../base/walloc.h"

float get_row_rate(float bpm, float rows_per_beat)
{
  return rows_per_beat * bpm / 60.0f;
}

// Blend modes like rocket
float blend_linear(const event *e0, const event *e1, float row)
{
  float a = (row - e0->row) / (e1->row - e0->row);
  return e0->value + (e1->value - e0->value) * a;
}

float blend_smooth(const event *e0, const event *e1, float row)
{
  // Smoothstep
  float a = (row - e0->row) / (e1->row - e0->row);
  a = a * a * (3.0f - 2.0f * a);
  return e0->value + (e1->value - e0->value) * a;
}

float blend_ramp(const event *e0, const event *e1, float row)
{
  float a = (row - e0->row) / (e1->row - e0->row);
  return e0->value + (e1->value - e0->value) * a * a;
}

void sync_init_track(track *t, uint16_t max_event_cnt, float bpm, float rows_per_beat)
{
  t->events = malloc(max_event_cnt * sizeof(*t->events));
  t->event_cnt = 0;
  t->row_rate = get_row_rate(bpm, rows_per_beat);
}

void sync_release_track(track *t)
{
  free(t->events);
}

void sync_add_event(track *t, uint16_t row, uint8_t id, float value, blend_type type)
{
  t->events[t->event_cnt++] = (event){ row, id, value, type };
}

float sync_get_value(const track *t, uint8_t id, float time)
{
  float row_time = time * t->row_rate;
  const event *e = NULL;

  // Brute force search pair of rows for given id
  for(uint32_t i=0; i<t->event_cnt; i++) {
    const event *c = &t->events[i];
    if(c->id == id) {
      if(c->row == row_time)
        // Exact row match
        return c->value;
      else if((!e && c->row < row_time) || (e && c->row < row_time && e->row < c->row))
        // Row before given row time
        e = c;
      else {
        // Row that is the next after the given row time
        switch(e->type) {
          case BT_STEP:
            return e->value;
          case BT_LINEAR:
            return blend_linear(e, c, row_time);
          case BT_SMOOTH:
            return blend_smooth(e, c, row_time);
          case BT_RAMP:
            return blend_ramp(e, c, row_time);
          default:
            // Unknown blend type
            return BROKEN_EVENT; 
        }
      }
    }
  }
  // Only found the row before row time or did not find the id at all
  return e ? e->value : BROKEN_EVENT; 
}
