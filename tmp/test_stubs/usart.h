#ifndef TEST_USART_H
#define TEST_USART_H
#include <stdint.h>
typedef struct { int unused; } UART_HandleTypeDef;
extern UART_HandleTypeDef huart1;
int HAL_UART_Transmit(UART_HandleTypeDef *uart, uint8_t *data, uint16_t size, uint32_t timeout);
#endif
