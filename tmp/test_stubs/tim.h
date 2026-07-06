#ifndef TEST_TIM_H
#define TEST_TIM_H
#include <stdint.h>
typedef struct { uint32_t compare[4]; uint16_t counter; } TIM_HandleTypeDef;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim8;
#define TIM_CHANNEL_1 1U
#define TIM_CHANNEL_2 2U
#define TIM_CHANNEL_3 3U
#define TIM_CHANNEL_4 4U
#define TIM_CHANNEL_ALL 0xFFFFU
#define __HAL_TIM_SET_COMPARE(timer, channel, value) ((timer)->compare[(channel) - 1U] = (value))
#define __HAL_TIM_SET_COUNTER(timer, value) ((timer)->counter = (uint16_t)(value))
#define __HAL_TIM_GET_COUNTER(timer) ((timer)->counter)
int HAL_TIM_PWM_Start(TIM_HandleTypeDef *timer, uint32_t channel);
int HAL_TIM_Encoder_Start(TIM_HandleTypeDef *timer, uint32_t channel);
#endif
