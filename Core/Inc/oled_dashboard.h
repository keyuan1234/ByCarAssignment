#ifndef __OLED_DASHBOARD_H
#define __OLED_DASHBOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "balance_types.h"
#include <stdint.h>

#define OLED_DASHBOARD_LINE_COUNT    1U
#define OLED_DASHBOARD_LINE_CHARS   16U

typedef struct
{
  const char *bluetooth_name;
  float pitch_deg;
  float gyro_pitch_dps;
  float left_speed_mm_s;
  float right_speed_mm_s;
  int16_t left_pwm;
  int16_t right_pwm;
  uint16_t distance_mm;
  BalanceState_t state;
  uint8_t grayscale_code;
  uint8_t line_mode_active;
  uint8_t bluetooth_active;
  uint8_t distance_valid;
  uint8_t avoidance_active;
} OledDashboardData_t;

uint8_t OLED_GrayscaleCodeFromLevels(uint8_t u1_high,
                                     uint8_t u2_high,
                                     uint8_t u3_high,
                                     uint8_t u4_high);
void OLED_DashboardFormat(
  const OledDashboardData_t *data,
  char lines[OLED_DASHBOARD_LINE_COUNT][OLED_DASHBOARD_LINE_CHARS + 1U]);
void OLED_RenderDashboard(const OledDashboardData_t *data);

#ifdef __cplusplus
}
#endif

#endif /* __OLED_DASHBOARD_H */
