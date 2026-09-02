#ifndef __BLUETOOTH_CONTROL_H
#define __BLUETOOTH_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define BLUETOOTH_DEVICE_NAME             "ByCar321"
#define BLUETOOTH_DEFAULT_SPEED_MM_S       800U
#define BLUETOOTH_SPEED_STEP_MM_S          100U
#define BLUETOOTH_MIN_SPEED_MM_S           0U
#define BLUETOOTH_MAX_SPEED_MM_S           500U
#define BLUETOOTH_LINE_DEFAULT_SPEED_MM_S  200U
#define BLUETOOTH_LINE_MODE_PRESS_BYTE      0x31U
#define BLUETOOTH_LINE_MODE_RELEASE_BYTE    0x61U
#define BLUETOOTH_SPEED_UP_PRESS_BYTE        0x32U
#define BLUETOOTH_SPEED_UP_RELEASE_BYTE      0x62U
#define BLUETOOTH_SPEED_DOWN_PRESS_BYTE      0x33U
#define BLUETOOTH_SPEED_DOWN_RELEASE_BYTE    0x63U
#define BLUETOOTH_COMMAND_TIMEOUT_MS       500U
#define BLUETOOTH_ON_FRAME_TIMEOUT_MS      10000U
#define BLUETOOTH_TURN_RATE_DPS            54.0f
#define BLUETOOTH_TURN_DIVISOR_FAST        1U
#define BLUETOOTH_TURN_DIVISOR_SLOW        2U
#define BLUETOOTH_AT_RESPONSE_SIZE         96U

typedef enum
{
  BLUETOOTH_DRIVE_BACKWARD = -1,
  BLUETOOTH_DRIVE_STOP = 0,
  BLUETOOTH_DRIVE_FORWARD = 1
} BluetoothDriveDirection_t;

typedef enum
{
  BLUETOOTH_TURN_LEFT = -1,
  BLUETOOTH_TURN_NONE = 0,
  BLUETOOTH_TURN_RIGHT = 1
} BluetoothTurnDirection_t;

typedef enum
{
  BLUETOOTH_MODE_MANUAL = 0,
  BLUETOOTH_MODE_LINE_TRACKING = 1
} BluetoothControlMode_t;

typedef struct
{
  int8_t drive_direction;
  int8_t turn_direction;
  uint16_t speed_mm_s;
  uint16_t line_speed_mm_s;
  uint8_t turn_divisor;
  uint8_t control_mode;
  uint8_t active;
  uint8_t frame_state;
  uint32_t last_command_ms;
  uint32_t timeout_ms;
} BluetoothCommand_t;

typedef struct
{
  char data[BLUETOOTH_AT_RESPONSE_SIZE];
  size_t length;
} BluetoothAtResponse_t;

void BluetoothCommand_Init(BluetoothCommand_t *command);
uint8_t BluetoothCommand_ProcessByte(BluetoothCommand_t *command,
                                     uint8_t byte,
                                     uint32_t now_ms);
void BluetoothCommand_ApplyTimeout(BluetoothCommand_t *command,
                                   uint32_t now_ms);

void BluetoothAtResponse_Init(BluetoothAtResponse_t *response);
void BluetoothAtResponse_Push(BluetoothAtResponse_t *response, uint8_t byte);
uint8_t BluetoothAtResponse_Contains(const BluetoothAtResponse_t *response,
                                     const char *token);

#ifdef __cplusplus
}
#endif

#endif /* __BLUETOOTH_CONTROL_H */
