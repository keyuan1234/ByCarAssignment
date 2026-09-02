#ifndef __LINE_TRACKING_H
#define __LINE_TRACKING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define LINE_TRACKING_ALL_HIGH_CODE       0x0FU

typedef enum
{
  LINE_TRACKING_DIRECTION_NONE = 0,
  LINE_TRACKING_DIRECTION_LEFT,
  LINE_TRACKING_DIRECTION_MIDDLE,
  LINE_TRACKING_DIRECTION_RIGHT,
  LINE_TRACKING_DIRECTION_SLIGHT_LEFT,
  LINE_TRACKING_DIRECTION_SLIGHT_RIGHT
} LineTrackingDirection_t;

typedef struct
{
  int8_t drive_direction;
  int8_t turn_direction;
  uint8_t turn_divisor;
  uint8_t line_detected;
} LineTrackingMotion_t;

uint8_t LineTracking_PackCode(uint8_t u1_high,
                              uint8_t u2_high,
                              uint8_t u3_high,
                              uint8_t u4_high);
LineTrackingDirection_t LineTracking_Decode(uint8_t code);
const char *LineTracking_DirectionText(LineTrackingDirection_t direction);
void LineTracking_BuildMotion(uint8_t code, LineTrackingMotion_t *motion);

#ifdef __cplusplus
}
#endif

#endif /* __LINE_TRACKING_H */
