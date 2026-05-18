#pragma once

// Board-specific hardware definitions.
// The PLATFORM_* macro is set by platformio.ini per-env build flag.

#if defined(PLATFORM_TWATCH)
  #include "twatch_ultra.h"
  #define BOARD_DISP_W TWATCH_DISP_W
  #define BOARD_DISP_H TWATCH_DISP_H
#elif defined(PLATFORM_TDECK)
  #include "tdeck_plus.h"
  #define BOARD_DISP_W TDECK_DISP_W
  #define BOARD_DISP_H TDECK_DISP_H
#else
  #error "No board platform defined - expected PLATFORM_TDECK or PLATFORM_TWATCH"
#endif
