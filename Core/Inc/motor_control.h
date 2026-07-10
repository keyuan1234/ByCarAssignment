#ifndef __MOTOR_CONTROL_H
#define __MOTOR_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define MOTOR_PWM_PERIOD_COUNTS       7200
#define MOTOR_PWM_LIMIT_COUNTS        6900
#define MOTOR_CONTROL_PERIOD_MS       10U
#define MOTOR_CONTROL_PERIOD_S        0.01f

#ifndef MOTOR_ENCODER_COUNTS_PER_REV
#define MOTOR_ENCODER_COUNTS_PER_REV  1560.0f
#endif

#ifndef MOTOR_WHEEL_DIAMETER_MM
#define MOTOR_WHEEL_DIAMETER_MM       70.0f
#endif

#define MOTOR_SPEED_FILTER_ALPHA      0.2f

#ifndef MOTOR_LEFT_ENCODER_SIGN
#define MOTOR_LEFT_ENCODER_SIGN       1
#endif

#ifndef MOTOR_RIGHT_ENCODER_SIGN
#define MOTOR_RIGHT_ENCODER_SIGN      -1
#endif

#ifndef MOTOR_LEFT_COMMAND_SIGN
#define MOTOR_LEFT_COMMAND_SIGN       1
#endif

#ifndef MOTOR_RIGHT_COMMAND_SIGN
#define MOTOR_RIGHT_COMMAND_SIGN      1
#endif

#ifndef MOTOR_LEFT_PWM_NUM
#define MOTOR_LEFT_PWM_NUM            1000
#endif

#ifndef MOTOR_LEFT_PWM_DEN
#define MOTOR_LEFT_PWM_DEN            1000
#endif

#ifndef MOTOR_RIGHT_PWM_NUM
#define MOTOR_RIGHT_PWM_NUM           1000
#endif

#ifndef MOTOR_RIGHT_PWM_DEN
#define MOTOR_RIGHT_PWM_DEN           1000
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

void Motor_Init(void);
void Motor_Brake(void);
void Motor_SetPWM(int16_t left, int16_t right);
void Encoder_Update10ms(void);
const MotorTelemetry_t *Motor_GetTelemetry(void);

#ifdef __cplusplus
}
#endif

#endif /* __MOTOR_CONTROL_H */
