#include "sync.h"

#include <stddef.h>

#include "../base/log.h"
#include "../base/math.h"
#include "../base/string.h"
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

void sync_init_track(track *t, uint16_t max_event_cnt, uint16_t bpm,
                     uint8_t rows_per_beat)
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

void sync_add_event(track *t, uint16_t row, uint8_t id, float value,
                    blend_type type)
{
  // Events come in sorted by row, make sure they are sorted by id as well
  uint16_t idx = 0;
  for(; idx < t->event_cnt; idx++) {
    event *e = &t->events[idx];
    if(e->id > id)
      break;
  }

  if(idx < t->event_cnt)
    // Move events from idx to end of array up by one position (in reverse)
    for(uint16_t i = t->event_cnt - 1; i >= idx; i--)
      memcpy(&t->events[i + 1], &t->events[i], sizeof(event));

  t->events[idx] = (event){row, id, value, type};
  t->event_cnt++;

  // Track latest row
  if(t->max_row < row)
    t->max_row = row;
}

void sync_index_track(track *t)
{
  // Find begin and end index for each event id
  for(uint16_t j = 0; j < sizeof(t->event_states) / sizeof(t->event_states[0]);
      j++) {
    int32_t beg = -1;
    int32_t end = -1;
    for(uint16_t i = 0; i < t->event_cnt; i++) {
      event *e = &t->events[i];
      if(beg < 0 && e->id == j) {
        beg = i;
        if(i == t->event_cnt - 1)
          end = i;
        continue;
      }
      if(beg >= 0 && (e->id > j || i == t->event_cnt - 1)) {
        end = i < t->event_cnt - 1 ? i - 1 : i;
        break;
      }
    }

    // Init even state (if event id was not contained, beg/end is still -1)
    t->event_states[j] = (event_state){beg, end, -1};
    // logc("id: %i, beg: %i, end: %i", j, beg, end);
  }
}

bool sync_is_finished(const track *t, float time)
{
  return t->max_row < time * t->row_rate || t->event_cnt == 0;
}

bool sync_event_is_active(const track *t, uint8_t id, float time)
{
  float row_time = time * t->row_rate;
  uint16_t row = (uint16_t)floorf(time * t->row_rate);

  // Event id is not contained, thus inactive
  const event_state *es = &t->event_states[id];
  if(es->beg_idx < 0)
    return false;

  // Get start index into the event type's begin or previous row
  uint16_t beg_idx = es->row_idx < 0 ? es->beg_idx : es->row_idx;

  return t->events[beg_idx].row <= row;
}

float sync_event_get_value(track *t, uint8_t id, float time)
{
  float row_time = time * t->row_rate;
  uint16_t row = (uint16_t)floorf(time * t->row_rate);
  const event *e0 = NULL;
  const event *e1 = NULL;

  // Event id is not contained, thus inactive
  const event_state *es = &t->event_states[id];
  if(es->beg_idx < 0) {
    return EVENT_INACTIVE;
  }

  // Get start index into the event type's begin or previous row
  uint16_t beg_idx = es->row_idx < 0 ? es->beg_idx : es->row_idx;

  // Event is not active yet
  if(t->events[beg_idx].row > row) {
    return EVENT_INACTIVE;
  }

  for(uint32_t i = beg_idx; i <= (uint32_t)es->end_idx; i++) {
    const event *c = &t->events[i];
    if(c->row <= row) {
      // First one before current row time is our e0
      e0 = c;
      // Update our running row index for this event id
      t->event_states[id].row_idx = i;
      continue;
    }
    // Next event is our e1
    e1 = c;
    break;
  }

  // if(e0->id == 5)
  //   logc("%3.6f: Found e0 at row %i", row_time, e0->row);

  if(!e1)
    return e0->value;

  // if(e1->id == 5)
  //   logc("%3.6f: Found e1 at row %i", row_time, e1->row);

  switch(e0->type) {
    case BT_STEP:
      return e0->value;
    case BT_LINEAR:
      return blend_linear(e0, e1, row_time);
    case BT_SMOOTH:
      return blend_smooth(e0, e1, row_time);
    case BT_RAMP:
      return blend_ramp(e0, e1, row_time);
    default:
      {
        logc("### ERROR Sync: Unknown blend type");
        return EVENT_ERROR;
      }
  }
}

/*bool sync_event_is_active(const track *t, uint8_t id, float time)
{
  uint16_t row = (uint16_t)floorf(time * t->row_rate);
  for(uint32_t i=0; i<t->event_cnt; i++) {
    const event *c = &t->events[i];
    if(c->id == id && c->row <= row)
      return true;
  }
  return false;
}


float sync_event_search_value(const track *t, uint8_t id, float time)
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
}*/
