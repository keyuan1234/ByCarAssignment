#include "balance_control.h"
#include "motor_control.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int FloatNear(float actual, float expected, float tolerance)
{
  return fabsf(actual - expected) <= tolerance;
}

static BalanceSample_t MakeSample(float pitch, float gyro, uint8_t valid)
{
  BalanceSample_t sample;

  memset(&sample, 0, sizeof(sample));
  sample.pitch_deg = pitch;
  sample.gyro_pitch_dps = gyro;
  sample.valid = valid;
  return sample;
}

static void TestInvalidTiltAndReset(void)
{
  BalanceController_t controller;
  BalanceSample_t sample;
  BalanceOutput_t output;

  BalanceControl_Init(&controller);
  sample = MakeSample(BALANCE_MIDDLE_ANGLE_DEG, 0.0f, 1U);
  (void)BalanceControl_Update(&controller, &sample, 500, 500, NULL);

  sample.valid = 0U;
  output = BalanceControl_Update(&controller, &sample, 0, 0, NULL);
  assert(output.state == BALANCE_STATE_IMU_ERROR);
  assert(output.left_pwm == 0);
  assert(FloatNear(controller.speed_integral, 0.0f, 0.001f));

  sample = MakeSample(BALANCE_TILT_SHUTDOWN_DEG + 0.1f, 0.0f, 1U);
  output = BalanceControl_Update(&controller, &sample, 0, 0, NULL);
  assert(output.state == BALANCE_STATE_TILT);
  assert(output.left_pwm == 0);
  assert(output.right_pwm == 0);
}

static void TestWheeltecAngleFormulaAndUnits(void)
{
  BalanceController_t controller;
  BalanceSample_t sample;
  BalanceOutput_t output;

  BalanceControl_Init(&controller);
  sample = MakeSample(BALANCE_MIDDLE_ANGLE_DEG + 2.0f, 10.0f, 1U);
  output = BalanceControl_Update(&controller, &sample, 0, 0, NULL);

  assert(output.balance_pwm ==
         (int32_t)((BALANCE_KP * 2.0f) + (BALANCE_KD * 10.0f) + 0.5f));
  assert(output.state == BALANCE_STATE_RUNNING);
}

static void TestVelocityDirectionAndGoldenValues(void)
{
  BalanceController_t controller;
  BalanceSample_t sample;
  BalanceOutput_t output;

  sample = MakeSample(BALANCE_MIDDLE_ANGLE_DEG, 0.0f, 1U);
  BalanceControl_Init(&controller);
  output = BalanceControl_Update(&controller, &sample, 500, 500, NULL);

#if BALANCE_ENABLE_VELOCITY_LOOP
  assert(FloatNear(output.speed_filtered, 160.0f, 0.001f));
  assert(FloatNear(output.speed_integral, 160.0f, 0.001f));
  assert(output.velocity_pwm == 643);
  assert(output.requested_pwm == 643);
  assert(output.left_pwm == 643);

  output = BalanceControl_Update(&controller, &sample, 500, 500, NULL);
  assert(FloatNear(output.speed_filtered, 294.4f, 0.01f));
  assert(FloatNear(output.speed_integral, 454.4f, 0.01f));
  assert(output.velocity_pwm == 1187);

  BalanceControl_Reset(&controller);
  output = BalanceControl_Update(&controller, &sample, -500, -500, NULL);
  assert(output.velocity_pwm == -643);
  assert(output.left_pwm == -643);
#else
  assert(output.velocity_pwm == 0);
  assert(output.requested_pwm == 0);
  assert(FloatNear(controller.speed_filtered, 0.0f, 0.001f));
  assert(FloatNear(controller.speed_integral, 0.0f, 0.001f));
#endif
}

static void TestIntegralAndTotalPwmLimits(void)
{
  BalanceController_t controller;
  BalanceSample_t sample;
  BalanceOutput_t output;
  int i;

  sample = MakeSample(BALANCE_MIDDLE_ANGLE_DEG, 0.0f, 1U);
  BalanceControl_Init(&controller);
  for (i = 0; i < 1000; ++i)
  {
    output = BalanceControl_Update(&controller, &sample, 32767, 32767, NULL);
  }

#if BALANCE_ENABLE_VELOCITY_LOOP
  assert(FloatNear(controller.speed_integral,
                   BALANCE_VELOCITY_INTEGRAL_LIMIT, 0.1f));
  assert(output.velocity_pwm > MOTOR_PWM_LIMIT_COUNTS);
  assert(output.left_pwm == MOTOR_PWM_LIMIT_COUNTS);
  assert(output.right_pwm == MOTOR_PWM_LIMIT_COUNTS);
#else
  assert(output.velocity_pwm == 0);
  assert(output.left_pwm == 0);
#endif

  sample = MakeSample(BALANCE_MIDDLE_ANGLE_DEG + 30.0f, 0.0f, 1U);
  output = BalanceControl_Update(&controller, &sample, 0, 0, NULL);
  assert(output.balance_pwm == (int32_t)(BALANCE_KP * 30.0f));
  assert(output.left_pwm == MOTOR_PWM_LIMIT_COUNTS);
}

static void TestSelectedDefaults(void)
{
  assert(FloatNear(BALANCE_KP, 260.0f, 0.001f));
  assert(FloatNear(BALANCE_KD, 13.0f, 0.001f));
  assert(FloatNear(BALANCE_MIDDLE_ANGLE_DEG, -1.0f, 0.001f));
  assert(FloatNear(BALANCE_VELOCITY_KP, 4.0f, 0.001f));
  assert(FloatNear(BALANCE_VELOCITY_KI, 0.02f, 0.001f));
  assert(FloatNear(BALANCE_VELOCITY_INTEGRAL_LIMIT, 380000.0f, 0.1f));
}

static void TestMotionSetpoints(void)
{
  BalanceController_t controller;
  BalanceSample_t sample;
  BalanceSetpoint_t setpoint;
  BalanceOutput_t output;

  sample = MakeSample(BALANCE_MIDDLE_ANGLE_DEG, 0.0f, 1U);
  memset(&setpoint, 0, sizeof(setpoint));

  BalanceControl_Init(&controller);
  setpoint.wheel_speed_sum_target_counts = 100.0f;
  output = BalanceControl_Update(&controller, &sample, 0, 0, &setpoint);
#if BALANCE_ENABLE_VELOCITY_LOOP
  assert(output.velocity_pwm == -2);
#else
  assert(output.velocity_pwm == 0);
#endif

  BalanceControl_Init(&controller);
  setpoint.wheel_speed_sum_target_counts = -100.0f;
  output = BalanceControl_Update(&controller, &sample, 0, 0, &setpoint);
#if BALANCE_ENABLE_VELOCITY_LOOP
  assert(output.velocity_pwm == 2);
#else
  assert(output.velocity_pwm == 0);
#endif

  BalanceControl_Init(&controller);
  memset(&setpoint, 0, sizeof(setpoint));
  setpoint.turn_rate_target_dps = 54.0f;
  output = BalanceControl_Update(&controller, &sample, 0, 0, &setpoint);
  assert(output.turn_pwm == 2268);
  assert(output.left_pwm == 2268);
  assert(output.right_pwm == -2268);

  sample.gyro_yaw_dps = 10.0f;
  setpoint.turn_rate_target_dps = 0.0f;
  setpoint.translation_active = 1U;
  output = BalanceControl_Update(&controller, &sample, 0, 0, &setpoint);
  assert(output.turn_pwm == 10);
}

int main(void)
{
  TestInvalidTiltAndReset();
  TestWheeltecAngleFormulaAndUnits();
  TestVelocityDirectionAndGoldenValues();
  TestIntegralAndTotalPwmLimits();
  TestSelectedDefaults();
  TestMotionSetpoints();
  puts("balance_control_tests: PASS");
  return 0;
}
