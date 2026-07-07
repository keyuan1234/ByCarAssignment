/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "tim.h"
#include "usart.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
  float Kp;
  float Ki;
  float Kd;
  float last_error;  /* e[k-1]，增量式PI用 */
  float output;      /* 增量式PI累加输出值 u(k) */
  float output_min;
  float output_max;
} PID_HandleTypeDef;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 编码器与车轮物理参数（按实际情况修改） */
#define ENCODER_PPR_PHYSICAL     2000        /* 编码器物理线数（转一圈的AB周期数） */
#define ENCODER_QUADRATURE       4           /* 4倍频 (TIM_ENCODERMODE_TI12) */
#define ENCODER_PPR              (ENCODER_PPR_PHYSICAL * ENCODER_QUADRATURE) /* 4倍频后 8000计数/转 */
#define GEAR_RATIO               7.5f        /* 电机减速比（待实测确认精确值） */
#define WHEEL_DIAMETER_MM        70.0f       /* 轮子直径 mm（量完修改） */
#define WHEEL_CIRCUMFERENCE_MM   (3.1415926f * WHEEL_DIAMETER_MM) /* 周长 */
#define SPEED_DT                 0.01f       /* 速度测量周期 10ms */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
volatile int32_t encoder_count_A = 0;
volatile int32_t encoder_count_B = 0;
volatile float motor_pwm_A = 0;
volatile float motor_pwm_B = 0;
volatile float speed_target_A = 0;
volatile float speed_target_B = 0;
volatile float current_speed_A = 0;
volatile float current_speed_B = 0;
PID_HandleTypeDef pid_A = {1.0f, 0.1f, 0.05f, 0, 0, -100.0f, 100.0f};
PID_HandleTypeDef pid_B = {1.0f, 0.1f, 0.05f, 0, 0, -100.0f, 100.0f};

/* 编码器累计脉冲（4倍频值，用于显示） */
volatile int32_t encoder_accum_A = 0;
volatile int32_t encoder_accum_B = 0;
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for EncoderTask */
osThreadId_t EncoderTaskHandle;
const osThreadAttr_t EncoderTask_attributes = {
  .name = "EncoderTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for ControlMotorTas */
osThreadId_t ControlMotorTasHandle;
const osThreadAttr_t ControlMotorTas_attributes = {
  .name = "ControlMotorTas",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for ControlSpeedTas */
osThreadId_t ControlSpeedTasHandle;
const osThreadAttr_t ControlSpeedTas_attributes = {
  .name = "ControlSpeedTas",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
float PID_Incremental(PID_HandleTypeDef *pid, float error);
void PID_Reset(PID_HandleTypeDef *pid);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartTaskEncoder(void *argument);
void StartTaskControlMotor(void *argument);
void StartTaskControlSpeed(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of EncoderTask */
  EncoderTaskHandle = osThreadNew(StartTaskEncoder, NULL, &EncoderTask_attributes);

  /* creation of ControlMotorTas */
  ControlMotorTasHandle = osThreadNew(StartTaskControlMotor, NULL, &ControlMotorTas_attributes);

  /* creation of ControlSpeedTas */
  ControlSpeedTasHandle = osThreadNew(StartTaskControlSpeed, NULL, &ControlSpeedTas_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  int print_counter = 0;
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
  HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
  HAL_TIM_Encoder_Start(&htim8, TIM_CHANNEL_ALL);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, 0);
  printf("系统初始化完成\n");
  for(;;)
  {
    /* 每10次（1秒）打印一次状态 */
    if (++print_counter >= 10) {
      print_counter = 0;
      printf("A_Speed:%.1fmm/s Pulse:%ld | B_Speed:%.1fmm/s Pulse:%ld\r\n",
             current_speed_A, (long)(encoder_accum_A / (ENCODER_QUADRATURE * GEAR_RATIO)),
             current_speed_B, (long)(encoder_accum_B / (ENCODER_QUADRATURE * GEAR_RATIO)));
    }
    osDelay(100);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartTaskEncoder */
/**
* @brief Function implementing the EncoderTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskEncoder */
void StartTaskEncoder(void *argument)
{
  /* USER CODE BEGIN StartTaskEncoder */
  for(;;)
  {
    /* int16_t读取：硬件CNT为16位，用有符号数直接获得带方向的值 */
    encoder_count_A = (int16_t)(__HAL_TIM_GET_COUNTER(&htim4));
    encoder_count_B = (int16_t)(__HAL_TIM_GET_COUNTER(&htim8));
    osDelay(1);
  }
  /* USER CODE END StartTaskEncoder */
}

/* USER CODE BEGIN Header_StartTaskControlMotor */
/**
* @brief Function implementing the ControlMotorTas thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskControlMotor */
void StartTaskControlMotor(void *argument)
{
  /* USER CODE BEGIN StartTaskControlMotor */
  uint32_t cmp1, cmp2, cmp3, cmp4;
  for(;;)
  {
    cmp1 = cmp2 = cmp3 = cmp4 = 0;
    float pwm_a = motor_pwm_A;
    float pwm_b = motor_pwm_B;
    if (pwm_a > 0) {
      cmp1 = (uint32_t)(pwm_a * 655.35f);
      cmp2 = 0;
    } else {
      cmp1 = 0;
      cmp2 = (uint32_t)(-pwm_a * 655.35f);
    }
    if (pwm_b > 0) {
      cmp3 = (uint32_t)(pwm_b * 655.35f);
      cmp4 = 0;
    } else {
      cmp3 = 0;
      cmp4 = (uint32_t)(-pwm_b * 655.35f);
    }
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, cmp1);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, cmp2);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, cmp3);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, cmp4);
    osDelay(1);
  }
  /* USER CODE END StartTaskControlMotor */
}

/* USER CODE BEGIN Header_StartTaskControlSpeed */
/**
* @brief Function implementing the ControlSpeedTas thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskControlSpeed */
void StartTaskControlSpeed(void *argument)
{
  /* USER CODE BEGIN StartTaskControlSpeed */
  int16_t last_raw_A = 0, last_raw_B = 0;
  int16_t raw_A, raw_B;
  int32_t delta_A, delta_B;
  float speed_A, speed_B;
  for(;;)
  {
    /* 取编码器原始值（带符号16位） */
    raw_A = (int16_t)encoder_count_A;
    raw_B = (int16_t)encoder_count_B;

    /* 用int16_t减法+强制转换，自动处理16位计数器溢出回绕 */
    delta_A = (int32_t)(int16_t)(raw_A - last_raw_A);
    delta_B = (int32_t)(int16_t)(raw_B - last_raw_B);
    last_raw_A = raw_A;
    last_raw_B = raw_B;

    /* 累计编码器脉冲（4倍频值） */
    encoder_accum_A += delta_A;
    encoder_accum_B += delta_B;

    /* 速度换算：脉冲增量 → 脉冲/秒 → mm/s */
    speed_A = (delta_A / SPEED_DT) / ENCODER_PPR * WHEEL_CIRCUMFERENCE_MM;
    speed_B = (delta_B / SPEED_DT) / ENCODER_PPR * WHEEL_CIRCUMFERENCE_MM;
    current_speed_A = speed_A;
    current_speed_B = speed_B;

    /* 增量式PI闭环控制 */
    float error_A = speed_target_A - speed_A;
    float error_B = speed_target_B - speed_B;
    motor_pwm_A = PID_Incremental(&pid_A, error_A);
    motor_pwm_B = PID_Incremental(&pid_B, error_B);

    osDelay(10);
  }
  /* USER CODE END StartTaskControlSpeed */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* 增量式PI：Δu = Kp*(e[k]-e[k-1]) + Ki*e[k], u(k) = u(k-1) + Δu */
float PID_Incremental(PID_HandleTypeDef *pid, float error)
{
  float delta_u;
  delta_u = pid->Kp * (error - pid->last_error) + pid->Ki * error;
  pid->last_error = error;
  pid->output += delta_u;
  /* 输出限幅 */
  if (pid->output > pid->output_max) {
    pid->output = pid->output_max;
  } else if (pid->output < pid->output_min) {
    pid->output = pid->output_min;
  }
  return pid->output;
}

void PID_Reset(PID_HandleTypeDef *pid)
{
  pid->last_error = 0;
  pid->output = 0;
}
/* USER CODE END Application */
