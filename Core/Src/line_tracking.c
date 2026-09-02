#include "line_tracking.h"
#include <stddef.h>

uint8_t LineTracking_PackCode(uint8_t u1_high,
                              uint8_t u2_high,
                              uint8_t u3_high,
                              uint8_t u4_high)
{
  uint8_t code = 0U;

  if (u1_high != 0U)
  {
    code |= 0x01U;
  }
  if (u2_high != 0U)
  {
    code |= 0x02U;
  }
  if (u3_high != 0U)
  {
    code |= 0x04U;
  }
  if (u4_high != 0U)
  {
    code |= 0x08U;
  }
  return code;
}

LineTrackingDirection_t LineTracking_Decode(uint8_t code)
{
  switch (code & 0x0FU)
  {
    case 0x07U: return LINE_TRACKING_DIRECTION_LEFT;
    case 0x09U:
    case LINE_TRACKING_ALL_HIGH_CODE:
      return LINE_TRACKING_DIRECTION_MIDDLE;
    case 0x0EU: return LINE_TRACKING_DIRECTION_RIGHT;
    case 0x0BU: return LINE_TRACKING_DIRECTION_SLIGHT_LEFT;
    case 0x0DU: return LINE_TRACKING_DIRECTION_SLIGHT_RIGHT;
    default: return LINE_TRACKING_DIRECTION_NONE;
  }
}

const char *LineTracking_DirectionText(LineTrackingDirection_t direction)
{
  switch (direction)
  {
    case LINE_TRACKING_DIRECTION_LEFT: return "L";
    case LINE_TRACKING_DIRECTION_MIDDLE: return "M";
    case LINE_TRACKING_DIRECTION_RIGHT: return "R";
    case LINE_TRACKING_DIRECTION_SLIGHT_LEFT: return "SL";
    case LINE_TRACKING_DIRECTION_SLIGHT_RIGHT: return "SR";
    case LINE_TRACKING_DIRECTION_NONE:
    default: return "N";
  }
}

void LineTracking_BuildMotion(uint8_t code, LineTrackingMotion_t *motion)
{
  LineTrackingDirection_t direction;

  if (motion == NULL)
  {
    return;
  }

  motion->drive_direction = 0;
  motion->turn_direction = 0;
  motion->turn_divisor = 2U;
  motion->line_detected = 0U;
  direction = LineTracking_Decode(code);

  if (direction == LINE_TRACKING_DIRECTION_NONE)
  {
    return;
  }

  motion->drive_direction = 1;
  motion->line_detected = 1U;
  switch (direction)
  {
    case LINE_TRACKING_DIRECTION_LEFT:
      motion->turn_direction = -1;
      motion->turn_divisor = 1U;
      break;
    case LINE_TRACKING_DIRECTION_RIGHT:
      motion->turn_direction = 1;
      motion->turn_divisor = 1U;
      break;
    case LINE_TRACKING_DIRECTION_SLIGHT_LEFT:
      motion->turn_direction = -1;
      motion->turn_divisor = 3U;
      break;
    case LINE_TRACKING_DIRECTION_SLIGHT_RIGHT:
      motion->turn_direction = 1;
      motion->turn_divisor = 3U;
      break;
    case LINE_TRACKING_DIRECTION_MIDDLE:
    case LINE_TRACKING_DIRECTION_NONE:
    default:
      break;
  }
}
