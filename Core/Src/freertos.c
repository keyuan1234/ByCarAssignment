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
#include "balance_control.h"
#include "imu.h"
#include "motor_control.h"
#include "usart.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BALANCE_TELEMETRY_PERIOD_MS  50U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for motorTask */
osThreadId_t motorTaskHandle;
const osThreadAttr_t motorTask_attributes = {
  .name = "motorTask",
  .stack_size = 384 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for DataTask */
osThreadId_t DataTaskHandle;
const osThreadAttr_t DataTask_attributes = {
  .name = "DataTask",
  .stack_size = 384 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for MPUQueue */
osMessageQueueId_t MPUQueueHandle;
const osMessageQueueAttr_t MPUQueue_attributes = {
  .name = "MPUQueue"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void App_SendText(const char *text);
static void App_SendBanner(void);
static void App_SendTelemetry(uint32_t now_ms,
                              const BalanceSample_t *sample,
                              const MotorTelemetry_t *telemetry,
                              const BalanceOutput_t *output);
static void App_FormatFloat(char *out, size_t out_size, float value,
                            uint8_t decimals);
static int32_t App_RoundFloat(float value);
static const char *App_StateText(BalanceState_t state);
/* USER CODE END FunctionPrototypes */

void StartmotorTask(void *argument);
void StartDataTask(void *argument);

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

  /* Create the queue(s) */
  /* creation of MPUQueue */
  MPUQueueHandle = osMessageQueueNew (16, sizeof(BalanceSample_t), &MPUQueue_attributes);
  if (MPUQueueHandle == NULL)
  {
    App_SendText("#balance,error,queue_create_failed\r\n");
  }

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of motorTask */
  motorTaskHandle = osThreadNew(StartmotorTask, NULL, &motorTask_attributes);
  if (motorTaskHandle == NULL)
  {
    App_SendText("#balance,error,motor_task_create_failed\r\n");
  }

  /* creation of DataTask */
  DataTaskHandle = osThreadNew(StartDataTask, NULL, &DataTask_attributes);
  if (DataTaskHandle == NULL)
  {
    App_SendText("#balance,error,data_task_create_failed\r\n");
  }

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartmotorTask */
/**
  * @brief  Function implementing the motorTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartmotorTask */
void StartmotorTask(void *argument)
{
  /* USER CODE BEGIN StartmotorTask */
  BalanceController_t controller;
  BalanceSample_t latest_sample;
  BalanceOutput_t output;
  const MotorTelemetry_t *telemetry;
  uint32_t next_wake;
  uint32_t last_telemetry_ms = 0U;
  uint8_t have_sample = 0U;
  osStatus_t queue_status;

  (void)argument;
  memset(&latest_sample, 0, sizeof(latest_sample));
  memset(&output, 0, sizeof(output));
  output.state = BALANCE_STATE_TIMEOUT;

  Motor_Init();
  BalanceControl_Init(&controller);
  App_SendBanner();
  next_wake = osKernelGetTickCount();

  /* Infinite loop */
  for(;;)
  {
    uint32_t now_ms;

    do
    {
      queue_status = osMessageQueueGet(MPUQueueHandle, &latest_sample, NULL, 0U);
      if (queue_status == osOK)
      {
        have_sample = 1U;
      }
    } while (queue_status == osOK);

    now_ms = HAL_GetTick();
    Encoder_Update10ms();
    telemetry = Motor_GetTelemetry();

    if (have_sample == 0U)
    {
      BalanceControl_Reset(&controller);
      Motor_Brake();
      memset(&output, 0, sizeof(output));
      output.state = BALANCE_STATE_TIMEOUT;
    }
    else if (latest_sample.valid == 0U)
    {
      BalanceControl_Reset(&controller);
      Motor_Brake();
      memset(&output, 0, sizeof(output));
      output.state = BALANCE_STATE_IMU_ERROR;
    }
    else if ((now_ms - latest_sample.timestamp_ms) > BALANCE_SAMPLE_TIMEOUT_MS)
    {
      BalanceControl_Reset(&controller);
      Motor_Brake();
      memset(&output, 0, sizeof(output));
      output.state = BALANCE_STATE_TIMEOUT;
    }
    else
    {
      output = BalanceControl_Update(&controller, &latest_sample,
                                     telemetry->left_delta,
                                     telemetry->right_delta);
      if (output.state == BALANCE_STATE_RUNNING)
      {
        Motor_SetPWM(output.left_pwm, output.right_pwm);
      }
      else
      {
        Motor_Brake();
      }
    }

    if ((now_ms - last_telemetry_ms) >= BALANCE_TELEMETRY_PERIOD_MS)
    {
      App_SendTelemetry(now_ms, &latest_sample, telemetry, &output);
      last_telemetry_ms = now_ms;
    }

    next_wake += MOTOR_CONTROL_PERIOD_MS;
    (void)osDelayUntil(next_wake);
  }
  /* USER CODE END StartmotorTask */
}

/* USER CODE BEGIN Header_StartDataTask */
/**
* @brief Function implementing the DataTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDataTask */
void StartDataTask(void *argument)
{
  /* USER CODE BEGIN StartDataTask */
  BalanceSample_t sample;
  BalanceSample_t dropped_sample;
  uint32_t next_wake;

  (void)argument;
  memset(&sample, 0, sizeof(sample));

  if (Imu_Init() == 0U)
  {
    App_SendText("#balance,error,imu_init_failed\r\n");
    for (;;)
    {
      sample.timestamp_ms = HAL_GetTick();
      sample.valid = 0U;
      if (osMessageQueuePut(MPUQueueHandle, &sample, 0U, 0U) != osOK)
      {
        (void)osMessageQueueGet(MPUQueueHandle, &dropped_sample, NULL, 0U);
        (void)osMessageQueuePut(MPUQueueHandle, &sample, 0U, 0U);
      }
      osDelay(100U);
    }
  }

  App_SendText("#balance,imu_init_ok\r\n");
  next_wake = osKernelGetTickCount();

  /* Infinite loop */
  for(;;)
  {
    (void)Imu_UpdateComplementary5ms(&sample);
    if (osMessageQueuePut(MPUQueueHandle, &sample, 0U, 0U) != osOK)
    {
      (void)osMessageQueueGet(MPUQueueHandle, &dropped_sample, NULL, 0U);
      (void)osMessageQueuePut(MPUQueueHandle, &sample, 0U, 0U);
    }

    next_wake += IMU_SAMPLE_PERIOD_MS;
    (void)osDelayUntil(next_wake);
  }
  /* USER CODE END StartDataTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
static void App_SendText(const char *text)
{
  if (text == NULL)
  {
    return;
  }
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)text,
                          (uint16_t)strlen(text), 100U);
}

static void App_SendBanner(void)
{
  App_SendText("#balance,ByCarStable,cascade_pid_stand\r\n");
  App_SendText("#map,pwm_l=TIM3_CH1_CH2,pwm_r=TIM3_CH3_CH4,enc_l=TIM4,enc_r=TIM8,imu=PB14_PB15\r\n");
  App_SendText("ms,pitch,gyro,enc_l,enc_r,balance_pwm,velocity_pwm,pwm_l,pwm_r,state\r\n");
}

static void App_SendTelemetry(uint32_t now_ms,
                              const BalanceSample_t *sample,
                              const MotorTelemetry_t *telemetry,
                              const BalanceOutput_t *output)
{
  char line[192];
  char pitch_text[18];
  char gyro_text[18];
  float pitch = 0.0f;
  float gyro = 0.0f;

  if ((sample != NULL) && (sample->valid != 0U))
  {
    pitch = sample->pitch_deg;
    gyro = sample->gyro_pitch_dps;
  }
  App_FormatFloat(pitch_text, sizeof(pitch_text), pitch, 2U);
  App_FormatFloat(gyro_text, sizeof(gyro_text), gyro, 2U);

  (void)snprintf(line, sizeof(line),
                 "%lu,%s,%s,%d,%d,%d,%d,%d,%d,%s\r\n",
                 (unsigned long)now_ms,
                 pitch_text,
                 gyro_text,
                 (telemetry == NULL) ? 0 : telemetry->left_delta,
                 (telemetry == NULL) ? 0 : telemetry->right_delta,
                 (output == NULL) ? 0 : output->balance_pwm,
                 (output == NULL) ? 0 : output->velocity_pwm,
                 (output == NULL) ? 0 : output->left_pwm,
                 (output == NULL) ? 0 : output->right_pwm,
                 (output == NULL) ? App_StateText(BALANCE_STATE_STOPPED) :
                                    App_StateText(output->state));
  App_SendText(line);
}

static void App_FormatFloat(char *out, size_t out_size, float value,
                            uint8_t decimals)
{
  int32_t scale = 1;
  int32_t scaled;
  int32_t whole;
  int32_t fraction;
  uint8_t i;

  if ((out == NULL) || (out_size == 0U))
  {
    return;
  }

  for (i = 0U; i < decimals; i++)
  {
    scale *= 10;
  }

  scaled = App_RoundFloat(value * (float)scale);
  if (scaled < 0)
  {
    whole = (-scaled) / scale;
    fraction = (-scaled) % scale;
    (void)snprintf(out, out_size, "-%ld.%0*ld",
                   (long)whole, decimals, (long)fraction);
  }
  else
  {
    whole = scaled / scale;
    fraction = scaled % scale;
    (void)snprintf(out, out_size, "%ld.%0*ld",
                   (long)whole, decimals, (long)fraction);
  }
}

static int32_t App_RoundFloat(float value)
{
  if (value >= 0.0f)
  {
    return (int32_t)(value + 0.5f);
  }
  return (int32_t)(value - 0.5f);
}

static const char *App_StateText(BalanceState_t state)
{
  switch (state)
  {
    case BALANCE_STATE_RUNNING:
      return "RUN";
    case BALANCE_STATE_IMU_ERROR:
      return "IMU_ERROR";
    case BALANCE_STATE_TILT:
      return "TILT";
    case BALANCE_STATE_TIMEOUT:
      return "TIMEOUT";
    case BALANCE_STATE_STOPPED:
    default:
      return "STOPPED";
  }
}

/* USER CODE END Application */
