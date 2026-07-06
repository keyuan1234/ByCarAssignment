/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "usart.h"

/* USER CODE BEGIN 0 */
#include <string.h>

/* 串口接收环形缓冲区 */
volatile uint8_t  uart_rx_buf[128];
volatile uint16_t uart_rx_head = 0;
volatile uint16_t uart_rx_tail = 0;
/* USER CODE END 0 */

UART_HandleTypeDef huart1;

/* USART1 init function */

void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */
  /* 使能UART接收中断（RXNE） */
  __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
  HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
  /* USER CODE END USART1_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspInit 0 */

  /* USER CODE END USART1_MspInit 0 */
    /* USART1 clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN USART1_MspInit 1 */

  /* USER CODE END USART1_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspDeInit 0 */

  /* USER CODE END USART1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART1_CLK_DISABLE();

    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9|GPIO_PIN_10);

  /* USER CODE BEGIN USART1_MspDeInit 1 */

  /* USER CODE END USART1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
#include <stdio.h>
#include <errno.h>

#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

/* 标准库 _sys_write：buf 写入，配合 printf 等标准I/O */
__attribute__((used))
int _sys_write(int fh, const unsigned char *buf, unsigned len, int mode)
{
  (void)fh;
  (void)mode;
  HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, HAL_MAX_DELAY);
  return len;
}

/* USART1 中断服务函数：接收字节存入环形缓冲区 */
void USART1_IRQHandler(void)
{
  if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) != RESET)
  {
    uint8_t ch = (uint8_t)(huart1.Instance->DR & 0xFF);
    uint16_t next = (uart_rx_head + 1) % 128;
    if (next != uart_rx_tail)  /* 缓冲区未满 */
    {
      uart_rx_buf[uart_rx_head] = ch;
      uart_rx_head = next;
    }
    /* 如果满了就丢弃该字节（防止覆盖未读数据） */
  }
  HAL_UART_IRQHandler(&huart1);  /* 处理其他UART中断（错误等） */
}

/* 环形缓冲区可用字节数 */
uint16_t uart_rx_available(void)
{
  return (uart_rx_head - uart_rx_tail) % 128;
}

/* 从环形缓冲区取一个字节 */
uint8_t uart_rx_get_char(void)
{
  uint8_t ch = 0;
  if (uart_rx_available() > 0)
  {
    ch = uart_rx_buf[uart_rx_tail];
    uart_rx_tail = (uart_rx_tail + 1) % 128;
  }
  return ch;
}

/* 清空环形缓冲区 */
void uart_rx_flush(void)
{
  uart_rx_tail = uart_rx_head;
}
/* USER CODE END 1 */
