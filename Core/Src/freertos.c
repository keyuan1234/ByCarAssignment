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
  float integral;
  float last_error;
  float output_min;
  float output_max;
} PID_HandleTypeDef;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

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
float PID_Calculate(PID_HandleTypeDef *pid, float error, float dt);
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
  printf("System initialized\n");
  for(;;)
  {
    printf("Speed A: %.1f, Target A: %.1f, Speed B: %.1f, Target B: %.1f\r\n", 
           current_speed_A, speed_target_A, current_speed_B, speed_target_B);
    osDelay(1000);
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
  int16_t cnt4, cnt8;
  for(;;)
  {
    cnt4 = (__HAL_TIM_GET_COUNTER(&htim4));
    cnt8 = (__HAL_TIM_GET_COUNTER(&htim8));
    encoder_count_A = (int32_t)cnt4;
    encoder_count_B = (int32_t)cnt8;
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
  float pwm_a, pwm_b;
  uint32_t cmp1, cmp2, cmp3, cmp4;
  for(;;)
  {
    pwm_a = motor_pwm_A;
    pwm_b = motor_pwm_B;
    cmp1 = cmp2 = cmp3 = cmp4 = 0;
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
  int32_t last_count_A = 0, last_count_B = 0;
  int32_t delta_A, delta_B;
  float speed_A, speed_B;
  float error_A, error_B;
  float output_A, output_B;
  const float dt = 0.01f;
  for(;;)
  {
    delta_A = encoder_count_A - last_count_A;
    delta_B = encoder_count_B - last_count_B;
    last_count_A = encoder_count_A;
    last_count_B = encoder_count_B;
    speed_A = delta_A / dt;
    speed_B = delta_B / dt;
    current_speed_A = speed_A;
    current_speed_B = speed_B;
    error_A = speed_target_A - speed_A;
    error_B = speed_target_B - speed_B;
    output_A = PID_Calculate(&pid_A, error_A, dt);
    output_B = PID_Calculate(&pid_B, error_B, dt);
    motor_pwm_A = output_A;
    motor_pwm_B = output_B;
    osDelay(10);
  }
  /* USER CODE END StartTaskControlSpeed */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
float PID_Calculate(PID_HandleTypeDef *pid, float error, float dt)
{
  float output;
  pid->integral += error * dt;
  if (pid->integral > pid->output_max) {
    pid->integral = pid->output_max;
  } else if (pid->integral < pid->output_min) {
    pid->integral = pid->output_min;
  }
  output = pid->Kp * error + pid->Ki * pid->integral + pid->Kd * (error - pid->last_error) / dt;
  pid->last_error = error;
  if (output > pid->output_max) {
    output = pid->output_max;
  } else if (output < pid->output_min) {
    output = pid->output_min;
  }
  return output;
}

void PID_Reset(PID_HandleTypeDef *pid)
{
  pid->integral = 0;
  pid->last_error = 0;
}
/* USER CODE END Application */

