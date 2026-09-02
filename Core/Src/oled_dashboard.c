#include "oled_dashboard.h"
#include "line_tracking.h"
#include "oled.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

uint8_t OLED_GrayscaleCodeFromLevels(uint8_t u1_high,
                                     uint8_t u2_high,
                                     uint8_t u3_high,
                                     uint8_t u4_high)
{
  return LineTracking_PackCode(u1_high, u2_high, u3_high, u4_high);
}

void OLED_DashboardFormat(
  const OledDashboardData_t *data,
  char lines[OLED_DASHBOARD_LINE_COUNT][OLED_DASHBOARD_LINE_CHARS + 1U])
{
  uint8_t grayscale_code;

  if ((data == NULL) || (lines == NULL))
  {
    return;
  }

  (void)memset(lines[0], 0, (size_t)OLED_DASHBOARD_LINE_CHARS + 1U);
  grayscale_code = data->grayscale_code & 0x0FU;

  (void)snprintf(lines[0], OLED_DASHBOARD_LINE_CHARS + 1U,
                 "GRAY:%c%c%c%c DIR:%s",
                 ((grayscale_code & 0x08U) != 0U) ? '1' : '0',
                 ((grayscale_code & 0x04U) != 0U) ? '1' : '0',
                 ((grayscale_code & 0x02U) != 0U) ? '1' : '0',
                 ((grayscale_code & 0x01U) != 0U) ? '1' : '0',
                 LineTracking_DirectionText(
                   LineTracking_Decode(grayscale_code)));
}

void OLED_RenderDashboard(const OledDashboardData_t *data)
{
  char lines[OLED_DASHBOARD_LINE_COUNT][OLED_DASHBOARD_LINE_CHARS + 1U];

  if (data == NULL)
  {
    return;
  }

  OLED_DashboardFormat(data, lines);
  OLED_ClearBuffer();
  OLED_ShowString(0U, 0U, lines[0]);
  OLED_Refresh();
}
