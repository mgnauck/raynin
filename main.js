// Sync track
const PLAY_SYNC_TRACK = true;
const LOOP_SYNC_TRACK = true;
const START_AT_SEQUENCE = 10;
const TRACK = [
  // Row, ev id, value, blend mode (0 = STEP, 1 = LINEAR, 2 = SMOOTH, 3 = RAMP)

 // ************************ SCENE 0 ************************

 0,    0,        0,  0, // SCN_ID

 //KEY1
 0,    5,   10.988,  1, // CAM_POS_X
 0,    6,   14.184,  1, // CAM_POS_Y
 0,    7,  -48.448,  1, // CAM_POS_Z
 0,    8,    0.296,  1, // CAM_DIR_X
 0,    9,    0.000,  1, // CAM_DIR_Y
 0,   10,   -0.955,  1, // CAM_DIR_Z

 //KEY2

 127,    5,   -4.512,  0, // CAM_POS_X
 127,    6,   13.760,  0, // CAM_POS_Y
 127,    7,  -46.087,  0, // CAM_POS_Z
 127,    8,   -0.276,  0, // CAM_DIR_X
 127,    9,   -0.015,  0, // CAM_DIR_Y
 127,   10,   -0.961,  0, // CAM_DIR_Z

 // ************************ SCENE 1 ************************

 128,    0,        1,  0, // SCN_ID

 //KEY1
 128,    5,   10.994,  1, // CAM_POS_X
 128,    6,   16.431,  1, // CAM_POS_Y
 128,    7,   56.957,  1, // CAM_POS_Z
 128,    8,    0.010,  1, // CAM_DIR_X
 128,    9,   -0.111,  1, // CAM_DIR_Y
 128,   10,    0.994,  1, // CAM_DIR_Z

 128,   11,   40.312,  0, // CAM_FOV

 //KEY2
 166,    5,   10.671,  0, // CAM_POS_X
 166,    6,   17.351,  0, // CAM_POS_Y
 166,    7,   42.533,  0, // CAM_POS_Z
 166,    8,    0.020,  0, // CAM_DIR_X
 166,    9,   -0.051,  0, // CAM_DIR_Y
 166,   10,    0.998,  0, // CAM_DIR_Z

 //KEY1
 167,    5,  -10.177,  1, // CAM_POS_X
 167,    6,   21.400,  1, // CAM_POS_Y
 167,    7,   35.347,  1, // CAM_POS_Z
 167,    8,    0.029,  1, // CAM_DIR_X
 167,    9,    0.037,  1, // CAM_DIR_Y
 167,   10,    0.999,  1, // CAM_DIR_Z

 //KEY2
 215,    5,    14.275,  0, // CAM_POS_X
 215,    6,   21.400,  0, // CAM_POS_Y
 215,    7,   34.925,  0, // CAM_POS_Z
 215,    8,    0.014,  0, // CAM_DIR_X
 215,    9,    0.032,  0, // CAM_DIR_Y
 215,   10,    0.999,  0, // CAM_DIR_Z

 //KEY1
 216,    5,   28.961,  1, // CAM_POS_X
 216,    6,   16.155,  1, // CAM_POS_Y
 216,    7,  -37.911,  1, // CAM_POS_Z
 216,    8,    0.070,  1, // CAM_DIR_X
 216,    9,   -0.016,  1, // CAM_DIR_Y
 216,   10,   -0.997,  1, // CAM_DIR_Z

 //KEY2
 277,    5,   14.539,  0, // CAM_POS_X
 277,    6,   16.155,  0, // CAM_POS_Y
 277,    7,  -38.927,  0, // CAM_POS_Z
 277,    8,    0.070,  0, // CAM_DIR_X
 277,    9,   -0.011,  0, // CAM_DIR_Y
 277,   10,   -0.997,  0, // CAM_DIR_Z

//KEY1
 278,    5,   27.724,  1, // CAM_POS_X
 278,    6,   18.304,  1, // CAM_POS_Y
 278,    7,  -38.456,  1, // CAM_POS_Z
 278,    8,    0.754,  1, // CAM_DIR_X
 278,    9,   -0.001,  1, // CAM_DIR_Y
 278,   10,   -0.657,  1, // CAM_DIR_Z

 //KEY2
 319,    5,   11.352,  0, // CAM_POS_X
 319,    6,   18.043,  0, // CAM_POS_Y
 319,    7,  -24.237,  0, // CAM_POS_Z
 319,    8,    0.757,  0, // CAM_DIR_X
 319,    9,    0.004,  0, // CAM_DIR_Y
 319,   10,   -0.653,  0, // CAM_DIR_Z

 // ************************ SCENE 2 ************************

 320,    0,        2,  0, // SCN_ID

 //KEY1
 320,    5,  -45.331,  2, // CAM_POS_X
 320,    6,   28.083,  2, // CAM_POS_Y
 320,    7,   58.093,  2, // CAM_POS_Z
 320,    8,   -0.573,  2, // CAM_DIR_X
 320,    9,    0.009,  2, // CAM_DIR_Y
 320,   10,    0.819,  2, // CAM_DIR_Z

 //KEY2
 383,    5,   19.595,  0, // CAM_POS_X
 383,    6,   28.083,  0, // CAM_POS_Y
 383,    7,   63.149,  0, // CAM_POS_Z
 383,    8,    0.398,  0, // CAM_DIR_X
 383,    9,   -0.001,  0, // CAM_DIR_Y
 383,   10,    0.917,  0, // CAM_DIR_Z

 //KEY1
 384,    5,  -17.423,  1, // CAM_POS_X
 384,    6,   18.218,  1, // CAM_POS_Y
 384,    7,   27.724,  1, // CAM_POS_Z
 384,    8,   -0.016,  1, // CAM_DIR_X
 384,    9,    0.014,  1, // CAM_DIR_Y
 384,   10,    1.000,  1, // CAM_DIR_Z

 //KEY2
 447,    5,    3.920,  0, // CAM_POS_X
 447,    6,   31.224,  0, // CAM_POS_Y
 447,    7,   34.421,  0, // CAM_POS_Z
 447,    8,   -0.026,  0, // CAM_DIR_X
 447,    9,    0.004,  0, // CAM_DIR_Y
 447,   10,    1.000,  0, // CAM_DIR_Z

 //KEY1
 448,    5,   -9.801,  1, // CAM_POS_X
 448,    6,   45.464,  1, // CAM_POS_Y
 448,    7,   33.397,  1, // CAM_POS_Z
 448,    8,    0.024,  1, // CAM_DIR_X
 448,    9,   -0.011,  1, // CAM_DIR_Y
 448,   10,    1.000,  1, // CAM_DIR_Z


 //KEY2
 511,    5,   -9.805,  0, // CAM_POS_X
 511,    6,   22.872,  0, // CAM_POS_Y
 511,    7,   29.819,  0, // CAM_POS_Z
 511,    8,   -0.006,  0, // CAM_DIR_X
 511,    9,   -0.001,  0, // CAM_DIR_Y
 511,   10,    1.000,  0, // CAM_DIR_Z

 // ************************ SCENE 3 ************************

 512,    0,        3,  0, // SCN_ID

//KEY1
 512,    5,  -37.843,  2, // CAM_POS_X
 512,    6,   12.764,  2, // CAM_POS_Y
 512,    7,    5.028,  2, // CAM_POS_Z
 512,    8,   -0.999,  2, // CAM_DIR_X
 512,    9,   -0.046,  2, // CAM_DIR_Y
 512,   10,   -0.000,  2, // CAM_DIR_Z
 512,   11,   40.312,  2, // CAM_FOV

//KEY2

 575,    5,  -59.738,  0, // CAM_POS_X
 575,    6,   13.278,  0, // CAM_POS_Y
 575,    7,    4.701,  0, // CAM_POS_Z
 575,    8,   -0.997,  0, // CAM_DIR_X
 575,    9,   -0.071,  0, // CAM_DIR_Y
 575,   10,    0.005,  0, // CAM_DIR_Z


//KEY1
 576,    5,  -55.237,  1, // CAM_POS_X
 576,    6,   17.657,  1, // CAM_POS_Y
 576,    7,    8.402,  1, // CAM_POS_Z
 576,    8,   -1.000,  1, // CAM_DIR_X
 576,    9,    0.004,  1, // CAM_DIR_Y
 576,   10,   -0.005,  1, // CAM_DIR_Z

//KEY2

 607,    5,  -51.039,  0, // CAM_POS_X
 607,    6,   17.114,  0, // CAM_POS_Y
 607,    7,   44.244,  0, // CAM_POS_Z
 607,    8,   -0.811,  0, // CAM_DIR_X
 607,    9,   -0.036,  0, // CAM_DIR_Y
 607,   10,    0.585,  0, // CAM_DIR_Z

 //KEY1

 608,    5,  -23.292,  1, // CAM_POS_X
 608,    6,   16.116,  1, // CAM_POS_Y
 608,    7,   16.099,  1, // CAM_POS_Z
 608,    8,   -0.850,  1, // CAM_DIR_X
 608,    9,   -0.066,  1, // CAM_DIR_Y
 608,   10,    0.523,  1, // CAM_DIR_Z

//KEY2

 639,    5,  -41.778,  0, // CAM_POS_X
 639,    6,   14.675,  0, // CAM_POS_Y
 639,    7,   27.475,  0, // CAM_POS_Z
 639,    8,   -0.850,  0, // CAM_DIR_X
 639,    9,   -0.066,  0, // CAM_DIR_Y
 639,   10,    0.523,  0, // CAM_DIR_Z

 // ************************ SCENE 4 ************************

 640,    0,        4,  0, // SCN_ID

 //KEY1
 640,    5,  -26.537,  1, // CAM_POS_X
 640,    6,   19.665,  1, // CAM_POS_Y
 640,    7,  -17.087,  1, // CAM_POS_Z
 640,    8,   -1.000,  1, // CAM_DIR_X
 640,    9,    0.024,  1, // CAM_DIR_Y
 640,   10,    0.017,  1, // CAM_DIR_Z
 
  //KEY2
 703,    5,  -26.017,  0, // CAM_POS_X
 703,    6,   19.665,  0, // CAM_POS_Y
 703,    7,   19.918,  0, // CAM_POS_Z
 703,    8,   -1.000,  0, // CAM_DIR_X
 703,    9,    0.009,  0, // CAM_DIR_Y
 703,   10,    0.017,  0, // CAM_DIR_Z
   
//KEY1

 704,    5,  -11.548,  2, // CAM_POS_X
 704,    6,   18.320,  2, // CAM_POS_Y
 704,    7,   -1.421,  2, // CAM_POS_Z
 704,    8,   -1.000,  2, // CAM_DIR_X
 704,    9,    0.024,  2, // CAM_DIR_Y
 704,   10,    0.012,  2, // CAM_DIR_Z

//KEY2

 767,    5,  -25.421,  0, // CAM_POS_X
 767,    6,   18.371,  0, // CAM_POS_Y
 767,    7,   -1.255,  0, // CAM_POS_Z
 767,    8,   -1.000,  0, // CAM_DIR_X
 767,    9,   -0.026,  0, // CAM_DIR_Y
 767,   10,    0.012,  0, // CAM_DIR_Z

//KEY1

 768,    5,  -11.029,  1, // CAM_POS_X
 768,    6,   38.872,  1, // CAM_POS_Y
 768,    7,    4.589,  1, // CAM_POS_Z
 768,    8,   -0.395,  1, // CAM_DIR_X
 768,    9,    0.884,  1, // CAM_DIR_Y
 768,   10,    0.251,  1, // CAM_DIR_Z

//KEY2

 831,    5,   -7.775,  0, // CAM_POS_X
 831,    6,   40.904,  0, // CAM_POS_Y
 831,    7,    1.852,  0, // CAM_POS_Z
 831,    8,   -0.416,  0, // CAM_DIR_X
 831,    9,    0.891,  0, // CAM_DIR_Y
 831,   10,   -0.183,  0, // CAM_DIR_Z

// ************************ SCENE 5 ************************

 832,    0,        5,  0, // SCN_ID

//KEY1

 832,    5,  -16.772,  2, // CAM_POS_X
 832,    6,   31.605,  2, // CAM_POS_Y
 832,    7,   30.411,  2, // CAM_POS_Z
 832,    8,    0.257,  2, // CAM_DIR_X
 832,    9,   -0.137,  2, // CAM_DIR_Y
 832,   10,    0.957,  2, // CAM_DIR_Z


//KEY2

 855,    5,   -6.728,  0, // CAM_POS_X
 855,    6,   30.142,  0, // CAM_POS_Y
 855,    7,   28.239,  0, // CAM_POS_Z
 855,    8,    0.047,  0, // CAM_DIR_X
 855,    9,   -0.015,  0, // CAM_DIR_Y
 855,   10,    0.999,  0, // CAM_DIR_Z

//KEY1

 856,    5,    5.672,  1, // CAM_POS_X
 856,    6,   25.886,  1, // CAM_POS_Y
 856,    7,   32.309,  1, // CAM_POS_Z
 856,    8,   -0.003,  1, // CAM_DIR_X
 856,    9,    0.872,  1, // CAM_DIR_Y
 856,   10,    0.490,  1, // CAM_DIR_Z
 856,   11,   40.312,  1, // CAM_FOV

//KEY2

 887,    5,   -6.322,  0, // CAM_POS_X
 887,    6,   26.809,  0, // CAM_POS_Y
 887,    7,   39.246,  0, // CAM_POS_Z
 887,    8,    0.007,  0, // CAM_DIR_X
 887,    9,    0.004,  0, // CAM_DIR_Y
 887,   10,    1.000,  0, // CAM_DIR_Z


//KEY1

 888,    5,    3.881,  2, // CAM_POS_X
 888,    6,   38.163,  2, // CAM_POS_Y
 888,    7,   32.782,  2, // CAM_POS_Z
 888,    8,   -0.004,  2, // CAM_DIR_X
 888,    9,   -0.076,  2, // CAM_DIR_Y
 888,   10,    0.997,  2, // CAM_DIR_Z
 888,   11,   40.312,  2, // CAM_FOV

 //KEY2

 959,    5,    6.274,  0, // CAM_POS_X
 959,    6,   15.891,  0, // CAM_POS_Y
 959,    7,   32.301,  0, // CAM_POS_Z
 959,    8,   -0.005,  0, // CAM_DIR_X
 959,    9,   -0.006,  0, // CAM_DIR_Y
 959,   10,    1.000,  0, // CAM_DIR_Z
 959,   11,   40.312,  0, // CAM_FOV

// ************************ SCENE 6 ************************

 960,    0,        6,  0, // SCN_ID

 //KEY1

 960,    5,   33.345,  1, // CAM_POS_X
 960,    6,   17.545,  1, // CAM_POS_Y
 960,    7,   38.925,  1, // CAM_POS_Z
 960,    8,    0.710,  1, // CAM_DIR_X
 960,    9,    0.014,  1, // CAM_DIR_Y
 960,   10,    0.704,  1, // CAM_DIR_Z

 //KEY2

 991,    5,   16.113,  0, // CAM_POS_X
 991,    6,   17.212,  0, // CAM_POS_Y
 991,    7,   21.850,  0, // CAM_POS_Z
 991,    8,    0.710,  0, // CAM_DIR_X
 991,    9,    0.014,  0, // CAM_DIR_Y
 991,   10,    0.704,  0, // CAM_DIR_Z

 //KEY1
 992,    5,   -4.692,  1, // CAM_POS_X
 992,    6,   27.996,  1, // CAM_POS_Y
 992,    7,   36.042,  1, // CAM_POS_Z
 992,    8,    0.421,  1, // CAM_DIR_X
 992,    9,    0.029,  1, // CAM_DIR_Y
 992,   10,    0.907,  1, // CAM_DIR_Z

 //KEY2
 1023,    5,   18.888,  0, // CAM_POS_X
 1023,    6,   23.374,  0, // CAM_POS_Y
 1023,    7,   15.482,  0, // CAM_POS_Z
 1023,    8,    0.920,  0, // CAM_DIR_X
 1023,    9,   -0.026,  0, // CAM_DIR_Y
 1023,   10,    0.390,  0, // CAM_DIR_Z

 //KEY1

 1024,    5,   32.334,  1, // CAM_POS_X
 1024,    6,   12.865,  1, // CAM_POS_Y
 1024,    7,   18.378,  1, // CAM_POS_Z
 1024,    8,    0.757,  1, // CAM_DIR_X
 1024,    9,    0.014,  1, // CAM_DIR_Y
 1024,   10,    0.654,  1, // CAM_DIR_Z

 //KEY2

 1055,    5,    8.543,  0, // CAM_POS_X
 1055,    6,   14.804,  0, // CAM_POS_Y
 1055,    7,   31.187,  0, // CAM_POS_Z
 1055,    8,    0.401,  0, // CAM_DIR_X
 1055,    9,   -0.046,  0, // CAM_DIR_Y
 1055,   10,    0.915,  0, // CAM_DIR_Z

 //KEY1

 1056,    5,   18.717,  1, // CAM_POS_X
 1056,    6,   14.190,  1, // CAM_POS_Y
 1056,    7,    8.627,  1, // CAM_POS_Z
 1056,    8,    0.720,  1, // CAM_DIR_X
 1056,    9,    0.024,  1, // CAM_DIR_Y
 1056,   10,    0.693,  1, // CAM_DIR_Z

 1087,    5,   30.952,  0, // CAM_POS_X
 1087,    6,   14.597,  0, // CAM_POS_Y
 1087,    7,   20.400,  0, // CAM_POS_Z
 1087,    8,    0.720,  0, // CAM_DIR_X
 1087,    9,    0.024,  0, // CAM_DIR_Y
 1087,   10,    0.693,  0, // CAM_DIR_Z

// ************************ SCENE 7 ************************

 1088,    0,        7,  0, // SCN_ID

 //KEY1

 1088,    5,   36.519,  1, // CAM_POS_X
 1088,    6,   -6.343,  1, // CAM_POS_Y
 1088,    7, -239.273,  1, // CAM_POS_Z
 1088,    8,    0.052,  1, // CAM_DIR_X
 1088,    9,   -0.061,  1, // CAM_DIR_Y
 1088,   10,   -0.997,  1, // CAM_DIR_Z

 //KEY2

 1119,    5,  -85.862,  0, // CAM_POS_X
 1119,    6,    1.574,  0, // CAM_POS_Y
 1119,    7, -206.606,  0, // CAM_POS_Z
 1119,    8,   -0.383,  0, // CAM_DIR_X
 1119,    9,    0.004,  0, // CAM_DIR_Y
 1119,   10,   -0.924,  0, // CAM_DIR_Z

 //KEY1

 1120,    5,  -51.341,  1, // CAM_POS_X
 1120,    6,   -2.395,  1, // CAM_POS_Y
 1120,    7, -106.548,  1, // CAM_POS_Z
 1120,    8,   -0.402,  1, // CAM_DIR_X
 1120,    9,    0.094,  1, // CAM_DIR_Y
 1120,   10,   -0.911,  1, // CAM_DIR_Z

 //KEY2

 1151,    5, -116.386,  0, // CAM_POS_X
 1151,    6,    8.604,  0, // CAM_POS_Y
 1151,    7, -237.314,  0, // CAM_POS_Z
 1151,    8,   -0.436,  0, // CAM_DIR_X
 1151,    9,   -0.011,  0, // CAM_DIR_Y
 1151,   10,   -0.900,  0, // CAM_DIR_Z

 //KEY1

 1152,    5,  105.441,  1, // CAM_POS_X
 1152,    6,  127.185,  1, // CAM_POS_Y
 1152,    7, -113.586,  1, // CAM_POS_Z
 1152,    8,   -0.316,  1, // CAM_DIR_X
 1152,    9,   -0.011,  1, // CAM_DIR_Y
 1152,   10,   -0.949,  1, // CAM_DIR_Z

 //KEY2

 1183,    5,  105.441,  0, // CAM_POS_X
 1183,    6,  127.185,  0, // CAM_POS_Y
 1183,    7, -113.586,  0, // CAM_POS_Z
 1183,    8,    0.530,  0, // CAM_DIR_X
 1183,    9,    0.039,  0, // CAM_DIR_Y
 1183,   10,   -0.847,  0, // CAM_DIR_Z

 //KEY1
 1184,    5, -135.238,  1, // CAM_POS_X
 1184,    6,    1.997,  1, // CAM_POS_Y
 1184,    7, -304.734,  1, // CAM_POS_Z
 1184,    8,   -0.401,  1, // CAM_DIR_X
 1184,    9,   -0.006,  1, // CAM_DIR_Y
 1184,   10,   -0.916,  1, // CAM_DIR_Z

 //KEY2
 1215,    5,  -67.322,  0, // CAM_POS_X
 1215,    6,    3.063,  0, // CAM_POS_Y
 1215,    7, -149.613,  0, // CAM_POS_Z
 1215,    8,   -0.401,  0, // CAM_DIR_X
 1215,    9,   -0.006,  0, // CAM_DIR_Y
 1215,   10,   -0.916,  0, // CAM_DIR_Z


// ************************ SCENE 8 ************************

 1216,    0,        8,  0, // SCN_ID

//KEY1
 1216,    5,  -23.528,  1, // CAM_POS_X
 1216,    6,   19.035,  1, // CAM_POS_Y
 1216,    7,   25.824,  1, // CAM_POS_Z
 1216,    8,   -0.327,  1, // CAM_DIR_X
 1216,    9,   -0.287,  1, // CAM_DIR_Y
 1216,   10,    0.900,  1, // CAM_DIR_Z

//KEY2

 1247,    5,  -40.726,  0, // CAM_POS_X
 1247,    6,   20.186,  0, // CAM_POS_Y
 1247,    7,   48.517,  0, // CAM_POS_Z
 1247,    8,   -0.623,  0, // CAM_DIR_X
 1247,    9,   -0.051,  0, // CAM_DIR_Y
 1247,   10,    0.781,  0, // CAM_DIR_Z

 //KEY1

 1248,    5,   24.647,  1, // CAM_POS_X
 1248,    6,   12.788,  1, // CAM_POS_Y
 1248,    7,   37.101,  1, // CAM_POS_Z
 1248,    8,    0.634,  1, // CAM_DIR_X
 1248,    9,   -0.296,  1, // CAM_DIR_Y
 1248,   10,    0.714,  1, // CAM_DIR_Z

 //KEY2

 1287,    5,   12.360,  0, // CAM_POS_X
 1287,    6,   12.788,  0, // CAM_POS_Y
 1287,    7,   44.742,  0, // CAM_POS_Z
 1287,    8,    0.050,  0, // CAM_DIR_X
 1287,    9,   -0.239,  0, // CAM_DIR_Y
 1287,   10,    0.970,  0, // CAM_DIR_Z

 //KEY1
 1288,    5,   -4.088,  1, // CAM_POS_X
 1288,    6,   27.014,  1, // CAM_POS_Y
 1288,    7,   82.637,  1, // CAM_POS_Z
 1288,    8,    0.006,  1, // CAM_DIR_X
 1288,    9,   -0.021,  1, // CAM_DIR_Y
 1288,   10,    1.000,  1, // CAM_DIR_Z

//KEY2

 1343,    5,   -4.070,  0, // CAM_POS_X
 1343,    6,   27.398,  0, // CAM_POS_Y
 1343,    7,   42.671,  0, // CAM_POS_Z
 1343,    8,   -0.009,  0, // CAM_DIR_X
 1343,    9,    0.004,  0, // CAM_DIR_Y
 1343,   10,    1.000,  0, // CAM_DIR_Z

// ************************ SCENE 9 ************************

 1344,    0,        9,  0, // SCN_ID

//KEY1

 1344,    5,  -38.184,  2, // CAM_POS_X
 1344,    6,   12.060,  2, // CAM_POS_Y
 1344,    7,   30.026,  2, // CAM_POS_Z
 1344,    8,    0.001,  2, // CAM_DIR_X
 1344,    9,   -0.006,  2, // CAM_DIR_Y
 1344,   10,    1.000,  2, // CAM_DIR_Z

 //KEY2

 1383,    5,    0.767,  0, // CAM_POS_X
 1383,    6,   15.749,  0, // CAM_POS_Y
 1383,    7,   50.676,  0, // CAM_POS_Z
 1383,    8,   -0.004,  0, // CAM_DIR_X
 1383,    9,   -0.031,  0, // CAM_DIR_Y
 1383,   10,    1.000,  0, // CAM_DIR_Z

 //KEY1

 1384,    5,  -22.373,  1, // CAM_POS_X
 1384,    6,   37.898,  1, // CAM_POS_Y
 1384,    7,   30.327,  1, // CAM_POS_Z
 1384,    8,   -0.467,  1, // CAM_DIR_X
 1384,    9,    0.616,  1, // CAM_DIR_Y
 1384,   10,    0.634,  1, // CAM_DIR_Z

//KEY2

 1399,    5,   10.820,  0, // CAM_POS_X
 1399,    6,   15.455,  0, // CAM_POS_Y
 1399,    7,   58.243,  0, // CAM_POS_Z
 1399,    8,   -0.014,  0, // CAM_DIR_X
 1399,    9,    0.009,  0, // CAM_DIR_Y
 1399,   10,    1.000,  0, // CAM_DIR_Z

 //KEY1

 1400,    5,    3.891,  2, // CAM_POS_X
 1400,    6,   17.237,  2, // CAM_POS_Y
 1400,    7,   26.060,  2, // CAM_POS_Z
 1400,    8,   -0.014,  2, // CAM_DIR_X
 1400,    9,    0.004,  2, // CAM_DIR_Y
 1400,   10,    1.000,  2, // CAM_DIR_Z
 1400,   11,   40.312,  2, // CAM_FOV

//KEY2

 1471,    5,    3.275,  0, // CAM_POS_X
 1471,    6,   18.733,  0, // CAM_POS_Y
 1471,    7,   70.029,  0, // CAM_POS_Z
 1471,    8,   -0.014,  0, // CAM_DIR_X
 1471,    9,    0.034,  0, // CAM_DIR_Y
 1471,   10,    0.999,  0, // CAM_DIR_Z

// ************************ SCENE ENDE ************************

 1472,    0,        10,  0, // SCN_ID

 //KEY1
 1472,    5,    0.613,  2, // CAM_POS_X
 1472,    6,    0.099,  2, // CAM_POS_Y
 1472,    7,    8.585,  2, // CAM_POS_Z
 1472,    8,    0.051,  2, // CAM_DIR_X
 1472,    9,   -0.001,  2, // CAM_DIR_Y
 1472,   10,    0.999,  2, // CAM_DIR_Z
 
 //KEY2
 1604,    5,    0.491,  0, // CAM_POS_X
 1604,    6,    0.101,  0, // CAM_POS_Y
 1604,    7,    6.194,  0, // CAM_POS_Z
 1604,    8,    0.051,  0, // CAM_DIR_X
 1604,    9,   -0.001,  0, // CAM_DIR_Y
 1604,   10,    0.999,  0, // CAM_DIR_Z


 //DUMMY
 2048,    5,    0.491,  0, // CAM_POS_X
 2048,    6,    0.101,  0, // CAM_POS_Y
 2048,    7,    6.194,  0, // CAM_POS_Z
 2048,    8,    0.051,  0, // CAM_DIR_X
 2048,    9,   -0.001,  0, // CAM_DIR_Y
 2048,   10,    0.999,  0, // CAM_DIR_Z

];

// Audio
const ENABLE_AUDIO = true;
const LOAD_AUDIO_FROM_FILE = true;
const AUDIO_TO_LOAD = "tunes/tune.bkpo"
const BPM = 125;
const ROWS_PER_BEAT = 4;

// Rendering
const ENABLE_RENDER = true;
const ENABLE_PRERENDER = false;
const FULLSCREEN = false;
const ASPECT = 16.0 / 7.0;
const WIDTH = 880;
const HEIGHT = Math.ceil(WIDTH / ASPECT);

// Scene loading/export
const LOAD_FROM_GLTF = true;
const PATH_TO_SCENES = "scenes/new/";
const SCENES_TO_LOAD = [
  "good_1.gltf",
  "good_2.gltf",
  "good_3.gltf",
  "good_4.gltf",
  "good_5.gltf",
  "good_6.gltf",
  "good_7.gltf",
  "good_8.gltf",
  "good_9.gltf",
  "good_10.gltf",
  "end.gltf",
];
//*/
/*const PATH_TO_SCENES = "scenes/old/";
const SCENES_TO_LOAD = [
  "bernstein.gltf",
  "big-triangle.gltf",
  "donuts.gltf",
  "layers-of-spheres.gltf",
  "only-god-forgives.gltf",
  "purple-motion.gltf",
  "red-skybox.gltf",
  "schellen.gltf",
  "sphere-grid.gltf",
  "triangle-cave.gltf",
  "yellow-blue-hangar.gltf",
  "yellow-donut-cubes.gltf",
];//*/
const EXPORT_BIN_TO_DISK = false && LOAD_FROM_GLTF;
const EXPORT_FILENAME = "scenes-export.bin";
const DO_NOT_LOAD_FROM_JS = false && !LOAD_FROM_GLTF;

const SPP = 1;
const MAX_BOUNCES = 5; // Max is 15 (encoded in bits 0-3 in frame data)

const CAM_LOOK_VELOCITY = 0.005;
const CAM_MOVE_VELOCITY = 0.1;

const WASM = `BEGIN_intro_wasm
END_intro_wasm`;

const SM_GENERATE = `BEGIN_generate_wgsl
END_generate_wgsl`;

const SM_INTERSECT = `BEGIN_intersect_wgsl
END_intersect_wgsl`;

const SM_SHADE = `BEGIN_shade_wgsl
END_shade_wgsl`;

const SM_SHADOW = `BEGIN_traceShadowRay_wgsl
END_traceShadowRay_wgsl`;

const SM_CONTROL = `BEGIN_control_wgsl
END_control_wgsl`;

const SM_DENOISE = `BEGIN_denoise_wgsl
END_denoise_wgsl`;

const SM_BLIT = `BEGIN_blit_wgsl
END_blit_wgsl`;

const BUF_CAM = 0; // Accessed from WASM
const BUF_MTL = 1; // Accessed from WASM
const BUF_INST = 2; // Accessed from WASM
const BUF_TRI = 3; // Accessed from WASM
const BUF_TNRM = 4; // Accessed from WASM
const BUF_LTRI = 5; // Accessed from WASM
const BUF_NODE = 6; // Accessed from WASM
const BUF_PATH0 = 7;
const BUF_PATH1 = 8;
const BUF_SRAY = 9;
const BUF_HIT = 10;
const BUF_ATTR = 11; // Attribute buf (pos and nrm)
const BUF_LATTR = 12; // Last attribute buf (pos and nrm)
const BUF_COL = 13; // Separated direct and indirect illumination
const BUF_MOM0 = 14; // Moments buffer (dir/indir)
const BUF_MOM1 = 15; // Moments buffer (dir/indir)
const BUF_HIS0 = 16; // History buffer (moments)
const BUF_HIS1 = 17; // History buffer (moments)
const BUF_ACC0 = 18; // Temporal accumulation buffer col + variance (dir/indir)
const BUF_ACC1 = 19; // Temporal accumulation buffer col + variance (dir/indir)
const BUF_ACC2 = 20; // Temporal accumulation buffer col + variance (dir/indir)
const BUF_CFG = 21; // Accessed from WASM
const BUF_POST = 22
const BUF_LCAM = 23;
const BUF_GRID = 24;

const PL_GENERATE = 0;
const PL_INTERSECT = 1;
const PL_SHADE = 2;
const PL_SHADOW = 3;
const PL_CONTROL0 = 4;
const PL_CONTROL1 = 5;
const PL_CONTROL2 = 6;
const PL_CONTROL3 = 7;
const PL_DENOISE0 = 8;
const PL_DENOISE1 = 9;
const PL_DENOISE2 = 10;
const PL_BLIT0 = 12;
const PL_BLIT1 = 13;

const BG_GENERATE = 0;
const BG_INTERSECT0 = 1;
const BG_INTERSECT1 = 2;
const BG_SHADE0 = 3;
const BG_SHADE1 = 4;
const BG_SHADOW = 5;
const BG_CONTROL = 6;
const BG_DENOISE0 = 7;
const BG_DENOISE1 = 8;
const BG_DENOISE2 = 9;
const BG_DENOISE3 = 10;
const BG_DENOISE4 = 11;
const BG_DENOISE5 = 12;
const BG_BLIT0 = 13;
const BG_BLIT1 = 14;
const BG_BLIT2 = 15;

const WG_SIZE_X = 16;
const WG_SIZE_Y = 16;

let canvas, context, device;
let wa, res = {};
let wasmModule;
let startTime = undefined, last;
let frames = 0;
let samples = 0;
let ltriCount = 0;
let converge = true;
let filter = true;
let reproj = filter | false;
let editMode = !PLAY_SYNC_TRACK;

function sleep(delay) {
  return new Promise((resolve) => { setTimeout(resolve, delay); } );
}

function handleMouseMoveEvent(e) {
  wa.mouse_move(e.movementX, e.movementY, CAM_LOOK_VELOCITY);
}

function installEventHandler() {
  canvas.addEventListener("click", async () => {
    if (!document.pointerLockElement)
      //await canvas.requestPointerLock({ unadjustedMovement: true });
      await canvas.requestPointerLock();
  });

  document.addEventListener("keydown",
    // Key codes do not map well to UTF-8. Use A-Z and 0-9 only. Convert A-Z to lower case.
    e => wa.key_down((e.keyCode >= 65 && e.keyCode <= 90) ? e.keyCode + 32 : e.keyCode, CAM_MOVE_VELOCITY));

  document.addEventListener("pointerlockchange", () => {
    if (document.pointerLockElement === canvas)
      canvas.addEventListener("mousemove", handleMouseMoveEvent);
    else
      canvas.removeEventListener("mousemove", handleMouseMoveEvent);
  });
}

function Wasm(module) {
  this.environment = {
    console: (level, addr, len) => {
      let s = "";
      for (let i = 0; i < len; i++)
        s += String.fromCharCode(this.memUint8[addr + i]);
      console.log(s);
    },
    sqrtf: (v) => Math.sqrt(v),
    sinf: (v) => Math.sin(v),
    cosf: (v) => Math.cos(v),
    tanf: (v) => Math.tan(v),
    asinf: (v) => Math.asin(v),
    acosf: (v) => Math.acos(v),
    atan2f: (y, x) => Math.atan2(y, x),
    powf: (b, e) => Math.pow(b, e),
    fracf: (v) => v % 1,
    atof: (addr) => {
      let s = "", c, i = 0;
      while ((c = String.fromCharCode(this.memUint8[addr + i])) != "\0") {
        s += c;
        i++;
      }
      return parseFloat(s);
    },
    // Cam, mtl, inst, tri, nrm, ltri, bvh node
    gpu_create_res: (c, m, i, t, n, l, b) => createGpuResources(c, m, i, t, n, l, b),
    gpu_write_buf: (id, ofs, addr, sz) => device.queue.writeBuffer(res.buf[id], ofs, wa.memUint8, addr, sz),
    reset_samples: () => { samples = 0; },
    set_ltri_cnt: (n) => { ltriCount = n; },
    toggle_converge: () => { converge = !converge; },
    toggle_edit: () => {
      editMode = !editMode;
      if(!editMode) {
        startTime = undefined;
        if(ENABLE_AUDIO) {
          audio.suspend();
          audio.reset(START_AT_SEQUENCE);
        }
      }
    },
    toggle_filter: () => { filter = !filter; if (filter) reproj = true; samples = 0; res.accumIdx = 0; },
    toggle_reprojection: () => { reproj = !reproj; if (!reproj) filter = false; samples = 0; res.accumIdx = 0; },
    save_binary: (ptr, size) => {
      const blob = new Blob([wa.memUint8.slice(ptr, ptr + size)], { type: "" });
      const anchor = document.createElement('a');
      anchor.href = URL.createObjectURL(blob);
      anchor.download = EXPORT_FILENAME;
      document.body.appendChild(anchor);
      anchor.click();
      document.body.removeChild(anchor);
      console.log("Downloaded exported binary scenes to " + EXPORT_FILENAME + " with " + size + " bytes");
    }
  };

  this.instantiate = async function () {
    const res = await WebAssembly.instantiate(module, { env: this.environment });
    Object.assign(this, res.instance.exports);
    this.memUint8 = new Uint8Array(this.memory.buffer);
  }
}

function deferred() {
  let res, rej;
  let p = new Promise((resolve, reject) => { res = resolve; rej = reject; });
  return { promise: p, resolve: res, reject: rej, signal: async () => res() };
}

function Audio(module) {
  this.module = module;
  this.initEvent = deferred();
  this.startTime = 0;
  this.playTime = 0;

  this.initialize = async function (sequence, file) {
    console.log("Audio: Initialize...");

    this.audioContext = new AudioContext;
    //await this.audioContext.resume();

    // Add audio processor
    // TODO add embedded handling
    // Load audio worklet script from file
    await this.audioContext.audioWorklet.addModule('audio.js');

    // Defaults are in:1, out:1, channels:2
    this.audioWorklet = new AudioWorkletNode(this.audioContext, 'a', { outputChannelCount: [2] });
    this.audioWorklet.connect(this.audioContext.destination);

    // Event handling
    this.audioWorklet.port.onmessage = async (event) => {
      if ('s' in event.data) {
        // synth is ready
        console.info(`Audio: Received start time ${event.data.s.toFixed(3)}`);
        this.playTime = event.data.s;
        this.initEvent.signal();
      }
    };

    // load song from external file if specified
    if (file) {
      console.info(`Audio: Loading external tune ${file}`);
      const tuneFile = await fetch(file);
      if (!tuneFile.ok) {
        alert(`The external tune in ${file} wasn't found.`);
        return;
      }
      const tuneBuffer = await (tuneFile).arrayBuffer();

      // send wasm + tune
      this.audioWorklet.port.postMessage({ 'w': this.module, s: sequence, t: tuneBuffer });
    }
    else {
      // send wasm
      this.audioWorklet.port.postMessage({ 'w': this.module, s: sequence });
    }

    // Wait for initialization to complete
    await this.initEvent.promise;
  }

  this.currentTime = function () {
    return ((performance.now() - this.startTime) / 1000) + this.playTime;
  }

  this.reset = function(sequence) {
    //console.log(`Audio: Reset to sequence ${sequence}`);
    this.audioWorklet.port.postMessage({ r: sequence });

    // Reset timer (TODO timer drift compensation)
    this.startTime = performance.now();
  }

  this.suspend = async function() {
    console.log(`Audio: Suspend`);
    await this.audioContext.suspend();
    this.audioWorklet.port.postMessage({ p: false });
  }

  this.play = async function () {
    console.log(`Audio: Play.`);

    // Send message to start rendering
    await this.audioContext.resume();
    this.audioWorklet.port.postMessage({ p: true });

    // Reset timer (TODO timer drift compensation)
    this.startTime = performance.now();

    console.log(`Audio: Time is ${this.currentTime().toFixed(2)}`);
  }
}

function createGpuResources(camSz, mtlSz, instSz, triSz, nrmSz, ltriSz, nodeSz) {
  res.buf = [];
  res.bindGroups = [];
  res.pipelineLayouts = [];
  res.pipelines = [];
  res.accumIdx = 0;

  // Buffers

  // No mesh in scene, keep min size buffers, for proper mapping to our layout/shader
  triSz = triSz == 0 ? 48 : triSz;
  nrmSz = nrmSz == 0 ? 48 : nrmSz;
  ltriSz = ltriSz == 0 ? 64 : ltriSz;
  nodeSz = nodeSz == 0 ? 64 : nodeSz;

  res.buf[BUF_CAM] = device.createBuffer({
    size: camSz,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST | GPUBufferUsage.COPY_SRC
  });

  res.buf[BUF_MTL] = device.createBuffer({
    size: mtlSz,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_INST] = device.createBuffer({
    size: instSz,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_TRI] = device.createBuffer({
    size: triSz,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_TNRM] = device.createBuffer({
    size: nrmSz,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_LTRI] = device.createBuffer({
    size: ltriSz,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_NODE] = device.createBuffer({
    size: nodeSz,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_PATH0] = device.createBuffer({
    size: WIDTH * HEIGHT * 12 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_PATH1] = device.createBuffer({
    size: WIDTH * HEIGHT * 12 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_SRAY] = device.createBuffer({
    size: WIDTH * HEIGHT * 12 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_HIT] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_ATTR] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4 * 2,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC
  });

  res.buf[BUF_LATTR] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4 * 2,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_COL] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4 * 2,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_MOM0] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_MOM1] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_HIS0] = device.createBuffer({
    size: WIDTH * HEIGHT * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_HIS1] = device.createBuffer({
    size: WIDTH * HEIGHT * 4,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_ACC0] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4 * 3,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_ACC1] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4 * 3,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_ACC2] = device.createBuffer({
    size: WIDTH * HEIGHT * 4 * 4 * 3,
    usage: GPUBufferUsage.STORAGE
  });

  res.buf[BUF_CFG] = device.createBuffer({
    size: 16 * 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST | GPUBufferUsage.COPY_SRC
  });

  res.buf[BUF_POST] = device.createBuffer({
    size: 4 * 4,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_LCAM] = device.createBuffer({
    size: camSz,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
  });

  res.buf[BUF_GRID] = device.createBuffer({
    size: 8 * 4,
    usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.INDIRECT
  });

  // Bind groups and pipelines

  // Generate
  let bindGroupLayout = device.createBindGroupLayout({
    entries: [
      { binding: 0, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 1, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 2, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
    ]
  });

  res.bindGroups[BG_GENERATE] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_CAM] } },
      { binding: 1, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 2, resource: { buffer: res.buf[BUF_PATH0] } },
    ]
  });

  res.pipelineLayouts[PL_GENERATE] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

  // Intersect
  bindGroupLayout = device.createBindGroupLayout({
    entries: [
      { binding: 0, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 1, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 2, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 3, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 4, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 5, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
    ]
  });

  res.bindGroups[BG_INTERSECT0] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_INST] } },
      { binding: 1, resource: { buffer: res.buf[BUF_TRI] } },
      { binding: 2, resource: { buffer: res.buf[BUF_NODE] } },
      { binding: 3, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 4, resource: { buffer: res.buf[BUF_PATH0] } },
      { binding: 5, resource: { buffer: res.buf[BUF_HIT] } },
    ]
  });

  res.bindGroups[BG_INTERSECT1] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_INST] } },
      { binding: 1, resource: { buffer: res.buf[BUF_TRI] } },
      { binding: 2, resource: { buffer: res.buf[BUF_NODE] } },
      { binding: 3, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 4, resource: { buffer: res.buf[BUF_PATH1] } },
      { binding: 5, resource: { buffer: res.buf[BUF_HIT] } },
    ]
  });

  res.pipelineLayouts[PL_INTERSECT] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

  // Shade
  bindGroupLayout = device.createBindGroupLayout({
    entries: [
      { binding: 0, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 1, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 2, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 3, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 4, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 5, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 6, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
      { binding: 7, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
      { binding: 8, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
      { binding: 9, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
      { binding: 10, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
    ]
  });

  res.bindGroups[BG_SHADE0] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_MTL] } },
      { binding: 1, resource: { buffer: res.buf[BUF_INST] } },
      { binding: 2, resource: { buffer: res.buf[BUF_TNRM] } },
      { binding: 3, resource: { buffer: res.buf[BUF_LTRI] } },
      { binding: 4, resource: { buffer: res.buf[BUF_HIT] } },
      { binding: 5, resource: { buffer: res.buf[BUF_PATH0] } }, // in
      { binding: 6, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 7, resource: { buffer: res.buf[BUF_PATH1] } }, // out
      { binding: 8, resource: { buffer: res.buf[BUF_SRAY] } },
      { binding: 9, resource: { buffer: res.buf[BUF_ATTR] } },
      { binding: 10, resource: { buffer: res.buf[BUF_COL] } },
    ]
  });

  res.bindGroups[BG_SHADE1] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_MTL] } },
      { binding: 1, resource: { buffer: res.buf[BUF_INST] } },
      { binding: 2, resource: { buffer: res.buf[BUF_TNRM] } },
      { binding: 3, resource: { buffer: res.buf[BUF_LTRI] } },
      { binding: 4, resource: { buffer: res.buf[BUF_HIT] } },
      { binding: 5, resource: { buffer: res.buf[BUF_PATH1] } }, // in
      { binding: 6, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 7, resource: { buffer: res.buf[BUF_PATH0] } }, // out
      { binding: 8, resource: { buffer: res.buf[BUF_SRAY] } },
      { binding: 9, resource: { buffer: res.buf[BUF_ATTR] } },
      { binding: 10, resource: { buffer: res.buf[BUF_COL] } },
    ]
  });

  res.pipelineLayouts[PL_SHADE] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

  // Shadow
  bindGroupLayout = device.createBindGroupLayout({
    entries: [
      { binding: 0, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 1, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 2, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 3, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 4, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 5, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
    ]
  });

  res.bindGroups[BG_SHADOW] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_INST] } },
      { binding: 1, resource: { buffer: res.buf[BUF_TRI] } },
      { binding: 2, resource: { buffer: res.buf[BUF_NODE] } },
      { binding: 3, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 4, resource: { buffer: res.buf[BUF_SRAY] } },
      { binding: 5, resource: { buffer: res.buf[BUF_COL] } },
    ]
  });

  res.pipelineLayouts[PL_SHADOW] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

  // Control
  bindGroupLayout = device.createBindGroupLayout({
    entries: [
      { binding: 0, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
    ]
  });

  res.bindGroups[BG_CONTROL] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_CFG] } },
    ]
  });

  res.pipelineLayouts[PL_CONTROL0] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });
  res.pipelineLayouts[PL_CONTROL1] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });
  res.pipelineLayouts[PL_CONTROL2] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });
  res.pipelineLayouts[PL_CONTROL3] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

  // Denoise
  bindGroupLayout = device.createBindGroupLayout({
    entries: [
      { binding: 0, visibility: GPUShaderStage.COMPUTE, buffer: { type: "uniform" } },
      { binding: 1, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 2, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 3, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 4, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 5, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } }, // Mom in
      { binding: 6, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } }, // Mom out
      { binding: 7, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } }, // His in
      { binding: 8, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } }, // His out
      { binding: 9, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } }, // Col/Var in
      { binding: 10, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } }, // Col/Var out
    ]
  });

  res.bindGroups[BG_DENOISE0] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_LCAM] } },
      { binding: 1, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 2, resource: { buffer: res.buf[BUF_ATTR] } },
      { binding: 3, resource: { buffer: res.buf[BUF_LATTR] } },
      { binding: 4, resource: { buffer: res.buf[BUF_COL] } },
      { binding: 5, resource: { buffer: res.buf[BUF_MOM0] } }, // in
      { binding: 6, resource: { buffer: res.buf[BUF_MOM1] } }, // out
      { binding: 7, resource: { buffer: res.buf[BUF_HIS0] } }, // in
      { binding: 8, resource: { buffer: res.buf[BUF_HIS1] } }, // out
      { binding: 9, resource: { buffer: res.buf[BUF_ACC0] } }, // in
      { binding: 10, resource: { buffer: res.buf[BUF_ACC1] } }, // out
    ]
  });

  res.bindGroups[BG_DENOISE1] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_LCAM] } },
      { binding: 1, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 2, resource: { buffer: res.buf[BUF_ATTR] } },
      { binding: 3, resource: { buffer: res.buf[BUF_LATTR] } },
      { binding: 4, resource: { buffer: res.buf[BUF_COL] } },
      { binding: 5, resource: { buffer: res.buf[BUF_MOM1] } }, // in
      { binding: 6, resource: { buffer: res.buf[BUF_MOM0] } }, // out
      { binding: 7, resource: { buffer: res.buf[BUF_HIS1] } }, // in
      { binding: 8, resource: { buffer: res.buf[BUF_HIS0] } }, // out
      { binding: 9, resource: { buffer: res.buf[BUF_ACC1] } }, // in
      { binding: 10, resource: { buffer: res.buf[BUF_ACC0] } }, // out
    ]
  });

  res.bindGroups[BG_DENOISE2] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_LCAM] } },
      { binding: 1, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 2, resource: { buffer: res.buf[BUF_ATTR] } },
      { binding: 3, resource: { buffer: res.buf[BUF_LATTR] } },
      { binding: 4, resource: { buffer: res.buf[BUF_COL] } },
      { binding: 5, resource: { buffer: res.buf[BUF_MOM0] } }, // Not used
      { binding: 6, resource: { buffer: res.buf[BUF_MOM1] } }, // Not used
      { binding: 7, resource: { buffer: res.buf[BUF_HIS0] } }, // Not used
      { binding: 8, resource: { buffer: res.buf[BUF_HIS1] } }, // Not used
      { binding: 9, resource: { buffer: res.buf[BUF_ACC1] } }, // in
      { binding: 10, resource: { buffer: res.buf[BUF_ACC2] } }, // out
    ]
  });

  res.bindGroups[BG_DENOISE3] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_LCAM] } },
      { binding: 1, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 2, resource: { buffer: res.buf[BUF_ATTR] } },
      { binding: 3, resource: { buffer: res.buf[BUF_LATTR] } },
      { binding: 4, resource: { buffer: res.buf[BUF_COL] } },
      { binding: 5, resource: { buffer: res.buf[BUF_MOM0] } }, // Not used
      { binding: 6, resource: { buffer: res.buf[BUF_MOM1] } }, // Not used
      { binding: 7, resource: { buffer: res.buf[BUF_HIS0] } }, // Not used
      { binding: 8, resource: { buffer: res.buf[BUF_HIS1] } }, // Not used
      { binding: 9, resource: { buffer: res.buf[BUF_ACC0] } }, // in
      { binding: 10, resource: { buffer: res.buf[BUF_ACC2] } }, // out
    ]
  });

  res.bindGroups[BG_DENOISE4] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_LCAM] } },
      { binding: 1, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 2, resource: { buffer: res.buf[BUF_ATTR] } },
      { binding: 3, resource: { buffer: res.buf[BUF_LATTR] } },
      { binding: 4, resource: { buffer: res.buf[BUF_COL] } },
      { binding: 5, resource: { buffer: res.buf[BUF_MOM0] } }, // Not used
      { binding: 6, resource: { buffer: res.buf[BUF_MOM1] } }, // Not used
      { binding: 7, resource: { buffer: res.buf[BUF_HIS0] } }, // Not used
      { binding: 8, resource: { buffer: res.buf[BUF_HIS1] } }, // Not used
      { binding: 9, resource: { buffer: res.buf[BUF_ACC2] } }, // in
      { binding: 10, resource: { buffer: res.buf[BUF_ACC0] } }, // out
    ]
  });

  res.bindGroups[BG_DENOISE5] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_LCAM] } },
      { binding: 1, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 2, resource: { buffer: res.buf[BUF_ATTR] } },
      { binding: 3, resource: { buffer: res.buf[BUF_LATTR] } },
      { binding: 4, resource: { buffer: res.buf[BUF_COL] } },
      { binding: 5, resource: { buffer: res.buf[BUF_MOM0] } }, // Not used
      { binding: 6, resource: { buffer: res.buf[BUF_MOM1] } }, // Not used
      { binding: 7, resource: { buffer: res.buf[BUF_HIS0] } }, // Not used
      { binding: 8, resource: { buffer: res.buf[BUF_HIS1] } }, // Not used
      { binding: 9, resource: { buffer: res.buf[BUF_ACC2] } }, // in
      { binding: 10, resource: { buffer: res.buf[BUF_ACC1] } }, // out
    ]
  });

  res.pipelineLayouts[PL_DENOISE0] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });
  res.pipelineLayouts[PL_DENOISE1] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });
  res.pipelineLayouts[PL_DENOISE2] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

  // Blit
  bindGroupLayout = device.createBindGroupLayout({
    entries: [
      { binding: 0, visibility: GPUShaderStage.FRAGMENT, buffer: { type: "uniform" } },
      { binding: 1, visibility: GPUShaderStage.FRAGMENT, buffer: { type: "read-only-storage" } },
      { binding: 2, visibility: GPUShaderStage.FRAGMENT, buffer: { type: "read-only-storage" } },
      { binding: 3, visibility: GPUShaderStage.FRAGMENT, buffer: { type: "read-only-storage" } },
    ]
  });

  res.bindGroups[BG_BLIT0] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_POST] } },
      { binding: 1, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 2, resource: { buffer: res.buf[BUF_COL] } },
      { binding: 3, resource: { buffer: res.buf[BUF_ACC0] } },
    ]
  });

  res.bindGroups[BG_BLIT1] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_POST] } },
      { binding: 1, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 2, resource: { buffer: res.buf[BUF_COL] } },
      { binding: 3, resource: { buffer: res.buf[BUF_ACC1] } },
    ]
  });

  res.bindGroups[BG_BLIT2] = device.createBindGroup({
    layout: bindGroupLayout,
    entries: [
      { binding: 0, resource: { buffer: res.buf[BUF_POST] } },
      { binding: 1, resource: { buffer: res.buf[BUF_CFG] } },
      { binding: 2, resource: { buffer: res.buf[BUF_COL] } },
      { binding: 3, resource: { buffer: res.buf[BUF_ACC2] } },
    ]
  });

  res.pipelineLayouts[PL_BLIT0] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });
  res.pipelineLayouts[PL_BLIT1] = device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] });

  // Render pass descriptor

  res.renderPassDescriptor =
  {
    colorAttachments: [
      {
        undefined, // view
        clearValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
        loadOp: "clear",
        storeOp: "store"
      }]
  };
}

function createPipeline(pipelineType, shaderCode, entryPoint0, entryPoint1) {
  let shaderModule = device.createShaderModule({ code: shaderCode });

  if (entryPoint1 === undefined)
    res.pipelines[pipelineType] = device.createComputePipeline({
      layout: res.pipelineLayouts[pipelineType],
      compute: { module: shaderModule, entryPoint: entryPoint0 }
    });
  else
    res.pipelines[pipelineType] = device.createRenderPipeline({
      layout: res.pipelineLayouts[pipelineType],
      vertex: { module: shaderModule, entryPoint: entryPoint0 },
      fragment: { module: shaderModule, entryPoint: entryPoint1, targets: [{ format: "bgra8unorm" }] },
      primitive: { topology: "triangle-strip" }
    });
}

function accumulateSample(commandEncoder) {
  // Copy path state and shadow ray buffer grid dimensions for indirect dispatch
  commandEncoder.copyBufferToBuffer(res.buf[BUF_CFG], 4 * 4, res.buf[BUF_GRID], 0, 8 * 4);

  let passEncoder = commandEncoder.beginComputePass();

  // Dispatch primary ray generation directly
  passEncoder.setBindGroup(0, res.bindGroups[BG_GENERATE]);
  passEncoder.setPipeline(res.pipelines[PL_GENERATE]);
  passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);

  let bindGroupPathState = 0;
  for (let j = 0; j < MAX_BOUNCES; j++) {

    // Intersect
    passEncoder.setBindGroup(0, res.bindGroups[BG_INTERSECT0 + bindGroupPathState]);
    passEncoder.setPipeline(res.pipelines[PL_INTERSECT]);
    passEncoder.dispatchWorkgroupsIndirect(res.buf[BUF_GRID], 0);

    // Shade
    passEncoder.setBindGroup(0, res.bindGroups[BG_SHADE0 + bindGroupPathState]);
    passEncoder.setPipeline(res.pipelines[PL_SHADE]);
    passEncoder.dispatchWorkgroupsIndirect(res.buf[BUF_GRID], 0);

    // Update frame data and workgroup grid dimensions
    passEncoder.setBindGroup(0, res.bindGroups[BG_CONTROL]);
    passEncoder.setPipeline(res.pipelines[PL_CONTROL0]);
    passEncoder.dispatchWorkgroups(1);

    passEncoder.end();

    // Copy path state and shadow ray buffer grid dimensions for indirect dispatch
    commandEncoder.copyBufferToBuffer(res.buf[BUF_CFG], 4 * 4, res.buf[BUF_GRID], 0, 8 * 4);

    passEncoder = commandEncoder.beginComputePass();

    // Trace shadow rays with offset pointing to grid dim of shadow rays
    passEncoder.setBindGroup(0, res.bindGroups[BG_SHADOW]);
    passEncoder.setPipeline(res.pipelines[PL_SHADOW]);
    passEncoder.dispatchWorkgroupsIndirect(res.buf[BUF_GRID], 4 * 4);

    // Reset shadow ray cnt. On last bounce, increase sample cnt and reset path cnt for primary ray gen.
    passEncoder.setBindGroup(0, res.bindGroups[BG_CONTROL]);
    passEncoder.setPipeline(res.pipelines[(j < MAX_BOUNCES - 1) ? PL_CONTROL1 : PL_CONTROL2]);
    passEncoder.dispatchWorkgroups(1);

    // Toggle path state buffer
    bindGroupPathState = 1 - bindGroupPathState;
  }

  passEncoder.end();
}

function reprojectAndFilter(commandEncoder) {
  let passEncoder = commandEncoder.beginComputePass();

  // Reprojection and temporal accumulation
  passEncoder.setBindGroup(0, res.bindGroups[BG_DENOISE0 + res.accumIdx]);
  passEncoder.setPipeline(res.pipelines[PL_DENOISE0]);
  passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);

  if (filter) {

    // Estimate variance
    passEncoder.setBindGroup(0, res.bindGroups[BG_DENOISE1 - res.accumIdx]);
    passEncoder.setPipeline(res.pipelines[PL_DENOISE1]);
    passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);

    // Edge-avoiding a-trous wavelet transform iterations
    // 0
    passEncoder.setBindGroup(0, res.bindGroups[BG_CONTROL]);
    passEncoder.setPipeline(res.pipelines[PL_CONTROL3]);
    passEncoder.dispatchWorkgroups(1);

    passEncoder.setBindGroup(0, res.bindGroups[BG_DENOISE0 + res.accumIdx]);
    passEncoder.setPipeline(res.pipelines[PL_DENOISE2]);
    passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);

    // 1
    passEncoder.setBindGroup(0, res.bindGroups[BG_CONTROL]);
    passEncoder.setPipeline(res.pipelines[PL_CONTROL3]);
    passEncoder.dispatchWorkgroups(1);

    passEncoder.setBindGroup(0, res.bindGroups[BG_DENOISE2 + res.accumIdx]);
    passEncoder.setPipeline(res.pipelines[PL_DENOISE2]);
    passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);

    // 2
    passEncoder.setBindGroup(0, res.bindGroups[BG_CONTROL]);
    passEncoder.setPipeline(res.pipelines[PL_CONTROL3]);
    passEncoder.dispatchWorkgroups(1);

    passEncoder.setBindGroup(0, res.bindGroups[BG_DENOISE4 + res.accumIdx]);
    passEncoder.setPipeline(res.pipelines[PL_DENOISE2]);
    passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);

    // 3
    passEncoder.setBindGroup(0, res.bindGroups[BG_CONTROL]);
    passEncoder.setPipeline(res.pipelines[PL_CONTROL3]);
    passEncoder.dispatchWorkgroups(1);

    passEncoder.setBindGroup(0, res.bindGroups[BG_DENOISE3 - res.accumIdx]);
    passEncoder.setPipeline(res.pipelines[PL_DENOISE2]);
    passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);

    /*
    // 4
    passEncoder.setBindGroup(0, res.bindGroups[BG_CONTROL]);
    passEncoder.setPipeline(res.pipelines[PL_CONTROL3]);
    passEncoder.dispatchWorkgroups(1);

    passEncoder.setBindGroup(0, res.bindGroups[BG_DENOISE4 + res.accumIdx]);
    passEncoder.setPipeline(res.pipelines[PL_DENOISE2]);
    passEncoder.dispatchWorkgroups(Math.ceil(WIDTH / 16), Math.ceil(HEIGHT / 16), 1);
    //*/
  }

  passEncoder.end();

  // Remember cam and attribute buffer
  commandEncoder.copyBufferToBuffer(res.buf[BUF_CAM], 0, res.buf[BUF_LCAM], 0, 12 * 4);
  commandEncoder.copyBufferToBuffer(res.buf[BUF_ATTR], 0, res.buf[BUF_LATTR], 0, WIDTH * HEIGHT * 4 * 4 * 2);
}

function blit(commandEncoder) {
  res.renderPassDescriptor.colorAttachments[0].view = context.getCurrentTexture().createView();

  passEncoder = commandEncoder.beginRenderPass(res.renderPassDescriptor);

  if (reproj) {
    //passEncoder.setBindGroup(0, res.bindGroups[filter ? BG_BLIT0 + res.accumIdx : BG_BLIT1 - res.accumIdx]); // 3 or 5 filter iterations
    passEncoder.setBindGroup(0, res.bindGroups[filter ? BG_BLIT2 : BG_BLIT1 - res.accumIdx]); // 2 or 4 filter iterations
    passEncoder.setPipeline(res.pipelines[PL_BLIT0]);
  } else {
    passEncoder.setBindGroup(0, res.bindGroups[BG_BLIT0]);
    passEncoder.setPipeline(res.pipelines[PL_BLIT1]);
  }
  passEncoder.draw(4);

  passEncoder.end();
}

async function render(time) {
  if (editMode || startTime === undefined)
    startTime = time;

  // FPS
  let frameTime = performance.now() - last;
  document.title = `${frameTime.toFixed(2)} / ${(1000.0 / frameTime).toFixed(2)} `;
  last = performance.now();

  // Initialize config data
  device.queue.writeBuffer(res.buf[BUF_CFG], 0,
    new Uint32Array([
      // Frame data
      (WIDTH << 16) | (ltriCount & 0xffff), (HEIGHT << 16) | (MAX_BOUNCES & 0xf), frames, samples,
      // Path state grid dimensions + w = path cnt
      Math.ceil(WIDTH / WG_SIZE_X), Math.ceil(HEIGHT / WG_SIZE_Y), 1, WIDTH * HEIGHT,
      // Shadow ray buffer grid dimensions + w = shadow ray cnt
      0, 0, 0, 0]));
  // Background color + w = ext ray cnt (is not written here, but belongs to the same buffer)

  let commandEncoder = device.createCommandEncoder();

  // Reset accumulated direct and indirect illumination
  if (samples == 0)
    commandEncoder.clearBuffer(res.buf[BUF_COL]);

  // Pathtrace
  for (let i = 0; i < SPP; i++) {
    accumulateSample(commandEncoder);
    samples++;
  }

  // SVGF
  if (reproj || filter)
    reprojectAndFilter(commandEncoder);

  // Blit to canvasa
  if (time != -303)
    blit(commandEncoder);

  device.queue.submit([commandEncoder.finish()]);

  if (time != -303) {
    // Toggle for next frame
    res.accumIdx = 1 - res.accumIdx;

    // Update scene and renderer for next frame
    let finished = wa.update((ENABLE_AUDIO && time != -303) ? audio.currentTime() : (time - startTime) / 1000, converge, !editMode);
    frames++;
  
    if(finished > 0 && !editMode && LOOP_SYNC_TRACK) {
      if(ENABLE_AUDIO)
        audio.reset(START_AT_SEQUENCE);
      frames = 0;
      startTime = undefined;
    }

    requestAnimationFrame(render);
  }
}

async function start() {
  if (FULLSCREEN)
    canvas.requestFullscreen();
  else {
    canvas.style.width = WIDTH;
    canvas.style.height = HEIGHT;
    canvas.style.position = "absolute";
    canvas.style.left = 0;
    canvas.style.top = 0;
  }

  // Initialize audio
  if (ENABLE_AUDIO) {
    audio = new Audio(wasmModule);
    if (LOAD_AUDIO_FROM_FILE)
      await audio.initialize(START_AT_SEQUENCE, AUDIO_TO_LOAD);
    else
      await audio.initialize(START_AT_SEQUENCE);
  }

  // Prepare for rendering
  wa.init_gpu_data();
  wa.update(0, false, !editMode);

  if (ENABLE_PRERENDER) {
    // Prerender to warm shaders
    for(let i=0; i<50; i++) {
      render(-303);
      startTime = undefined;
      frames = 0;
      samples = 0;
      ltriCount = 0;
      res.accumIdx = 0;
      await sleep(50);
    }
  }

  if (ENABLE_AUDIO)
    await audio.play();

  if (ENABLE_RENDER) {
    installEventHandler();
    requestAnimationFrame(render);
  }
}

async function loadSceneGltf(gltfPath, prepareForExport) {
  // Create buffers with scene gltf text and binary data
  let gltf = await (await fetch(gltfPath)).arrayBuffer();
  let gltfPtr = wa.malloc(gltf.byteLength);
  wa.memUint8.set(new Uint8Array(gltf), gltfPtr);

  let gltfBin = await (await fetch(gltfPath.replace(/\.gltf/, ".bin"))).arrayBuffer();
  let gltfBinPtr = wa.malloc(gltfBin.byteLength);
  wa.memUint8.set(new Uint8Array(gltfBin), gltfBinPtr);

  // Load scene from gltf data
  return wa.load_scene_gltf(gltfPtr, gltf.byteLength, gltfBinPtr, gltfBin.byteLength, prepareForExport) == 0;
}

async function loadSceneBin(binPath) {
  let bin = await (await fetch(binPath)).arrayBuffer();
  let binPtr = wa.malloc(bin.byteLength);
  wa.memUint8.set(new Uint8Array(bin), binPtr);
  return wa.load_scenes_bin(binPtr) == 0;
}

async function main() {
  // WebGPU
  if (!navigator.gpu) {
    alert("WebGPU is not supported on this browser.");
    return;
  }

  //const adapter = await navigator.gpu.requestAdapter();
  const adapter = await navigator.gpu.requestAdapter({ powerPreference: "low-power" });
  //const adapter = await navigator.gpu.requestAdapter({ powerPreference: "high-performance" });
  if (!adapter) {
    alert("Can not use WebGPU. No GPU adapter available.");
    return;
  }

  device = await adapter.requestDevice({
    requiredLimits: {
      maxStorageBuffersPerShaderStage: 10, maxStorageBufferBindingSize: 1073741824
    }
  });
  if (!device) {
    alert("Failed to request logical device.");
    return;
  }

  // Canvas/context
  document.body.innerHTML = "<button disabled>Loading..</button><canvas style='width:0;cursor:none'>";
  canvas = document.querySelector("canvas");
  canvas.width = WIDTH;
  canvas.height = HEIGHT;

  let presentationFormat = navigator.gpu.getPreferredCanvasFormat();
  if (presentationFormat !== "bgra8unorm") {
    alert(`Expected canvas pixel format of bgra8unorm but was '${presentationFormat}'.`);
    return;
  }

  context = canvas.getContext("webgpu");
  context.configure({ device, format: presentationFormat, alphaMode: "opaque" });

  // Load actual code
  if (WASM.includes("END_")) {
    // Load wasm from file
    wasmModule = await (await fetch("intro.wasm")).arrayBuffer();
  } else {
    // When embedded, the wasm is deflated and base64 encoded. Undo in reverse.
    wasmModule = new Blob([Uint8Array.from(atob(WASM), (m) => m.codePointAt(0))]);
    wasmNodule = await new Response(wasmModule.stream().pipeThrough(new DecompressionStream("deflate-raw"))).arrayBuffer();
  }

  wa = new Wasm(wasmModule);
  await wa.instantiate();

  // Load scenes
  if (!DO_NOT_LOAD_FROM_JS) {
    let t0 = performance.now();
    if (LOAD_FROM_GLTF) {
      for (let i = 0; i < SCENES_TO_LOAD.length; i++) {
        console.log("Trying to load scene " + PATH_TO_SCENES + SCENES_TO_LOAD[i]);
        if (!await loadSceneGltf(PATH_TO_SCENES + SCENES_TO_LOAD[i], EXPORT_BIN_TO_DISK)) {
          alert("Failed to load scene");
          return;
        }
      }
    } else {
      if (!await loadSceneBin(PATH_TO_SCENES + EXPORT_FILENAME)) {
        alert("Failed to load scenes");
        return;
      }
    }
    console.log("Loaded and generated scenes in " + ((performance.now() - t0) / 1000.0).toFixed(2) + "s");

    // Save exported scene to file via download
    if (EXPORT_BIN_TO_DISK) {
      if (wa.export_scenes() > 0) {
        alert("Failed to make a binary export of the available scenes");
        return;
      }
    }
  }

  // Init gpu resources and prepare scene
  t0 = performance.now();
  wa.init(BPM, ROWS_PER_BEAT, TRACK.length / 4);
  console.log("Initialized data and GPU in " + ((performance.now() - t0) / 1000.0).toFixed(2) + "s");

  // Provide event track to wasm
  for (let i = 0; i < TRACK.length / 4; i++) {
    let e = i << 2;
    wa.add_event(TRACK[e + 0], TRACK[e + 1], TRACK[e + 2], TRACK[e + 3]);
  }

  // Shader modules and pipelines
  if (SM_BLIT.includes("END_blit_wgsl")) {
    createPipeline(PL_GENERATE, await (await fetch("generate.wgsl")).text(), "m");
    createPipeline(PL_INTERSECT, await (await fetch("intersect.wgsl")).text(), "m");
    createPipeline(PL_SHADE, await (await fetch("shade.wgsl")).text(), "m");
    createPipeline(PL_SHADOW, await (await fetch("traceShadowRay.wgsl")).text(), "m");
    createPipeline(PL_CONTROL0, await (await fetch("control.wgsl")).text(), "m");
    createPipeline(PL_CONTROL1, await (await fetch("control.wgsl")).text(), "m1");
    createPipeline(PL_CONTROL2, await (await fetch("control.wgsl")).text(), "m2");
    createPipeline(PL_CONTROL3, await (await fetch("control.wgsl")).text(), "m3");
    createPipeline(PL_DENOISE0, await (await fetch("denoise.wgsl")).text(), "m");
    createPipeline(PL_DENOISE1, await (await fetch("denoise.wgsl")).text(), "m1");
    createPipeline(PL_DENOISE2, await (await fetch("denoise.wgsl")).text(), "m2");
    createPipeline(PL_BLIT0, await (await fetch("blit.wgsl")).text(), "vm", "m");
    createPipeline(PL_BLIT1, await (await fetch("blit.wgsl")).text(), "vm", "m1");
  } else {
    createPipeline(PL_GENERATE, SM_GENERATE, "m");
    createPipeline(PL_INTERSECT, SM_INTERSECT, "m");
    createPipeline(PL_SHADE, SM_SHADE, "m");
    createPipeline(PL_SHADOW, SM_SHADOW, "m");
    createPipeline(PL_CONTROL0, SM_CONTROL, "m");
    createPipeline(PL_CONTROL1, SM_CONTROL, "m1");
    createPipeline(PL_CONTROL2, SM_CONTROL, "m2");
    createPipeline(PL_CONTROL3, SM_CONTROL, "m3");
    createPipeline(PL_DENOISE0, SM_DENOISE, "m");
    createPipeline(PL_DENOISE1, SM_DENOISE, "m1");
    createPipeline(PL_DENOISE2, SM_DENOISE, "m2");
    createPipeline(PL_BLIT0, SM_BLIT, "vm", "m");
    createPipeline(PL_BLIT1, SM_BLIT, "vm", "m1");
  }

  // Start
  let button = document.querySelector("button");
  button.disabled = false;
  button.textContent = "Click to start";

  document.addEventListener("click", start, { once: true });
}

main();
