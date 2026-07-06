#include "motor_control.h"
#include "tim.h"

#define MOTOR_PI_F 3.14159265358979323846f
#define MOTOR_MM_PER_COUNT \
  ((MOTOR_PI_F * MOTOR_WHEEL_DIAMETER_MM) / MOTOR_ENCODER_COUNTS_PER_REV)
#define MOTOR_MM_S_PER_COUNT (MOTOR_MM_PER_COUNT / MOTOR_CONTROL_PERIOD_S)

static MotorTelemetry_t motor_telemetry;
static uint16_t left_previous_count;
static uint16_t right_previous_count;

static int16_t Motor_ClampPWM(int32_t value)
{
  if (value > MOTOR_PWM_PERIOD_COUNTS)
  {
    return MOTOR_PWM_PERIOD_COUNTS;
  }
  if (value < -MOTOR_PWM_PERIOD_COUNTS)
  {
    return -MOTOR_PWM_PERIOD_COUNTS;
  }
  return (int16_t)value;
}

static uint32_t Motor_PWMCompareFromMagnitude(int32_t magnitude)
{
  if (magnitude < 0)
  {
    magnitude = -magnitude;
  }
  if (magnitude > MOTOR_PWM_PERIOD_COUNTS)
  {
    magnitude = MOTOR_PWM_PERIOD_COUNTS;
  }
  return (uint32_t)(MOTOR_PWM_PERIOD_COUNTS - magnitude);
}

void Motor_Init(void)
{
  motor_telemetry.left_delta = 0;
  motor_telemetry.right_delta = 0;
  motor_telemetry.left_total = 0;
  motor_telemetry.right_total = 0;
  motor_telemetry.left_raw_mm_s = 0.0f;
  motor_telemetry.right_raw_mm_s = 0.0f;
  motor_telemetry.left_filtered_mm_s = 0.0f;
  motor_telemetry.right_filtered_mm_s = 0.0f;
  motor_telemetry.left_pwm = 0;
  motor_telemetry.right_pwm = 0;

  /* AT8236 braking state: IN1=1 and IN2=1 on both bridges. */
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, MOTOR_PWM_PERIOD_COUNTS);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, MOTOR_PWM_PERIOD_COUNTS);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, MOTOR_PWM_PERIOD_COUNTS);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, MOTOR_PWM_PERIOD_COUNTS);

  if ((HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK) ||
      (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2) != HAL_OK) ||
      (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3) != HAL_OK) ||
      (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4) != HAL_OK))
  {
    Error_Handler();
  }

  __HAL_TIM_SET_COUNTER(&htim4, 0U);
  __HAL_TIM_SET_COUNTER(&htim8, 0U);
  if ((HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL) != HAL_OK) ||
      (HAL_TIM_Encoder_Start(&htim8, TIM_CHANNEL_ALL) != HAL_OK))
  {
    Error_Handler();
  }
  left_previous_count = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);
  right_previous_count = (uint16_t)__HAL_TIM_GET_COUNTER(&htim8);
}

void Motor_SetPWM(int16_t left, int16_t right)
{
  uint32_t left_compare;
  uint32_t right_compare;

  left = Motor_ClampPWM((int32_t)left * MOTOR_LEFT_COMMAND_SIGN);
  right = Motor_ClampPWM((int32_t)right * MOTOR_RIGHT_COMMAND_SIGN);
  left_compare = Motor_PWMCompareFromMagnitude(left);
  right_compare = Motor_PWMCompareFromMagnitude(right);

  /* The motors are mirrored on the chassis, so their forward bridge states differ. */
  if (left > 0)
  {
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, MOTOR_PWM_PERIOD_COUNTS);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, left_compare);
  }
  else if (left < 0)
  {
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, left_compare);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, MOTOR_PWM_PERIOD_COUNTS);
  }
  else
  {
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, MOTOR_PWM_PERIOD_COUNTS);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, MOTOR_PWM_PERIOD_COUNTS);
  }

  if (right > 0)
  {
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, right_compare);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, MOTOR_PWM_PERIOD_COUNTS);
  }
  else if (right < 0)
  {
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, MOTOR_PWM_PERIOD_COUNTS);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, right_compare);
  }
  else
  {
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, MOTOR_PWM_PERIOD_COUNTS);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, MOTOR_PWM_PERIOD_COUNTS);
  }

  motor_telemetry.left_pwm = left;
  motor_telemetry.right_pwm = right;
}

void Encoder_Update10ms(void)
{
  uint16_t left_current = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);
  uint16_t right_current = (uint16_t)__HAL_TIM_GET_COUNTER(&htim8);
  int16_t left_delta = (int16_t)(left_current - left_previous_count);
  int16_t right_delta = (int16_t)(right_current - right_previous_count);

  left_previous_count = left_current;
  right_previous_count = right_current;
  left_delta = (int16_t)(left_delta * MOTOR_LEFT_ENCODER_SIGN);
  right_delta = (int16_t)(right_delta * MOTOR_RIGHT_ENCODER_SIGN);

  motor_telemetry.left_delta = left_delta;
  motor_telemetry.right_delta = right_delta;
  motor_telemetry.left_total += left_delta;
  motor_telemetry.right_total += right_delta;
  motor_telemetry.left_raw_mm_s = (float)left_delta * MOTOR_MM_S_PER_COUNT;
  motor_telemetry.right_raw_mm_s = (float)right_delta * MOTOR_MM_S_PER_COUNT;
  motor_telemetry.left_filtered_mm_s += MOTOR_SPEED_FILTER_ALPHA *
    (motor_telemetry.left_raw_mm_s - motor_telemetry.left_filtered_mm_s);
  motor_telemetry.right_filtered_mm_s += MOTOR_SPEED_FILTER_ALPHA *
    (motor_telemetry.right_raw_mm_s - motor_telemetry.right_filtered_mm_s);
}

const MotorTelemetry_t *Motor_GetTelemetry(void)
{
  return &motor_telemetry;
}

void PI_Init(PIController_t *controller, float kp, float ki)
{
  controller->kp = kp;
  controller->ki = ki;
  PI_Reset(controller);
}

void PI_Reset(PIController_t *controller)
{
  controller->previous_error = 0.0f;
  controller->output = 0.0f;
}

int16_t PI_Update(PIController_t *controller, float target, float measured)
{
  float error;
  float increment;

  if (target == 0.0f)
  {
    PI_Reset(controller);
    return 0;
  }

  error = target - measured;
  increment = controller->kp * (error - controller->previous_error) +
              controller->ki * MOTOR_CONTROL_PERIOD_S * error;
  controller->output += increment;

  if (controller->output > (float)MOTOR_PWM_PERIOD_COUNTS)
  {
    controller->output = (float)MOTOR_PWM_PERIOD_COUNTS;
  }
  else if (controller->output < (float)-MOTOR_PWM_PERIOD_COUNTS)
  {
    controller->output = (float)-MOTOR_PWM_PERIOD_COUNTS;
  }

  controller->previous_error = error;
  if (controller->output >= 0.0f)
  {
    return (int16_t)(controller->output + 0.5f);
  }
  return (int16_t)(controller->output - 0.5f);
}
