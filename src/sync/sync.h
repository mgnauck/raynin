#ifndef SYNC_H
#define SYNC_H

#include <stdbool.h>
#include <stdint.h>

#include "sync_types.h"

#define EVENT_INACTIVE -999999.9f
#define EVENT_ERROR    999999.9f

typedef enum blend_type {
  BT_STEP = 0,
  BT_LINEAR,
  BT_SMOOTH,
  BT_RAMP
} blend_type;

typedef struct event {
  uint16_t row;
  uint8_t id;
  float value;
  blend_type type;
} event;

// Indexed runtime state
typedef struct event_state {
  int32_t beg_idx; // Index into track array for this event id
  int32_t end_idx;
  int32_t row_idx; // Runtime idx for this event id
} event_state;

typedef struct track {
  event *events;
  uint16_t event_cnt;
  event_state event_states[LAST_EVENT_ID]; // TODO Improve
  uint16_t max_row;
  float row_rate;
} track;

void sync_init_track(track *track, uint16_t max_event_cnt, uint16_t bpm,
                     uint8_t rows_per_beat);
void sync_release_track(track *track);

void sync_add_event(track *t, uint16_t row, uint8_t id, float value,
                    blend_type type);
void sync_index_track(track *t);

bool sync_is_finished(const track *t, float time);

bool sync_event_is_active(const track *t, uint8_t id, float time);
float sync_event_get_value(track *t, uint8_t id, float time);

#endif
