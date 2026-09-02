#include "line_tracking.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void TestBitPacking(void)
{
  assert(LineTracking_PackCode(1U, 0U, 0U, 0U) == 0x01U);
  assert(LineTracking_PackCode(0U, 1U, 0U, 0U) == 0x02U);
  assert(LineTracking_PackCode(0U, 0U, 1U, 0U) == 0x04U);
  assert(LineTracking_PackCode(0U, 0U, 0U, 1U) == 0x08U);
  assert(LineTracking_PackCode(1U, 1U, 1U, 0U) == 0x07U);
}

static void AssertMotion(uint8_t code,
                         LineTrackingDirection_t direction,
                         const char *text,
                         int8_t turn_direction,
                         uint8_t turn_divisor)
{
  LineTrackingMotion_t motion;

  assert(LineTracking_Decode(code) == direction);
  assert(strcmp(LineTracking_DirectionText(direction), text) == 0);
  LineTracking_BuildMotion(code, &motion);
  assert(motion.line_detected == 1U);
  assert(motion.drive_direction == 1);
  assert(motion.turn_direction == turn_direction);
  assert(motion.turn_divisor == turn_divisor);
}

static void TestRecognizedMotions(void)
{
  AssertMotion(0x07U, LINE_TRACKING_DIRECTION_LEFT, "L", -1, 1U);
  AssertMotion(0x09U, LINE_TRACKING_DIRECTION_MIDDLE, "M", 0, 2U);
  AssertMotion(0x0EU, LINE_TRACKING_DIRECTION_RIGHT, "R", 1, 1U);
  AssertMotion(0x0BU, LINE_TRACKING_DIRECTION_SLIGHT_LEFT, "SL", -1, 3U);
  AssertMotion(0x0DU, LINE_TRACKING_DIRECTION_SLIGHT_RIGHT, "SR", 1, 3U);
}

static void TestUnrecognizedCodesStop(void)
{
  uint8_t code;

  for (code = 0U; code < 16U; ++code)
  {
    if ((code != 0x07U) && (code != 0x09U) && (code != 0x0EU) &&
        (code != 0x0BU) && (code != 0x0DU) && (code != 0x0FU))
    {
      LineTrackingMotion_t motion;

      assert(LineTracking_Decode(code) == LINE_TRACKING_DIRECTION_NONE);
      LineTracking_BuildMotion(code, &motion);
      assert(motion.line_detected == 0U);
      assert(motion.drive_direction == 0);
      assert(motion.turn_direction == 0);
    }
  }
}

static void TestAllHighAlwaysDrivesStraight(void)
{
  LineTrackingMotion_t motion;

  assert(LineTracking_Decode(0x0FU) == LINE_TRACKING_DIRECTION_MIDDLE);
  assert(strcmp(LineTracking_DirectionText(LineTracking_Decode(0x0FU)),
                "M") == 0);
  LineTracking_BuildMotion(0x0FU, &motion);
  assert(motion.line_detected == 1U);
  assert(motion.drive_direction == 1);
  assert(motion.turn_direction == 0);
  assert(motion.turn_divisor == 2U);
}

int main(void)
{
  TestBitPacking();
  TestRecognizedMotions();
  TestUnrecognizedCodesStop();
  TestAllHighAlwaysDrivesStraight();
  puts("line_tracking_tests: PASS");
  return 0;
}
