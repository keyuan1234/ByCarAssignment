#include "motor_control.h"
#include <assert.h>
#include <stdio.h>

static void TestConfiguredDeadzoneMapping(void)
{
#if MOTOR_PWM_DEADZONE_ENABLE == 1U
  assert(Motor_MapPWMCommand(0, MOTOR_LEFT_DEADZONE_COUNTS) == 0);
  assert(Motor_MapPWMCommand(80, MOTOR_LEFT_DEADZONE_COUNTS) == 0);
  assert(Motor_MapPWMCommand(-80, MOTOR_LEFT_DEADZONE_COUNTS) == 0);
  assert(Motor_MapPWMCommand(81, MOTOR_LEFT_DEADZONE_COUNTS) == 361);
  assert(Motor_MapPWMCommand(-81, MOTOR_LEFT_DEADZONE_COUNTS) == -361);
  assert(Motor_MapPWMCommand(500, MOTOR_LEFT_DEADZONE_COUNTS) == 763);
  assert(Motor_MapPWMCommand(-500, MOTOR_LEFT_DEADZONE_COUNTS) == -763);
  assert(Motor_MapPWMCommand(6900, MOTOR_LEFT_DEADZONE_COUNTS) == 6900);
  assert(Motor_MapPWMCommand(-6900, MOTOR_LEFT_DEADZONE_COUNTS) == -6900);
#else
  assert(Motor_MapPWMCommand(0, MOTOR_LEFT_DEADZONE_COUNTS) == 0);
  assert(Motor_MapPWMCommand(80, MOTOR_LEFT_DEADZONE_COUNTS) == 80);
  assert(Motor_MapPWMCommand(-80, MOTOR_LEFT_DEADZONE_COUNTS) == -80);
  assert(Motor_MapPWMCommand(500, MOTOR_LEFT_DEADZONE_COUNTS) == 500);
  assert(Motor_MapPWMCommand(-500, MOTOR_LEFT_DEADZONE_COUNTS) == -500);
  assert(Motor_MapPWMCommand(7000, MOTOR_LEFT_DEADZONE_COUNTS) == 6900);
  assert(Motor_MapPWMCommand(-7000, MOTOR_LEFT_DEADZONE_COUNTS) == -6900);
#endif
}

static void TestMonotonicAndSymmetricMapping(void)
{
  int16_t previous = 0;
  int16_t requested;

  for (requested = 0; requested <= MOTOR_PWM_LIMIT_COUNTS; requested++)
  {
    int16_t positive = Motor_MapPWMCommand(requested,
                                           MOTOR_LEFT_DEADZONE_COUNTS);
    int16_t negative = Motor_MapPWMCommand((int16_t)-requested,
                                           MOTOR_LEFT_DEADZONE_COUNTS);

    assert(positive >= previous);
    assert(positive <= MOTOR_PWM_LIMIT_COUNTS);
    assert(negative == (int16_t)-positive);
    previous = positive;
  }
}

int main(void)
{
  TestConfiguredDeadzoneMapping();
  TestMonotonicAndSymmetricMapping();
  puts("motor_pwm_map_tests: PASS");
  return 0;
}
