#include "pitch_kalman.h"
#include <stddef.h>

void PitchKalman_Reset(PitchKalman_t *filter)
{
  if (filter == NULL)
  {
    return;
  }
  filter->angle_deg = 0.0f;
  filter->gyro_bias_dps = 0.0f;
  filter->p00 = 0.0f;
  filter->p01 = 0.0f;
  filter->p10 = 0.0f;
  filter->p11 = 0.0f;
  filter->initialized = 0U;
}

float PitchKalman_Update(PitchKalman_t *filter,
                         float accel_angle_deg,
                         float gyro_dps,
                         float dt_s,
                         float q_angle,
                         float q_bias,
                         float r_angle,
                         float *corrected_gyro_dps)
{
  float rate;
  float innovation;
  float innovation_covariance;
  float gain_angle;
  float gain_bias;
  float p00_previous;
  float p01_previous;

  if (filter == NULL)
  {
    if (corrected_gyro_dps != NULL)
    {
      *corrected_gyro_dps = 0.0f;
    }
    return accel_angle_deg;
  }

  if (filter->initialized == 0U)
  {
    filter->angle_deg = accel_angle_deg;
    filter->gyro_bias_dps = 0.0f;
    filter->initialized = 1U;
  }

  rate = gyro_dps - filter->gyro_bias_dps;
  filter->angle_deg += dt_s * rate;

  filter->p00 += dt_s * ((dt_s * filter->p11) - filter->p01 -
                         filter->p10 + q_angle);
  filter->p01 -= dt_s * filter->p11;
  filter->p10 -= dt_s * filter->p11;
  filter->p11 += q_bias * dt_s;

  innovation = accel_angle_deg - filter->angle_deg;
  innovation_covariance = filter->p00 + r_angle;
  if (innovation_covariance > 0.0f)
  {
    gain_angle = filter->p00 / innovation_covariance;
    gain_bias = filter->p10 / innovation_covariance;
    filter->angle_deg += gain_angle * innovation;
    filter->gyro_bias_dps += gain_bias * innovation;

    p00_previous = filter->p00;
    p01_previous = filter->p01;
    filter->p00 -= gain_angle * p00_previous;
    filter->p01 -= gain_angle * p01_previous;
    filter->p10 -= gain_bias * p00_previous;
    filter->p11 -= gain_bias * p01_previous;
  }

  rate = gyro_dps - filter->gyro_bias_dps;
  if (corrected_gyro_dps != NULL)
  {
    *corrected_gyro_dps = rate;
  }
  return filter->angle_deg;
}
