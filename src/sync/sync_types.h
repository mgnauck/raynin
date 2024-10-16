#ifndef SYNC_TYPES
#define SYNC_TYPES

// Keep the IDs tightly packed
#define SCN_ID 0 // Selected scene

#define FADE_COL_R 1 // Fade color (0..1)
#define FADE_COL_G 2
#define FADE_COL_B 3
#define FADE_VAL   4 // Fade value: mix(pixel_col, fade_col, fade_value)

#define CAM_POS_X     5 // Position
#define CAM_POS_Y     6
#define CAM_POS_Z     7
#define CAM_DIR_X     8 // Direction
#define CAM_DIR_Y     9
#define CAM_DIR_Z     10
#define CAM_FOV       11 // Field of view
#define CAM_FOC_DIST  12 // Focus distance (DOF)
#define CAM_FOC_ANGLE 13 // Focus angle (DOF)

#define BG_COL_R 15 // Background color (0..1)
#define BG_COL_G 16
#define BG_COL_B 17

#define TRIGGER_0 20
#define TRIGGER_1 21
#define TRIGGER_2 22
#define TRIGGER_3 23

// This needs to have the highest ID of above
#define LAST_EVENT_ID TRIGGER_3

#endif
