#ifndef __BALANCE_CONTROL_H
#define __BALANCE_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "balance_types.h"

#ifndef BALANCE_MIDDLE_ANGLE_DEG
#define BALANCE_MIDDLE_ANGLE_DEG      1.0f
#endif

#ifndef BALANCE_KP
#define BALANCE_KP                    (27000.0f / 100.0f)
#endif

#ifndef BALANCE_KD
#define BALANCE_KD                    (110.0f / 100.0f)
#endif

#ifndef BALANCE_VELOCITY_KP
#define BALANCE_VELOCITY_KP           (400.0f / 100.0f)
#endif

#ifndef BALANCE_VELOCITY_KI
#define BALANCE_VELOCITY_KI           (2.0f / 100.0f)
#endif

#ifndef BALANCE_TILT_SHUTDOWN_DEG
#define BALANCE_TILT_SHUTDOWN_DEG     40.0f
#endif

#ifndef BALANCE_SAMPLE_TIMEOUT_MS
#define BALANCE_SAMPLE_TIMEOUT_MS     30U
#endif

#ifndef BALANCE_ENCODER_FILTER_KEEP
#define BALANCE_ENCODER_FILTER_KEEP   0.84f
#endif

#ifndef BALANCE_ENCODER_FILTER_NEW
#define BALANCE_ENCODER_FILTER_NEW    0.16f
#endif

#ifndef BALANCE_VELOCITY_INTEGRAL_LIMIT
#define BALANCE_VELOCITY_INTEGRAL_LIMIT  380000.0f
#endif

typedef struct
{
  float encoder_bias;
  float encoder_integral;
} BalanceController_t;

typedef struct
{
  int16_t balance_pwm;
  int16_t velocity_pwm;
  int16_t turn_pwm;
  int16_t left_pwm;
  int16_t right_pwm;
  BalanceState_t state;
} BalanceOutput_t;

void BalanceControl_Init(BalanceController_t *controller);
void BalanceControl_Reset(BalanceController_t *controller);
BalanceOutput_t BalanceControl_Update(BalanceController_t *controller,
                                      const BalanceSample_t *sample,
                                      int16_t encoder_left,
                                      int16_t encoder_right);

#ifdef __cplusplus
}
#endif

#endif /* __BALANCE_CONTROL_H */
