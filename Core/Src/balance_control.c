#include "balance_control.h"
#include "motor_control.h"
#include <stddef.h>
#include <string.h>

#if BALANCE_ENABLE_VELOCITY_LOOP
static float Balance_ClampFloat(float value, float limit)
{
  if (value > limit)
  {
    return limit;
  }
  if (value < -limit)
  {
    return -limit;
  }
  return value;
}
#endif

static int32_t Balance_RoundFloat(float value)
{
  if (value >= 0.0f)
  {
    return (int32_t)(value + 0.5f);
  }
  return (int32_t)(value - 0.5f);
}

static int16_t Balance_ClampTotalPwm(int32_t value)
{
  if (value > MOTOR_PWM_LIMIT_COUNTS)
  {
    return MOTOR_PWM_LIMIT_COUNTS;
  }
  if (value < -MOTOR_PWM_LIMIT_COUNTS)
  {
    return -MOTOR_PWM_LIMIT_COUNTS;
  }
  return (int16_t)value;
}

static int16_t Balance_ClampInt16(int32_t value)
{
  if (value > 32767)
  {
    return 32767;
  }
  if (value < -32768)
  {
    return -32768;
  }
  return (int16_t)value;
}

static uint8_t Balance_IsTilted(float angle)
{
  return (uint8_t)((angle > BALANCE_TILT_SHUTDOWN_DEG) ||
                   (angle < -BALANCE_TILT_SHUTDOWN_DEG));
}

static BalanceOutput_t Balance_StoppedOutput(BalanceState_t state)
{
  BalanceOutput_t output;

  (void)memset(&output, 0, sizeof(output));
  output.state = state;
  return output;
}

void BalanceControl_Init(BalanceController_t *controller)
{
  BalanceControl_Reset(controller);
}

void BalanceControl_Reset(BalanceController_t *controller)
{
  if (controller == NULL)
  {
    return;
  }
  controller->speed_filtered = 0.0f;
  controller->speed_integral = 0.0f;
}

BalanceOutput_t BalanceControl_Update(BalanceController_t *controller,
                                      const BalanceSample_t *sample,
                                      int16_t encoder_left,
                                      int16_t encoder_right,
                                      const BalanceSetpoint_t *setpoint)
{
  BalanceOutput_t output;
  float angle_error;
  float balance_pwm;
  float velocity_pwm = 0.0f;
  float target_speed_counts = 0.0f;
  float turn_pwm = 0.0f;
  int32_t total_pwm;

  if ((controller == NULL) || (sample == NULL) || (sample->valid == 0U))
  {
    BalanceControl_Reset(controller);
    return Balance_StoppedOutput(BALANCE_STATE_IMU_ERROR);
  }

  if (Balance_IsTilted(sample->pitch_deg) != 0U)
  {
    BalanceControl_Reset(controller);
    return Balance_StoppedOutput(BALANCE_STATE_TILT);
  }

  (void)memset(&output, 0, sizeof(output));
  if (setpoint != NULL)
  {
    target_speed_counts = setpoint->wheel_speed_sum_target_counts;
    turn_pwm = BALANCE_TURN_KP * setpoint->turn_rate_target_dps;
    if (setpoint->translation_active != 0U)
    {
      turn_pwm += BALANCE_TURN_KD * sample->gyro_yaw_dps;
    }
  }
  angle_error = sample->pitch_deg - BALANCE_MIDDLE_ANGLE_DEG;
  balance_pwm = (BALANCE_KP * angle_error) +
                (BALANCE_KD * sample->gyro_pitch_dps);

#if BALANCE_ENABLE_VELOCITY_LOOP
  {
    float speed = (float)encoder_left + (float)encoder_right;

    /*
     * 正逻辑 PWM 必须对应正编码器增量。正速度产生正 velocity_pwm，
     * 使车轮继续追向运动方向并建立反向车身倾角，这是示例的等效串级符号。
     */
    controller->speed_filtered =
      (BALANCE_ENCODER_FILTER_KEEP * controller->speed_filtered) +
      (BALANCE_ENCODER_FILTER_NEW * speed);
    controller->speed_integral = Balance_ClampFloat(
      controller->speed_integral + controller->speed_filtered -
        target_speed_counts,
      BALANCE_VELOCITY_INTEGRAL_LIMIT);
    velocity_pwm = (BALANCE_VELOCITY_KP * controller->speed_filtered) +
                   (BALANCE_VELOCITY_KI * controller->speed_integral);
  }
#else
  (void)encoder_left;
  (void)encoder_right;
  (void)target_speed_counts;
  BalanceControl_Reset(controller);
#endif

  output.balance_pwm = Balance_RoundFloat(balance_pwm);
  output.velocity_pwm = Balance_RoundFloat(velocity_pwm);
  output.requested_pwm = output.balance_pwm + output.velocity_pwm;
  output.speed_filtered = controller->speed_filtered;
  output.speed_integral = controller->speed_integral;
  output.turn_pwm = Balance_ClampInt16(Balance_RoundFloat(turn_pwm));
  total_pwm = output.requested_pwm;
  output.left_pwm = Balance_ClampTotalPwm(total_pwm + output.turn_pwm);
  output.right_pwm = Balance_ClampTotalPwm(total_pwm - output.turn_pwm);
  output.state = BALANCE_STATE_RUNNING;
  return output;
}
