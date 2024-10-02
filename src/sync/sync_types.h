#ifndef SYNC_TYPES
#define SYNC_TYPES

#define SCN_ID         0 // Selected scene
#define CAM_ID         1 // Selected camera

#define CAM_EYE_X      5 // Cam eye pos (look from)
#define CAM_EYE_Y      6
#define CAM_EYE_Z      7
#define CAM_TGT_X      8 // Cam target (look at)
#define CAM_TGT_Y      9
#define CAM_TGT_Z     10

#define BG_COL_R      15 // Background color (0..1)
#define BG_COL_G      16
#define BG_COL_B      17

#define FADE_COL_R    20 // Fade color (0..1)
#define FADE_COL_G    21
#define FADE_COL_B    22
#define FADE_VAL      23 // Fade value: mix(pixel_col, fade_col, fade_value)

#define TRIGGER_0     30
#define TRIGGER_1     31
#define TRIGGER_2     32
#define TRIGGER_3     33

#endif
