#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "motor_control.h"
#include "tim.h"

TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim8;

void Error_Handler(void)
{
  assert(0 && "unexpected HAL initialization failure");
}

int HAL_TIM_PWM_Start(TIM_HandleTypeDef *timer, uint32_t channel)
{
  (void)timer;
  (void)channel;
  return 0;
}

int HAL_TIM_Encoder_Start(TIM_HandleTypeDef *timer, uint32_t channel)
{
  (void)timer;
  (void)channel;
  return 0;
}

int main(void)
{
  const MotorTelemetry_t *telemetry;
  PIController_t pi;
  int i;

  Motor_Init();
  assert(htim3.compare[0] == 7200U && htim3.compare[1] == 7200U);
  assert(htim3.compare[2] == 7200U && htim3.compare[3] == 7200U);

  Motor_SetPWM(1440, 1440);
  assert(htim3.compare[0] == 7200U && htim3.compare[1] == 5760U);
  assert(htim3.compare[2] == 5760U && htim3.compare[3] == 7200U);

  Motor_SetPWM(-1440, -1440);
  assert(htim3.compare[0] == 5760U && htim3.compare[1] == 7200U);
  assert(htim3.compare[2] == 7200U && htim3.compare[3] == 5760U);

  Motor_SetPWM(9000, -9000);
  assert(htim3.compare[0] == 7200U && htim3.compare[1] == 0U);
  assert(htim3.compare[2] == 7200U && htim3.compare[3] == 0U);

  htim4.counter = 65530U;
  htim8.counter = 6U;
  Encoder_Update10ms();
  telemetry = Motor_GetTelemetry();
  assert(telemetry->left_delta == -6 && telemetry->right_delta == 6);
  htim4.counter = 5U;
  htim8.counter = 65531U;
  Encoder_Update10ms();
  telemetry = Motor_GetTelemetry();
  assert(telemetry->left_delta == 11 && telemetry->right_delta == -11);
  assert(telemetry->left_total == 5 && telemetry->right_total == -5);
  assert(fabs(telemetry->left_raw_mm_s - 177.217f) < 0.1f);

  PI_Init(&pi, 6.0f, 45.0f);
  for (i = 0; i < 1000; ++i)
  {
    (void)PI_Update(&pi, 200.0f, 0.0f);
  }
  assert(pi.output == 7200.0f);
  assert(PI_Update(&pi, 0.0f, 0.0f) == 0);
  assert(pi.output == 0.0f && pi.previous_error == 0.0f);

  puts("motor_control tests: OK");
  return 0;
}
