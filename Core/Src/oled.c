#include "oled.h"
#include "main.h"
#include <stddef.h>
#include <string.h>

#define OLED_CMD                    0U
#define OLED_DATA                   1U
#define OLED_PAGE_COUNT             8U
#define OLED_FONT_WIDTH_PIXELS      5U

static uint8_t oled_gram[OLED_WIDTH_PIXELS][OLED_PAGE_COUNT];

static void OLED_WriteByte(uint8_t value, uint8_t data_mode)
{
  uint8_t bit;

  HAL_GPIO_WritePin(OLED_DC_GPIO_Port, OLED_DC_Pin,
                    (data_mode != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  for (bit = 0U; bit < 8U; ++bit)
  {
    HAL_GPIO_WritePin(OLED_SCLK_GPIO_Port, OLED_SCLK_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(OLED_SDIN_GPIO_Port, OLED_SDIN_Pin,
                      ((value & 0x80U) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(OLED_SCLK_GPIO_Port, OLED_SCLK_Pin, GPIO_PIN_SET);
    value <<= 1;
  }
  HAL_GPIO_WritePin(OLED_DC_GPIO_Port, OLED_DC_Pin, GPIO_PIN_SET);
}

static void OLED_DrawPixel(uint8_t x, uint8_t y, uint8_t on)
{
  uint8_t page;
  uint8_t mask;

  if ((x >= OLED_WIDTH_PIXELS) || (y >= OLED_HEIGHT_PIXELS))
  {
    return;
  }

  /* The WHEELTEC panel is mounted with COM scan direction C0. */
  page = (uint8_t)(7U - (y / 8U));
  mask = (uint8_t)(1U << (7U - (y % 8U)));
  if (on != 0U)
  {
    oled_gram[x][page] |= mask;
  }
  else
  {
    oled_gram[x][page] &= (uint8_t)~mask;
  }
}

static const uint8_t *OLED_FindGlyph(char character)
{
  static const uint8_t blank[OLED_FONT_WIDTH_PIXELS] =
    {0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
  static const uint8_t unknown[OLED_FONT_WIDTH_PIXELS] =
    {0x02U, 0x01U, 0x51U, 0x09U, 0x06U};
  static const uint8_t symbols[][OLED_FONT_WIDTH_PIXELS] =
  {
    {0x08U, 0x08U, 0x3EU, 0x08U, 0x08U}, /* + */
    {0x08U, 0x08U, 0x08U, 0x08U, 0x08U}, /* - */
    {0x00U, 0x60U, 0x60U, 0x00U, 0x00U}, /* . */
    {0x20U, 0x10U, 0x08U, 0x04U, 0x02U}, /* / */
    {0x00U, 0x36U, 0x36U, 0x00U, 0x00U}, /* : */
    {0x40U, 0x40U, 0x40U, 0x40U, 0x40U}  /* _ */
  };
  static const uint8_t digits[10][OLED_FONT_WIDTH_PIXELS] =
  {
    {0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU},
    {0x00U, 0x42U, 0x7FU, 0x40U, 0x00U},
    {0x42U, 0x61U, 0x51U, 0x49U, 0x46U},
    {0x21U, 0x41U, 0x45U, 0x4BU, 0x31U},
    {0x18U, 0x14U, 0x12U, 0x7FU, 0x10U},
    {0x27U, 0x45U, 0x45U, 0x45U, 0x39U},
    {0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U},
    {0x01U, 0x71U, 0x09U, 0x05U, 0x03U},
    {0x36U, 0x49U, 0x49U, 0x49U, 0x36U},
    {0x06U, 0x49U, 0x49U, 0x29U, 0x1EU}
  };
  static const uint8_t uppercase[26][OLED_FONT_WIDTH_PIXELS] =
  {
    {0x7EU, 0x11U, 0x11U, 0x11U, 0x7EU}, /* A */
    {0x7FU, 0x49U, 0x49U, 0x49U, 0x36U}, /* B */
    {0x3EU, 0x41U, 0x41U, 0x41U, 0x22U}, /* C */
    {0x7FU, 0x41U, 0x41U, 0x22U, 0x1CU}, /* D */
    {0x7FU, 0x49U, 0x49U, 0x49U, 0x41U}, /* E */
    {0x7FU, 0x09U, 0x09U, 0x09U, 0x01U}, /* F */
    {0x3EU, 0x41U, 0x49U, 0x49U, 0x7AU}, /* G */
    {0x7FU, 0x08U, 0x08U, 0x08U, 0x7FU}, /* H */
    {0x00U, 0x41U, 0x7FU, 0x41U, 0x00U}, /* I */
    {0x20U, 0x40U, 0x41U, 0x3FU, 0x01U}, /* J */
    {0x7FU, 0x08U, 0x14U, 0x22U, 0x41U}, /* K */
    {0x7FU, 0x40U, 0x40U, 0x40U, 0x40U}, /* L */
    {0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU}, /* M */
    {0x7FU, 0x04U, 0x08U, 0x10U, 0x7FU}, /* N */
    {0x3EU, 0x41U, 0x41U, 0x41U, 0x3EU}, /* O */
    {0x7FU, 0x09U, 0x09U, 0x09U, 0x06U}, /* P */
    {0x3EU, 0x41U, 0x51U, 0x21U, 0x5EU}, /* Q */
    {0x7FU, 0x09U, 0x19U, 0x29U, 0x46U}, /* R */
    {0x46U, 0x49U, 0x49U, 0x49U, 0x31U}, /* S */
    {0x01U, 0x01U, 0x7FU, 0x01U, 0x01U}, /* T */
    {0x3FU, 0x40U, 0x40U, 0x40U, 0x3FU}, /* U */
    {0x1FU, 0x20U, 0x40U, 0x20U, 0x1FU}, /* V */
    {0x3FU, 0x40U, 0x38U, 0x40U, 0x3FU}, /* W */
    {0x63U, 0x14U, 0x08U, 0x14U, 0x63U}, /* X */
    {0x07U, 0x08U, 0x70U, 0x08U, 0x07U}, /* Y */
    {0x61U, 0x51U, 0x49U, 0x45U, 0x43U}  /* Z */
  };
  static const uint8_t lowercase[26][OLED_FONT_WIDTH_PIXELS] =
  {
    {0x20U, 0x54U, 0x54U, 0x54U, 0x78U}, /* a */
    {0x7FU, 0x48U, 0x44U, 0x44U, 0x38U}, /* b */
    {0x38U, 0x44U, 0x44U, 0x44U, 0x20U}, /* c */
    {0x38U, 0x44U, 0x44U, 0x48U, 0x7FU}, /* d */
    {0x38U, 0x54U, 0x54U, 0x54U, 0x18U}, /* e */
    {0x08U, 0x7EU, 0x09U, 0x01U, 0x02U}, /* f */
    {0x0CU, 0x52U, 0x52U, 0x52U, 0x3EU}, /* g */
    {0x7FU, 0x08U, 0x04U, 0x04U, 0x78U}, /* h */
    {0x00U, 0x44U, 0x7DU, 0x40U, 0x00U}, /* i */
    {0x20U, 0x40U, 0x44U, 0x3DU, 0x00U}, /* j */
    {0x7FU, 0x10U, 0x28U, 0x44U, 0x00U}, /* k */
    {0x00U, 0x41U, 0x7FU, 0x40U, 0x00U}, /* l */
    {0x7CU, 0x04U, 0x18U, 0x04U, 0x78U}, /* m */
    {0x7CU, 0x08U, 0x04U, 0x04U, 0x78U}, /* n */
    {0x38U, 0x44U, 0x44U, 0x44U, 0x38U}, /* o */
    {0x7CU, 0x14U, 0x14U, 0x14U, 0x08U}, /* p */
    {0x08U, 0x14U, 0x14U, 0x18U, 0x7CU}, /* q */
    {0x7CU, 0x08U, 0x04U, 0x04U, 0x08U}, /* r */
    {0x48U, 0x54U, 0x54U, 0x54U, 0x20U}, /* s */
    {0x04U, 0x3FU, 0x44U, 0x40U, 0x20U}, /* t */
    {0x3CU, 0x40U, 0x40U, 0x20U, 0x7CU}, /* u */
    {0x1CU, 0x20U, 0x40U, 0x20U, 0x1CU}, /* v */
    {0x3CU, 0x40U, 0x30U, 0x40U, 0x3CU}, /* w */
    {0x44U, 0x28U, 0x10U, 0x28U, 0x44U}, /* x */
    {0x0CU, 0x50U, 0x50U, 0x50U, 0x3CU}, /* y */
    {0x44U, 0x64U, 0x54U, 0x4CU, 0x44U}  /* z */
  };

  if (character == ' ')
  {
    return blank;
  }
  if ((character >= '0') && (character <= '9'))
  {
    return digits[(uint8_t)(character - '0')];
  }
  if ((character >= 'A') && (character <= 'Z'))
  {
    return uppercase[(uint8_t)(character - 'A')];
  }
  if ((character >= 'a') && (character <= 'z'))
  {
    return lowercase[(uint8_t)(character - 'a')];
  }
  switch (character)
  {
    case '+': return symbols[0];
    case '-': return symbols[1];
    case '.': return symbols[2];
    case '/': return symbols[3];
    case ':': return symbols[4];
    case '_': return symbols[5];
    default: return unknown;
  }
}

static void OLED_DrawChar(uint8_t x, uint8_t y, char character)
{
  const uint8_t *glyph = OLED_FindGlyph(character);
  uint8_t column;
  uint8_t row;

  for (column = 0U; column < OLED_FONT_WIDTH_PIXELS; ++column)
  {
    for (row = 0U; row < OLED_TEXT_HEIGHT_PIXELS; ++row)
    {
      OLED_DrawPixel((uint8_t)(x + column), (uint8_t)(y + row),
                     (uint8_t)((glyph[column] >> row) & 0x01U));
    }
  }
}

void OLED_ClearBuffer(void)
{
  (void)memset(oled_gram, 0, sizeof(oled_gram));
}

void OLED_ShowString(uint8_t x, uint8_t y, const char *text)
{
  if (text == NULL)
  {
    return;
  }

  while ((*text != '\0') &&
         (x <= (OLED_WIDTH_PIXELS - OLED_FONT_WIDTH_PIXELS)) &&
         (y <= (OLED_HEIGHT_PIXELS - OLED_TEXT_HEIGHT_PIXELS)))
  {
    OLED_DrawChar(x, y, *text);
    x = (uint8_t)(x + OLED_TEXT_ADVANCE_PIXELS);
    text++;
  }
}

void OLED_Refresh(void)
{
  uint8_t page;
  uint8_t column;

  for (page = 0U; page < OLED_PAGE_COUNT; ++page)
  {
    OLED_WriteByte((uint8_t)(0xB0U + page), OLED_CMD);
    OLED_WriteByte(0x00U, OLED_CMD);
    OLED_WriteByte(0x10U, OLED_CMD);
    for (column = 0U; column < OLED_WIDTH_PIXELS; ++column)
    {
      OLED_WriteByte(oled_gram[column][page], OLED_DATA);
    }
  }
}

void OLED_Init(void)
{
  HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_RESET);
  HAL_Delay(100U);
  HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_SET);

  OLED_WriteByte(0xAEU, OLED_CMD);
  OLED_WriteByte(0xD5U, OLED_CMD);
  OLED_WriteByte(0x50U, OLED_CMD);
  OLED_WriteByte(0xA8U, OLED_CMD);
  OLED_WriteByte(0x3FU, OLED_CMD);
  OLED_WriteByte(0xD3U, OLED_CMD);
  OLED_WriteByte(0x00U, OLED_CMD);
  OLED_WriteByte(0x40U, OLED_CMD);
  OLED_WriteByte(0x8DU, OLED_CMD);
  OLED_WriteByte(0x14U, OLED_CMD);
  OLED_WriteByte(0x20U, OLED_CMD);
  OLED_WriteByte(0x02U, OLED_CMD);
  OLED_WriteByte(0xA1U, OLED_CMD);
  OLED_WriteByte(0xC0U, OLED_CMD);
  OLED_WriteByte(0xDAU, OLED_CMD);
  OLED_WriteByte(0x12U, OLED_CMD);
  OLED_WriteByte(0x81U, OLED_CMD);
  OLED_WriteByte(0xEFU, OLED_CMD);
  OLED_WriteByte(0xD9U, OLED_CMD);
  OLED_WriteByte(0xF1U, OLED_CMD);
  OLED_WriteByte(0xDBU, OLED_CMD);
  OLED_WriteByte(0x30U, OLED_CMD);
  OLED_WriteByte(0xA4U, OLED_CMD);
  OLED_WriteByte(0xA6U, OLED_CMD);
  OLED_WriteByte(0xAFU, OLED_CMD);

  OLED_ClearBuffer();
  OLED_Refresh();
}
