#ifndef __MOTOR_CONTROL_H
#define __MOTOR_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Compile-time experiment selection. */
#define MOTOR_EXPERIMENT_OPEN_LOOP       1U
#define MOTOR_EXPERIMENT_DEADZONE        2U
#define MOTOR_EXPERIMENT_ENCODER_VERIFY  3U
#define MOTOR_EXPERIMENT_PI_CLOSED_LOOP  4U

#ifndef MOTOR_EXPERIMENT_MODE
#define MOTOR_EXPERIMENT_MODE MOTOR_EXPERIMENT_ENCODER_VERIFY
#endif

#ifndef MOTOR_PI_PARAMETER_SET
#define MOTOR_PI_PARAMETER_SET 0U
#endif

/* Hardware parameters from the C10B schematic and B585 manual. */
#define MOTOR_PWM_PERIOD_COUNTS        7200
#ifndef MOTOR_ENCODER_COUNTS_PER_REV
#define MOTOR_ENCODER_COUNTS_PER_REV   1560.0f
#endif

#ifndef MOTOR_WHEEL_DIAMETER_MM
#define MOTOR_WHEEL_DIAMETER_MM        80.0f
#endif
#define MOTOR_CONTROL_PERIOD_S         0.01f
#define MOTOR_SPEED_FILTER_ALPHA       0.2f

/* Set either value to -1 if the measured forward direction is negative. */
#ifndef MOTOR_LEFT_ENCODER_SIGN
#define MOTOR_LEFT_ENCODER_SIGN  1
#endif

#ifndef MOTOR_RIGHT_ENCODER_SIGN
#define MOTOR_RIGHT_ENCODER_SIGN 1
#endif

/* Set either value to -1 only if the corresponding motor wiring is reversed. */
#ifndef MOTOR_LEFT_COMMAND_SIGN
#define MOTOR_LEFT_COMMAND_SIGN  1
#endif

#ifndef MOTOR_RIGHT_COMMAND_SIGN
#define MOTOR_RIGHT_COMMAND_SIGN 1
#endif

typedef struct
{
  int16_t left_delta;
  int16_t right_delta;
  int32_t left_total;
  int32_t right_total;
  float left_raw_mm_s;
  float right_raw_mm_s;
  float left_filtered_mm_s;
  float right_filtered_mm_s;
  int16_t left_pwm;
  int16_t right_pwm;
} MotorTelemetry_t;

typedef struct
{
  float kp;
  float ki;
  float previous_error;
  float output;
} PIController_t;

void Motor_Init(void);
void Motor_SetPWM(int16_t left, int16_t right);
void Encoder_Update10ms(void);
const MotorTelemetry_t *Motor_GetTelemetry(void);

void PI_Init(PIController_t *controller, float kp, float ki);
void PI_Reset(PIController_t *controller);
int16_t PI_Update(PIController_t *controller, float target, float measured);

#ifdef __cplusplus
}
#endif

#endif /* __MOTOR_CONTROL_H */
