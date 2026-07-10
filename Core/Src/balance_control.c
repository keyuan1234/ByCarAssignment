#include "balance_control.h"
#include "motor_control.h"

static int16_t Balance_RoundToPwm(float value)
{
  int32_t rounded;

  if (value >= 0.0f)
  {
    rounded = (int32_t)(value + 0.5f);
  }
  else
  {
    rounded = (int32_t)(value - 0.5f);
  }

  if (rounded > MOTOR_PWM_LIMIT_COUNTS)
  {
    rounded = MOTOR_PWM_LIMIT_COUNTS;
  }
  else if (rounded < -MOTOR_PWM_LIMIT_COUNTS)
  {
    rounded = -MOTOR_PWM_LIMIT_COUNTS;
  }

  return (int16_t)rounded;
}

static uint8_t Balance_IsTilted(float angle)
{
  return (uint8_t)((angle > BALANCE_TILT_SHUTDOWN_DEG) ||
                   (angle < -BALANCE_TILT_SHUTDOWN_DEG));
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
  controller->encoder_bias = 0.0f;
  controller->encoder_integral = 0.0f;
}

BalanceOutput_t BalanceControl_Update(BalanceController_t *controller,
                                      const BalanceSample_t *sample,
                                      int16_t encoder_left,
                                      int16_t encoder_right)
{
  BalanceOutput_t output;
  float angle_bias;
  float gyro_bias;
  float balance_pwm;
  float encoder_least;
  float velocity_pwm;
  float left_pwm;
  float right_pwm;

  output.balance_pwm = 0;
  output.velocity_pwm = 0;
  output.turn_pwm = 0;
  output.left_pwm = 0;
  output.right_pwm = 0;
  output.state = BALANCE_STATE_STOPPED;

  if ((controller == NULL) || (sample == NULL) || (sample->valid == 0U))
  {
    BalanceControl_Reset(controller);
    output.state = BALANCE_STATE_IMU_ERROR;
    return output;
  }

  if (Balance_IsTilted(sample->pitch_deg) != 0U)
  {
    BalanceControl_Reset(controller);
    output.state = BALANCE_STATE_TILT;
    return output;
  }

  angle_bias = BALANCE_MIDDLE_ANGLE_DEG - sample->pitch_deg;
  gyro_bias = 0.0f - sample->gyro_pitch_dps;
  balance_pwm = (-BALANCE_KP * angle_bias) - (BALANCE_KD * gyro_bias);

  encoder_least = 0.0f - ((float)encoder_left + (float)encoder_right);
  controller->encoder_bias = (controller->encoder_bias * BALANCE_ENCODER_FILTER_KEEP) +
                             (encoder_least * BALANCE_ENCODER_FILTER_NEW);
  controller->encoder_integral += controller->encoder_bias;
  if (controller->encoder_integral > BALANCE_VELOCITY_INTEGRAL_LIMIT)
  {
    controller->encoder_integral = BALANCE_VELOCITY_INTEGRAL_LIMIT;
  }
  else if (controller->encoder_integral < -BALANCE_VELOCITY_INTEGRAL_LIMIT)
  {
    controller->encoder_integral = -BALANCE_VELOCITY_INTEGRAL_LIMIT;
  }

  velocity_pwm = (-BALANCE_VELOCITY_KP * controller->encoder_bias) -
                 (BALANCE_VELOCITY_KI * controller->encoder_integral);

  left_pwm = balance_pwm + velocity_pwm;
  right_pwm = balance_pwm + velocity_pwm;

  output.balance_pwm = Balance_RoundToPwm(balance_pwm);
  output.velocity_pwm = Balance_RoundToPwm(velocity_pwm);
  output.left_pwm = Balance_RoundToPwm(left_pwm);
  output.right_pwm = Balance_RoundToPwm(right_pwm);
  output.state = BALANCE_STATE_RUNNING;
  return output;
}
