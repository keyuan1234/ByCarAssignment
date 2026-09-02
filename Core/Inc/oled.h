#ifndef __OLED_H
#define __OLED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define OLED_WIDTH_PIXELS          128U
#define OLED_HEIGHT_PIXELS          64U
#define OLED_TEXT_ADVANCE_PIXELS     8U
#define OLED_TEXT_HEIGHT_PIXELS      7U

void OLED_Init(void);
void OLED_ClearBuffer(void);
void OLED_ShowString(uint8_t x, uint8_t y, const char *text);
void OLED_Refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* __OLED_H */
