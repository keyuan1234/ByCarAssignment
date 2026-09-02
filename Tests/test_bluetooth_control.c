#include "bluetooth_control.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void TestLetterProtocol(void)
{
  BluetoothCommand_t command;

  BluetoothCommand_Init(&command);
  assert(command.speed_mm_s == BLUETOOTH_DEFAULT_SPEED_MM_S);
  assert(command.line_speed_mm_s == BLUETOOTH_LINE_DEFAULT_SPEED_MM_S);
  assert(command.turn_divisor == 2U);
  assert(command.control_mode == BLUETOOTH_MODE_MANUAL);

  assert(BluetoothCommand_ProcessByte(&command, 'A', 10U) == 1U);
  assert(command.drive_direction == BLUETOOTH_DRIVE_FORWARD);
  assert(command.turn_direction == BLUETOOTH_TURN_NONE);

  (void)BluetoothCommand_ProcessByte(&command, 'E', 20U);
  assert(command.drive_direction == BLUETOOTH_DRIVE_BACKWARD);

  (void)BluetoothCommand_ProcessByte(&command, 'C', 30U);
  assert(command.drive_direction == BLUETOOTH_DRIVE_STOP);
  assert(command.turn_direction == BLUETOOTH_TURN_RIGHT);

  (void)BluetoothCommand_ProcessByte(&command, 'G', 40U);
  assert(command.turn_direction == BLUETOOTH_TURN_LEFT);

  (void)BluetoothCommand_ProcessByte(&command, 'Z', 50U);
  assert(command.drive_direction == BLUETOOTH_DRIVE_STOP);
  assert(command.turn_direction == BLUETOOTH_TURN_NONE);
}

static void TestLegacyProtocol(void)
{
  BluetoothCommand_t command;

  BluetoothCommand_Init(&command);
  (void)BluetoothCommand_ProcessByte(&command, 0x01U, 1U);
  assert(command.drive_direction == BLUETOOTH_DRIVE_FORWARD);
  assert(command.turn_divisor == BLUETOOTH_TURN_DIVISOR_FAST);

  (void)BluetoothCommand_ProcessByte(&command, 0x05U, 2U);
  assert(command.drive_direction == BLUETOOTH_DRIVE_BACKWARD);

  (void)BluetoothCommand_ProcessByte(&command, 0x03U, 3U);
  assert(command.turn_direction == BLUETOOTH_TURN_RIGHT);

  (void)BluetoothCommand_ProcessByte(&command, 0x07U, 4U);
  assert(command.turn_direction == BLUETOOTH_TURN_LEFT);

  (void)BluetoothCommand_ProcessByte(&command, 0x00U, 5U);
  assert(command.drive_direction == BLUETOOTH_DRIVE_STOP);
  assert(command.turn_direction == BLUETOOTH_TURN_NONE);
}

static void SendOnFrame(BluetoothCommand_t *command, uint8_t code,
                        uint32_t now_ms)
{
  assert(BluetoothCommand_ProcessByte(command, 'O', now_ms) == 0U);
  assert(BluetoothCommand_ProcessByte(command, 'N', now_ms) == 0U);
  assert(BluetoothCommand_ProcessByte(command, code, now_ms) == 1U);
}

static void TestOnFramedAppProtocol(void)
{
  BluetoothCommand_t command;

  BluetoothCommand_Init(&command);
  SendOnFrame(&command, 'A', 10U);
  assert(command.drive_direction == BLUETOOTH_DRIVE_FORWARD);
  assert(command.turn_direction == BLUETOOTH_TURN_NONE);

  SendOnFrame(&command, 'F', 20U);
  assert(command.drive_direction == BLUETOOTH_DRIVE_STOP);
  assert(command.turn_direction == BLUETOOTH_TURN_NONE);

  SendOnFrame(&command, 'B', 30U);
  assert(command.drive_direction == BLUETOOTH_DRIVE_BACKWARD);
  assert(command.turn_direction == BLUETOOTH_TURN_NONE);

  SendOnFrame(&command, 'C', 40U);
  assert(command.drive_direction == BLUETOOTH_DRIVE_STOP);
  assert(command.turn_direction == BLUETOOTH_TURN_LEFT);

  SendOnFrame(&command, 'D', 50U);
  assert(command.drive_direction == BLUETOOTH_DRIVE_STOP);
  assert(command.turn_direction == BLUETOOTH_TURN_RIGHT);

  SendOnFrame(&command, 'F', 60U);
  assert(command.drive_direction == BLUETOOTH_DRIVE_STOP);
  assert(command.turn_direction == BLUETOOTH_TURN_NONE);
}

static void TestLineModeToggleReleaseAndTakeover(void)
{
  BluetoothCommand_t command;

  BluetoothCommand_Init(&command);
  SendOnFrame(&command, 'K', 10U);
  assert(command.control_mode == BLUETOOTH_MODE_LINE_TRACKING);
  assert(command.drive_direction == BLUETOOTH_DRIVE_STOP);
  assert(command.turn_direction == BLUETOOTH_TURN_NONE);

  SendOnFrame(&command, 'F', 20U);
  assert(command.control_mode == BLUETOOTH_MODE_LINE_TRACKING);

  SendOnFrame(&command, 'A', 30U);
  assert(command.control_mode == BLUETOOTH_MODE_MANUAL);
  assert(command.drive_direction == BLUETOOTH_DRIVE_FORWARD);

  SendOnFrame(&command, 'K', 40U);
  assert(command.control_mode == BLUETOOTH_MODE_LINE_TRACKING);
  (void)BluetoothCommand_ProcessByte(&command, 'G', 50U);
  assert(command.control_mode == BLUETOOTH_MODE_MANUAL);
  assert(command.turn_direction == BLUETOOTH_TURN_LEFT);

  SendOnFrame(&command, 'K', 60U);
  SendOnFrame(&command, 'K', 70U);
  assert(command.control_mode == BLUETOOTH_MODE_MANUAL);
  assert(command.drive_direction == BLUETOOTH_DRIVE_STOP);
  assert(command.turn_direction == BLUETOOTH_TURN_NONE);
}

static void TestIndependentLineSpeed(void)
{
  BluetoothCommand_t command;
  uint16_t manual_speed;
  int i;

  BluetoothCommand_Init(&command);
  manual_speed = command.speed_mm_s;
  SendOnFrame(&command, 'K', 10U);

  SendOnFrame(&command, BLUETOOTH_SPEED_UP_PRESS_BYTE, 20U);
  SendOnFrame(&command, BLUETOOTH_SPEED_UP_RELEASE_BYTE, 21U);
  assert(command.control_mode == BLUETOOTH_MODE_LINE_TRACKING);
  assert(command.line_speed_mm_s == 300U);
  assert(command.speed_mm_s == manual_speed);

  for (i = 0; i < 10; ++i)
  {
    SendOnFrame(&command, BLUETOOTH_SPEED_UP_PRESS_BYTE,
                (uint32_t)(30 + i));
  }
  assert(command.line_speed_mm_s == BLUETOOTH_MAX_SPEED_MM_S);
  assert(command.speed_mm_s == manual_speed);

  for (i = 0; i < 10; ++i)
  {
    SendOnFrame(&command, BLUETOOTH_SPEED_DOWN_PRESS_BYTE,
                (uint32_t)(50 + i));
  }
  assert(command.line_speed_mm_s == BLUETOOTH_MIN_SPEED_MM_S);
  assert(command.speed_mm_s == manual_speed);
  assert(command.control_mode == BLUETOOTH_MODE_LINE_TRACKING);

  /* Raw 2/3 remain compatible with terminals that do not add ON. */
  (void)BluetoothCommand_ProcessByte(
    &command, BLUETOOTH_SPEED_UP_PRESS_BYTE, 70U);
  assert(command.line_speed_mm_s == 100U);
  (void)BluetoothCommand_ProcessByte(
    &command, BLUETOOTH_SPEED_DOWN_PRESS_BYTE, 80U);
  assert(command.line_speed_mm_s == 0U);
}

static void TestLineModePersistsAfterTimeout(void)
{
  BluetoothCommand_t command;

  BluetoothCommand_Init(&command);
  SendOnFrame(&command, 'K', 100U);
  SendOnFrame(&command, 'F', 101U);
  BluetoothCommand_ApplyTimeout(&command, 601U);
  assert(command.active == 0U);
  assert(command.control_mode == BLUETOOTH_MODE_LINE_TRACKING);
  assert(command.line_speed_mm_s == BLUETOOTH_LINE_DEFAULT_SPEED_MM_S);
}

static void TestActualFunctionButtonProtocol(void)
{
  BluetoothCommand_t command;

  BluetoothCommand_Init(&command);
  SendOnFrame(&command, BLUETOOTH_LINE_MODE_PRESS_BYTE, 10U);
  assert(command.control_mode == BLUETOOTH_MODE_LINE_TRACKING);

  SendOnFrame(&command, BLUETOOTH_LINE_MODE_RELEASE_BYTE, 20U);
  assert(command.control_mode == BLUETOOTH_MODE_LINE_TRACKING);
  assert(command.drive_direction == BLUETOOTH_DRIVE_STOP);
  assert(command.turn_direction == BLUETOOTH_TURN_NONE);

  SendOnFrame(&command, BLUETOOTH_LINE_MODE_PRESS_BYTE, 30U);
  assert(command.control_mode == BLUETOOTH_MODE_MANUAL);
  SendOnFrame(&command, BLUETOOTH_LINE_MODE_RELEASE_BYTE, 40U);
  assert(command.control_mode == BLUETOOTH_MODE_MANUAL);

  /* Keep raw 1/a compatible for terminals that do not add the ON prefix. */
  (void)BluetoothCommand_ProcessByte(
    &command, BLUETOOTH_LINE_MODE_PRESS_BYTE, 50U);
  assert(command.control_mode == BLUETOOTH_MODE_LINE_TRACKING);
  (void)BluetoothCommand_ProcessByte(
    &command, BLUETOOTH_LINE_MODE_RELEASE_BYTE, 60U);
  assert(command.control_mode == BLUETOOTH_MODE_LINE_TRACKING);
}

static void TestSpeedLimitsAndUnknownCommand(void)
{
  BluetoothCommand_t command;
  int i;

  BluetoothCommand_Init(&command);
  for (i = 0; i < 10; ++i)
  {
    (void)BluetoothCommand_ProcessByte(&command, 'X', (uint32_t)i);
  }
  assert(command.speed_mm_s == BLUETOOTH_MAX_SPEED_MM_S);
  assert(command.turn_divisor == BLUETOOTH_TURN_DIVISOR_FAST);
  assert(command.drive_direction == BLUETOOTH_DRIVE_STOP);

  for (i = 0; i < 10; ++i)
  {
    (void)BluetoothCommand_ProcessByte(&command, 'Y', (uint32_t)i);
  }
  assert(command.speed_mm_s == BLUETOOTH_MIN_SPEED_MM_S);
  assert(command.turn_divisor == BLUETOOTH_TURN_DIVISOR_SLOW);

  (void)BluetoothCommand_ProcessByte(&command, 'A', 50U);
  (void)BluetoothCommand_ProcessByte(&command, '?', 51U);
  assert(command.drive_direction == BLUETOOTH_DRIVE_STOP);
  assert(command.turn_direction == BLUETOOTH_TURN_NONE);
  assert(BluetoothCommand_ProcessByte(&command, 0x0AU, 52U) == 0U);
}

static void TestTimeout(void)
{
  BluetoothCommand_t command;

  BluetoothCommand_Init(&command);
  (void)BluetoothCommand_ProcessByte(&command, 'A', 100U);
  BluetoothCommand_ApplyTimeout(&command, 599U);
  assert(command.drive_direction == BLUETOOTH_DRIVE_FORWARD);
  BluetoothCommand_ApplyTimeout(&command, 600U);
  assert(command.drive_direction == BLUETOOTH_DRIVE_STOP);
  assert(command.turn_direction == BLUETOOTH_TURN_NONE);
  assert(command.active == 0U);
  assert(command.speed_mm_s == BLUETOOTH_DEFAULT_SPEED_MM_S);

  (void)BluetoothCommand_ProcessByte(&command, 'E', 700U);
  assert(command.drive_direction == BLUETOOTH_DRIVE_BACKWARD);
  assert(command.active == 1U);
}

static void TestOnFrameHoldTimeout(void)
{
  BluetoothCommand_t command;

  BluetoothCommand_Init(&command);
  SendOnFrame(&command, 'A', 100U);
  assert(command.timeout_ms == BLUETOOTH_ON_FRAME_TIMEOUT_MS);
  BluetoothCommand_ApplyTimeout(&command, 600U);
  assert(command.drive_direction == BLUETOOTH_DRIVE_FORWARD);
  BluetoothCommand_ApplyTimeout(&command, 10099U);
  assert(command.drive_direction == BLUETOOTH_DRIVE_FORWARD);
  BluetoothCommand_ApplyTimeout(&command, 10100U);
  assert(command.drive_direction == BLUETOOTH_DRIVE_STOP);
  assert(command.active == 0U);
}

static void TestAtResponseFragments(void)
{
  BluetoothAtResponse_t response;
  const char *part1 = "+NAME=By";
  const char *part2 = "Car321\r\nOK\r\n";
  size_t i;

  BluetoothAtResponse_Init(&response);
  for (i = 0; i < strlen(part1); ++i)
  {
    BluetoothAtResponse_Push(&response, (uint8_t)part1[i]);
  }
  assert(BluetoothAtResponse_Contains(&response, BLUETOOTH_DEVICE_NAME) == 0U);
  for (i = 0; i < strlen(part2); ++i)
  {
    BluetoothAtResponse_Push(&response, (uint8_t)part2[i]);
  }
  assert(BluetoothAtResponse_Contains(&response, BLUETOOTH_DEVICE_NAME) == 1U);
  assert(BluetoothAtResponse_Contains(&response, "OK") == 1U);
  assert(BluetoothAtResponse_Contains(&response, "ERROR") == 0U);
}

int main(void)
{
  TestLetterProtocol();
  TestLegacyProtocol();
  TestOnFramedAppProtocol();
  TestLineModeToggleReleaseAndTakeover();
  TestIndependentLineSpeed();
  TestLineModePersistsAfterTimeout();
  TestActualFunctionButtonProtocol();
  TestSpeedLimitsAndUnknownCommand();
  TestTimeout();
  TestOnFrameHoldTimeout();
  TestAtResponseFragments();
  puts("bluetooth_control_tests: PASS");
  return 0;
}
