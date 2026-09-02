#include "pitch_kalman.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

#define DT       0.005f
#define Q_ANGLE  0.001f
#define Q_BIAS   0.003f
#define R_ANGLE  0.5f

static void TestInitialAngle(void)
{
  PitchKalman_t filter;
  float corrected = 99.0f;
  float angle;

  PitchKalman_Reset(&filter);
  angle = PitchKalman_Update(&filter, 12.5f, 0.0f, DT,
                             Q_ANGLE, Q_BIAS, R_ANGLE, &corrected);
  assert(fabsf(angle - 12.5f) < 0.01f);
  assert(fabsf(corrected) < 0.01f);
}

static void TestGyroBiasConvergence(void)
{
  PitchKalman_t filter;
  float corrected = 0.0f;
  float angle = 0.0f;
  int i;

  PitchKalman_Reset(&filter);
  for (i = 0; i < 5000; ++i)
  {
    angle = PitchKalman_Update(&filter, 0.0f, 2.0f, DT,
                               Q_ANGLE, Q_BIAS, R_ANGLE, &corrected);
  }
  assert(fabsf(angle) < 0.1f);
  assert(fabsf(corrected) < 0.1f);
  assert(fabsf(filter.gyro_bias_dps - 2.0f) < 0.1f);
}

static void TestMovingAngleTracking(void)
{
  PitchKalman_t filter;
  float corrected = 0.0f;
  float measured_angle = 0.0f;
  float estimate = 0.0f;
  int i;

  PitchKalman_Reset(&filter);
  for (i = 0; i < 200; ++i)
  {
    measured_angle += 10.0f * DT;
    estimate = PitchKalman_Update(&filter, measured_angle, 10.0f, DT,
                                  Q_ANGLE, Q_BIAS, R_ANGLE, &corrected);
  }
  assert(fabsf(estimate - 10.0f) < 0.2f);
  assert(fabsf(corrected - 10.0f) < 0.2f);
}

int main(void)
{
  TestInitialAngle();
  TestGyroBiasConvergence();
  TestMovingAngleTracking();
  puts("pitch_kalman_tests: PASS");
  return 0;
}
