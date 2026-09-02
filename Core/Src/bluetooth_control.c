#include "bluetooth_control.h"
#include <string.h>

static void BluetoothCommand_Stop(BluetoothCommand_t *command)
{
  command->drive_direction = BLUETOOTH_DRIVE_STOP;
  command->turn_direction = BLUETOOTH_TURN_NONE;
}

static void BluetoothCommand_UseManualMode(BluetoothCommand_t *command)
{
  command->control_mode = BLUETOOTH_MODE_MANUAL;
}

static void BluetoothCommand_ToggleMode(BluetoothCommand_t *command)
{
  command->control_mode =
    (command->control_mode == BLUETOOTH_MODE_LINE_TRACKING) ?
    BLUETOOTH_MODE_MANUAL : BLUETOOTH_MODE_LINE_TRACKING;
  BluetoothCommand_Stop(command);
}

static uint16_t BluetoothCommand_ClampSpeed(int32_t speed_mm_s)
{
  if (speed_mm_s < (int32_t)BLUETOOTH_MIN_SPEED_MM_S)
  {
    return BLUETOOTH_MIN_SPEED_MM_S;
  }
  if (speed_mm_s > (int32_t)BLUETOOTH_MAX_SPEED_MM_S)
  {
    return BLUETOOTH_MAX_SPEED_MM_S;
  }
  return (uint16_t)speed_mm_s;
}

static void BluetoothCommand_AdjustSelectedSpeed(BluetoothCommand_t *command,
                                                  int32_t adjustment_mm_s)
{
  if (command->control_mode == BLUETOOTH_MODE_LINE_TRACKING)
  {
    command->line_speed_mm_s = BluetoothCommand_ClampSpeed(
      (int32_t)command->line_speed_mm_s + adjustment_mm_s);
  }
  else
  {
    command->speed_mm_s = BluetoothCommand_ClampSpeed(
      (int32_t)command->speed_mm_s + adjustment_mm_s);
    command->turn_divisor = (adjustment_mm_s > 0) ?
                            BLUETOOTH_TURN_DIVISOR_FAST :
                            BLUETOOTH_TURN_DIVISOR_SLOW;
  }
  BluetoothCommand_Stop(command);
}

static uint8_t BluetoothCommand_ProcessOnFrame(BluetoothCommand_t *command,
                                                uint8_t byte,
                                                uint32_t now_ms)
{
  if ((byte == 0x4BU) || (byte == BLUETOOTH_LINE_MODE_PRESS_BYTE))
  {
    BluetoothCommand_ToggleMode(command);
    command->timeout_ms = BLUETOOTH_ON_FRAME_TIMEOUT_MS;
  }
  else if (byte == BLUETOOTH_SPEED_UP_PRESS_BYTE)
  {
    BluetoothCommand_AdjustSelectedSpeed(
      command, (int32_t)BLUETOOTH_SPEED_STEP_MM_S);
    command->timeout_ms = BLUETOOTH_COMMAND_TIMEOUT_MS;
  }
  else if (byte == BLUETOOTH_SPEED_DOWN_PRESS_BYTE)
  {
    BluetoothCommand_AdjustSelectedSpeed(
      command, -(int32_t)BLUETOOTH_SPEED_STEP_MM_S);
    command->timeout_ms = BLUETOOTH_COMMAND_TIMEOUT_MS;
  }
  else if (byte == 0x41U)
  {
    BluetoothCommand_UseManualMode(command);
    command->drive_direction = BLUETOOTH_DRIVE_FORWARD;
    command->turn_direction = BLUETOOTH_TURN_NONE;
    command->timeout_ms = BLUETOOTH_ON_FRAME_TIMEOUT_MS;
  }
  else if (byte == 0x42U)
  {
    BluetoothCommand_UseManualMode(command);
    command->drive_direction = BLUETOOTH_DRIVE_BACKWARD;
    command->turn_direction = BLUETOOTH_TURN_NONE;
    command->timeout_ms = BLUETOOTH_ON_FRAME_TIMEOUT_MS;
  }
  else if (byte == 0x43U)
  {
    BluetoothCommand_UseManualMode(command);
    command->drive_direction = BLUETOOTH_DRIVE_STOP;
    command->turn_direction = BLUETOOTH_TURN_LEFT;
    command->timeout_ms = BLUETOOTH_ON_FRAME_TIMEOUT_MS;
  }
  else if (byte == 0x44U)
  {
    BluetoothCommand_UseManualMode(command);
    command->drive_direction = BLUETOOTH_DRIVE_STOP;
    command->turn_direction = BLUETOOTH_TURN_RIGHT;
    command->timeout_ms = BLUETOOTH_ON_FRAME_TIMEOUT_MS;
  }
  else
  {
    /* ONF/ONa are key-release commands; unknown framed bytes also stop. */
    BluetoothCommand_Stop(command);
    command->timeout_ms = BLUETOOTH_COMMAND_TIMEOUT_MS;
  }

  command->active = 1U;
  command->last_command_ms = now_ms;
  return 1U;
}

void BluetoothCommand_Init(BluetoothCommand_t *command)
{
  if (command == NULL)
  {
    return;
  }

  BluetoothCommand_Stop(command);
  command->speed_mm_s = BLUETOOTH_DEFAULT_SPEED_MM_S;
  command->line_speed_mm_s = BLUETOOTH_LINE_DEFAULT_SPEED_MM_S;
  command->turn_divisor = BLUETOOTH_TURN_DIVISOR_SLOW;
  command->control_mode = BLUETOOTH_MODE_MANUAL;
  command->active = 0U;
  command->frame_state = 0U;
  command->last_command_ms = 0U;
  command->timeout_ms = BLUETOOTH_COMMAND_TIMEOUT_MS;
}

uint8_t BluetoothCommand_ProcessByte(BluetoothCommand_t *command,
                                     uint8_t byte,
                                     uint32_t now_ms)
{
  if (command == NULL)
  {
    return 0U;
  }

  /* The phone app wraps commands as ONA/ONB/ONC/OND and ONF on release. */
  if (command->frame_state == 0U)
  {
    if (byte == 0x4FU)
    {
      command->frame_state = 1U;
      return 0U;
    }
  }
  else if (command->frame_state == 1U)
  {
    command->frame_state = 0U;
    if (byte == 0x4EU)
    {
      command->frame_state = 2U;
      return 0U;
    }
  }
  else
  {
    command->frame_state = 0U;
    return BluetoothCommand_ProcessOnFrame(command, byte, now_ms);
  }

  /* The original protocol intentionally leaves 0x0A unused. */
  if (byte == 0x0AU)
  {
    return 0U;
  }

  if (byte > 10U)
  {
    if (byte == BLUETOOTH_LINE_MODE_PRESS_BYTE)
    {
      BluetoothCommand_ToggleMode(command);
    }
    else if (byte == BLUETOOTH_SPEED_UP_PRESS_BYTE)
    {
      BluetoothCommand_AdjustSelectedSpeed(
        command, (int32_t)BLUETOOTH_SPEED_STEP_MM_S);
    }
    else if (byte == BLUETOOTH_SPEED_DOWN_PRESS_BYTE)
    {
      BluetoothCommand_AdjustSelectedSpeed(
        command, -(int32_t)BLUETOOTH_SPEED_STEP_MM_S);
    }
    else if (byte == BLUETOOTH_LINE_MODE_RELEASE_BYTE)
    {
      /* The phone app sends 'a' when the function button is released. */
      BluetoothCommand_Stop(command);
    }
    else if (byte == 0x58U)
    {
      BluetoothCommand_AdjustSelectedSpeed(
        command, (int32_t)BLUETOOTH_SPEED_STEP_MM_S);
    }
    else if (byte == 0x59U)
    {
      BluetoothCommand_AdjustSelectedSpeed(
        command, -(int32_t)BLUETOOTH_SPEED_STEP_MM_S);
    }
    else if (byte == 0x41U)
    {
      BluetoothCommand_UseManualMode(command);
      command->drive_direction = BLUETOOTH_DRIVE_FORWARD;
      command->turn_direction = BLUETOOTH_TURN_NONE;
    }
    else if (byte == 0x45U)
    {
      BluetoothCommand_UseManualMode(command);
      command->drive_direction = BLUETOOTH_DRIVE_BACKWARD;
      command->turn_direction = BLUETOOTH_TURN_NONE;
    }
    else if ((byte == 0x42U) || (byte == 0x43U) || (byte == 0x44U))
    {
      BluetoothCommand_UseManualMode(command);
      command->drive_direction = BLUETOOTH_DRIVE_STOP;
      command->turn_direction = BLUETOOTH_TURN_RIGHT;
    }
    else if ((byte == 0x46U) || (byte == 0x47U) || (byte == 0x48U))
    {
      BluetoothCommand_UseManualMode(command);
      command->drive_direction = BLUETOOTH_DRIVE_STOP;
      command->turn_direction = BLUETOOTH_TURN_LEFT;
    }
    else
    {
      BluetoothCommand_Stop(command);
    }
  }
  else
  {
    command->turn_divisor = BLUETOOTH_TURN_DIVISOR_FAST;
    if (byte == 0x01U)
    {
      BluetoothCommand_UseManualMode(command);
      command->drive_direction = BLUETOOTH_DRIVE_FORWARD;
      command->turn_direction = BLUETOOTH_TURN_NONE;
    }
    else if (byte == 0x05U)
    {
      BluetoothCommand_UseManualMode(command);
      command->drive_direction = BLUETOOTH_DRIVE_BACKWARD;
      command->turn_direction = BLUETOOTH_TURN_NONE;
    }
    else if ((byte == 0x02U) || (byte == 0x03U) || (byte == 0x04U))
    {
      BluetoothCommand_UseManualMode(command);
      command->drive_direction = BLUETOOTH_DRIVE_STOP;
      command->turn_direction = BLUETOOTH_TURN_RIGHT;
    }
    else if ((byte == 0x06U) || (byte == 0x07U) || (byte == 0x08U))
    {
      BluetoothCommand_UseManualMode(command);
      command->drive_direction = BLUETOOTH_DRIVE_STOP;
      command->turn_direction = BLUETOOTH_TURN_LEFT;
    }
    else
    {
      if (byte == 0x00U)
      {
        BluetoothCommand_UseManualMode(command);
      }
      BluetoothCommand_Stop(command);
    }
  }

  command->active = 1U;
  command->last_command_ms = now_ms;
  command->timeout_ms = BLUETOOTH_COMMAND_TIMEOUT_MS;
  return 1U;
}

void BluetoothCommand_ApplyTimeout(BluetoothCommand_t *command,
                                   uint32_t now_ms)
{
  if ((command == NULL) || (command->active == 0U))
  {
    return;
  }

  if ((uint32_t)(now_ms - command->last_command_ms) >= command->timeout_ms)
  {
    BluetoothCommand_Stop(command);
    command->active = 0U;
    command->frame_state = 0U;
  }
}

void BluetoothAtResponse_Init(BluetoothAtResponse_t *response)
{
  if (response == NULL)
  {
    return;
  }
  (void)memset(response, 0, sizeof(*response));
}

void BluetoothAtResponse_Push(BluetoothAtResponse_t *response, uint8_t byte)
{
  if ((response == NULL) ||
      (response->length >= (BLUETOOTH_AT_RESPONSE_SIZE - 1U)))
  {
    return;
  }

  response->data[response->length] = (char)byte;
  response->length++;
  response->data[response->length] = '\0';
}

uint8_t BluetoothAtResponse_Contains(const BluetoothAtResponse_t *response,
                                     const char *token)
{
  if ((response == NULL) || (token == NULL))
  {
    return 0U;
  }
  return (uint8_t)((strstr(response->data, token) != NULL) ? 1U : 0U);
}
