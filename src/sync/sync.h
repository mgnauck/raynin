#ifndef SYNC_H
#define SYNC_H

#include <stdint.h>

#define BROKEN_EVENT 999999.9f

typedef enum blend_type {
  BT_STEP = 0,
  BT_LINEAR,
  BT_SMOOTH,
  BT_RAMP
} blend_type;

typedef struct event {
  uint16_t    row;
  uint8_t     id;
  float       value;
  blend_type  type;
} event;

typedef struct track {
  event       *events;
  uint16_t    event_cnt;
  float       row_rate;
} track;

void  sync_init_track(track *track, uint16_t max_event_cnt, float bpm, float rows_per_beat);
void  sync_release_track(track *track);

void  sync_add_event(track *t, uint16_t row, uint8_t id, float value, blend_type type);
float sync_get_value(const track *t, uint8_t id, float time);

#endif
