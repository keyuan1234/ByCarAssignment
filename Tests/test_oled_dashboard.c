#include "oled_dashboard.h"
#include "oled.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned int clear_count;
static unsigned int refresh_count;
static unsigned int show_count;
static uint8_t shown_x[OLED_DASHBOARD_LINE_COUNT];
static uint8_t shown_y[OLED_DASHBOARD_LINE_COUNT];
static char shown_text[OLED_DASHBOARD_LINE_COUNT]
                      [OLED_DASHBOARD_LINE_CHARS + 1U];

void OLED_Init(void)
{
}

void OLED_ClearBuffer(void)
{
  clear_count++;
}

void OLED_ShowString(uint8_t x, uint8_t y, const char *text)
{
  assert(show_count < OLED_DASHBOARD_LINE_COUNT);
  shown_x[show_count] = x;
  shown_y[show_count] = y;
  (void)snprintf(shown_text[show_count], sizeof(shown_text[show_count]),
                 "%s", text);
  show_count++;
}

void OLED_Refresh(void)
{
  refresh_count++;
}

static OledDashboardData_t MakeDashboard(void)
{
  OledDashboardData_t data;

  (void)memset(&data, 0, sizeof(data));
  data.bluetooth_name = "ByCar321";
  data.pitch_deg = 12.34f;
  data.gyro_pitch_dps = -123.6f;
  data.left_speed_mm_s = -12.6f;
  data.right_speed_mm_s = 1200.0f;
  data.left_pwm = -6900;
  data.right_pwm = 6900;
  data.distance_mm = 42U;
  data.state = BALANCE_STATE_RUNNING;
  data.grayscale_code = 0x07U;
  data.distance_valid = 1U;
  data.avoidance_active = 1U;
  return data;
}

static void TestNormalDashboard(void)
{
  OledDashboardData_t data = MakeDashboard();
  char lines[OLED_DASHBOARD_LINE_COUNT][OLED_DASHBOARD_LINE_CHARS + 1U];

  OLED_DashboardFormat(&data, lines);
  assert(strcmp(lines[0], "GRAY:0111 DIR:L") == 0);
}

static void TestLinkInvalidDistanceAndSaturation(void)
{
  OledDashboardData_t data = MakeDashboard();
  char lines[OLED_DASHBOARD_LINE_COUNT][OLED_DASHBOARD_LINE_CHARS + 1U];

  data.bluetooth_active = 1U;
  data.line_mode_active = 1U;
  data.pitch_deg = 120.0f;
  data.gyro_pitch_dps = -1200.0f;
  data.left_speed_mm_s = -1200.0f;
  data.right_speed_mm_s = 1200.0f;
  data.left_pwm = -10000;
  data.right_pwm = 10000;
  data.distance_valid = 0U;
  data.avoidance_active = 0U;
  data.state = BALANCE_STATE_IMU_ERROR;
  data.grayscale_code = 0x0FU;

  OLED_DashboardFormat(&data, lines);
  assert(strcmp(lines[0], "GRAY:1111 DIR:M") == 0);
}

static void TestGrayscaleBitOrder(void)
{
  assert(OLED_GrayscaleCodeFromLevels(1U, 0U, 0U, 0U) == 0x01U);
  assert(OLED_GrayscaleCodeFromLevels(0U, 1U, 0U, 0U) == 0x02U);
  assert(OLED_GrayscaleCodeFromLevels(0U, 0U, 1U, 0U) == 0x04U);
  assert(OLED_GrayscaleCodeFromLevels(0U, 0U, 0U, 1U) == 0x08U);
  assert(OLED_GrayscaleCodeFromLevels(1U, 1U, 1U, 0U) == 0x07U);
  assert(OLED_GrayscaleCodeFromLevels(1U, 0U, 0U, 1U) == 0x09U);
}

static void TestGrayscaleMappings(void)
{
  static const struct
  {
    uint8_t code;
    const char *expected;
  } cases[] =
  {
    {0x07U, "GRAY:0111 DIR:L"},
    {0x09U, "GRAY:1001 DIR:M"},
    {0x0EU, "GRAY:1110 DIR:R"},
    {0x0BU, "GRAY:1011 DIR:SL"},
    {0x0DU, "GRAY:1101 DIR:SR"}
  };
  OledDashboardData_t data = MakeDashboard();
  char lines[OLED_DASHBOARD_LINE_COUNT][OLED_DASHBOARD_LINE_CHARS + 1U];
  size_t index;
  uint8_t code;

  for (index = 0U; index < (sizeof(cases) / sizeof(cases[0])); ++index)
  {
    data.grayscale_code = cases[index].code;
    OLED_DashboardFormat(&data, lines);
    assert(strcmp(lines[0], cases[index].expected) == 0);
    assert(strlen(lines[0]) <= OLED_DASHBOARD_LINE_CHARS);
  }

  for (code = 0U; code < 16U; ++code)
  {
    if ((code != 0x07U) && (code != 0x09U) && (code != 0x0EU) &&
        (code != 0x0BU) && (code != 0x0DU) && (code != 0x0FU))
    {
      data.grayscale_code = code;
      OLED_DashboardFormat(&data, lines);
      assert(strstr(lines[0], " DIR:N") != NULL);
      assert(strlen(lines[0]) <= OLED_DASHBOARD_LINE_CHARS);
    }
  }
}

static void TestRenderCoordinates(void)
{
  OledDashboardData_t data = MakeDashboard();
  unsigned int line;

  clear_count = 0U;
  refresh_count = 0U;
  show_count = 0U;
  (void)memset(shown_text, 0, sizeof(shown_text));
  OLED_RenderDashboard(&data);

  assert(clear_count == 1U);
  assert(refresh_count == 1U);
  assert(show_count == OLED_DASHBOARD_LINE_COUNT);
  for (line = 0U; line < OLED_DASHBOARD_LINE_COUNT; ++line)
  {
    assert(shown_x[line] == 0U);
    assert(shown_y[line] == (uint8_t)(line * 10U));
    assert(strlen(shown_text[line]) <= OLED_DASHBOARD_LINE_CHARS);
  }
}

int main(void)
{
  TestNormalDashboard();
  TestLinkInvalidDistanceAndSaturation();
  TestGrayscaleBitOrder();
  TestGrayscaleMappings();
  TestRenderCoordinates();
  puts("oled_dashboard_tests: PASS");
  return 0;
}
