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
#include "motor_control.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MOTOR_TASK_PERIOD_MS          10U
#define OPEN_LOOP_RUN_MS              3000U
#define OPEN_LOOP_STOP_MS             1000U
#define DEADZONE_DWELL_MS             1000U
#define DEADZONE_MAX_PERCENT          50U
#define DEADZONE_MIN_WINDOW_COUNTS    3U
#define PI_STEP_DELAY_MS              1000U
#define PI_TARGET_MM_S                200

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for motorTask */
osThreadId_t motorTaskHandle;
const osThreadAttr_t motorTask_attributes = {
  .name = "motorTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void MotorTask_SendText(const char *text);
static void MotorTask_SendBanner(float kp, float ki);
static void MotorTask_SendTelemetry(uint32_t elapsed_ms, int32_t target_left,
                                    int32_t target_right);
static int32_t MotorTask_RoundFloat(float value);

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartmotorTask(void *argument);

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

  /* creation of motorTask */
  motorTaskHandle = osThreadNew(StartmotorTask, NULL, &motorTask_attributes);

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
  (void)argument;
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartmotorTask */
/**
* @brief Function implementing the motorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartmotorTask */
void StartmotorTask(void *argument)
{
  /* USER CODE BEGIN StartmotorTask */
#if MOTOR_EXPERIMENT_MODE == MOTOR_EXPERIMENT_OPEN_LOOP
  static const uint8_t open_loop_percent[] = {20U, 35U, 50U, 65U, 80U};
#endif
#if MOTOR_EXPERIMENT_MODE == MOTOR_EXPERIMENT_PI_CLOSED_LOOP
  PIController_t left_pi;
  PIController_t right_pi;
#endif
  uint32_t next_wake;
  uint32_t elapsed_ms = 0U;
  int16_t left_pwm = 0;
  int16_t right_pwm = 0;
  int32_t target_left = 0;
  int32_t target_right = 0;
  float kp;
  float ki;

  (void)argument;

#if MOTOR_PI_PARAMETER_SET == 0U
  kp = 2.0f;
  ki = 15.0f;
#elif MOTOR_PI_PARAMETER_SET == 1U
  kp = 4.0f;
  ki = 30.0f;
#elif MOTOR_PI_PARAMETER_SET == 2U
  kp = 6.0f;
  ki = 45.0f;
#else
#error "MOTOR_PI_PARAMETER_SET must be 0, 1, or 2"
#endif

  Motor_Init();
#if MOTOR_EXPERIMENT_MODE == MOTOR_EXPERIMENT_PI_CLOSED_LOOP
  PI_Init(&left_pi, kp, ki);
  PI_Init(&right_pi, kp, ki);
#endif
  MotorTask_SendBanner(kp, ki);
  next_wake = osKernelGetTickCount();

  for(;;)
  {
    const MotorTelemetry_t *telemetry;

    Encoder_Update10ms();
    telemetry = Motor_GetTelemetry();
    (void)telemetry;

#if MOTOR_EXPERIMENT_MODE == MOTOR_EXPERIMENT_OPEN_LOOP
    {
      uint32_t stage = elapsed_ms / (OPEN_LOOP_RUN_MS + OPEN_LOOP_STOP_MS);
      uint32_t stage_time = elapsed_ms % (OPEN_LOOP_RUN_MS + OPEN_LOOP_STOP_MS);
      if ((stage < (sizeof(open_loop_percent) / sizeof(open_loop_percent[0]))) &&
          (stage_time < OPEN_LOOP_RUN_MS))
      {
        left_pwm = (int16_t)((uint32_t)open_loop_percent[stage] *
                             MOTOR_PWM_PERIOD_COUNTS / 100U);
        right_pwm = left_pwm;
      }
      else
      {
        left_pwm = 0;
        right_pwm = 0;
      }
    }
#elif MOTOR_EXPERIMENT_MODE == MOTOR_EXPERIMENT_DEADZONE
    {
      static uint8_t side = 0U;
      static uint8_t duty_percent = 1U;
      static uint16_t sample_count = 0U;
      static uint32_t movement_count = 0U;
      static uint16_t pause_samples = 0U;
      char event_line[96];

      left_pwm = 0;
      right_pwm = 0;
      if (side < 2U)
      {
        if (pause_samples > 0U)
        {
          pause_samples--;
        }
        else
        {
          int16_t scan_pwm = (int16_t)((uint32_t)duty_percent *
                                       MOTOR_PWM_PERIOD_COUNTS / 100U);
          int16_t delta = (side == 0U) ? telemetry->left_delta : telemetry->right_delta;
          if (side == 0U)
          {
            left_pwm = scan_pwm;
          }
          else
          {
            right_pwm = scan_pwm;
          }
          movement_count += (delta < 0) ? (uint32_t)(-delta) : (uint32_t)delta;
          sample_count++;

          if (sample_count >= (DEADZONE_DWELL_MS / MOTOR_TASK_PERIOD_MS))
          {
            if (movement_count >= DEADZONE_MIN_WINDOW_COUNTS)
            {
              (void)snprintf(event_line, sizeof(event_line),
                             "#deadzone,%s,%u,%u\r\n",
                             (side == 0U) ? "left" : "right",
                             duty_percent, (unsigned int)scan_pwm);
              MotorTask_SendText(event_line);
              side++;
              duty_percent = 1U;
              pause_samples = (uint16_t)(OPEN_LOOP_STOP_MS / MOTOR_TASK_PERIOD_MS);
            }
            else if (duty_percent >= DEADZONE_MAX_PERCENT)
            {
              (void)snprintf(event_line, sizeof(event_line),
                             "#deadzone,%s,not_found,0\r\n",
                             (side == 0U) ? "left" : "right");
              MotorTask_SendText(event_line);
              side++;
              duty_percent = 1U;
              pause_samples = (uint16_t)(OPEN_LOOP_STOP_MS / MOTOR_TASK_PERIOD_MS);
            }
            else
            {
              duty_percent++;
            }
            sample_count = 0U;
            movement_count = 0U;
          }
        }
      }
    }
#elif MOTOR_EXPERIMENT_MODE == MOTOR_EXPERIMENT_ENCODER_VERIFY
    {
      static uint8_t left_reported = 0U;
      static uint8_t right_reported = 0U;
      left_pwm = 0;
      right_pwm = 0;
      if ((left_reported == 0U) &&
          ((telemetry->left_total >= (int32_t)MOTOR_ENCODER_COUNTS_PER_REV) ||
           (telemetry->left_total <= -(int32_t)MOTOR_ENCODER_COUNTS_PER_REV)))
      {
        MotorTask_SendText("#encoder,left,reached_1560\r\n");
        left_reported = 1U;
      }
      if ((right_reported == 0U) &&
          ((telemetry->right_total >= (int32_t)MOTOR_ENCODER_COUNTS_PER_REV) ||
           (telemetry->right_total <= -(int32_t)MOTOR_ENCODER_COUNTS_PER_REV)))
      {
        MotorTask_SendText("#encoder,right,reached_1560\r\n");
        right_reported = 1U;
      }
    }
#elif MOTOR_EXPERIMENT_MODE == MOTOR_EXPERIMENT_PI_CLOSED_LOOP
    if (elapsed_ms >= PI_STEP_DELAY_MS)
    {
      target_left = PI_TARGET_MM_S;
      target_right = PI_TARGET_MM_S;
    }
    else
    {
      target_left = 0;
      target_right = 0;
    }
    left_pwm = PI_Update(&left_pi, (float)target_left,
                         telemetry->left_filtered_mm_s);
    right_pwm = PI_Update(&right_pi, (float)target_right,
                          telemetry->right_filtered_mm_s);
#else
#error "Invalid MOTOR_EXPERIMENT_MODE"
#endif

    Motor_SetPWM(left_pwm, right_pwm);
    MotorTask_SendTelemetry(elapsed_ms, target_left, target_right);
    elapsed_ms += MOTOR_TASK_PERIOD_MS;
    next_wake += MOTOR_TASK_PERIOD_MS;
    (void)osDelayUntil(next_wake);
  }
  /* USER CODE END StartmotorTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
static void MotorTask_SendText(const char *text)
{
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), 50U);
}

static void MotorTask_SendBanner(float kp, float ki)
{
  char line[180];
  MotorTask_SendText("#motor_control,C10B_AT8236\r\n");
  (void)snprintf(line, sizeof(line),
                 "#config,mode=%u,wheel_mm=80,cpr=1560,pi_set=%u,kp_x100=%ld,ki_x100=%ld\r\n",
                 (unsigned int)MOTOR_EXPERIMENT_MODE,
                 (unsigned int)MOTOR_PI_PARAMETER_SET,
                 (long)MotorTask_RoundFloat(kp * 100.0f),
                 (long)MotorTask_RoundFloat(ki * 100.0f));
  MotorTask_SendText(line);
  MotorTask_SendText("#map,left_pwm=PA6_PA7,right_pwm=PB0_PB1,left_enc=TIM4_PB6_PB7,right_enc=TIM8_PC6_PC7\r\n");
  MotorTask_SendText("#notice,hardware_cpr_1560_differs_from_assignment_2000\r\n");
  MotorTask_SendText("ms,mode,target_l,target_r,raw_l,raw_r,speed_l,speed_r,pwm_l,pwm_r,count_l,count_r\r\n");
}

static void MotorTask_SendTelemetry(uint32_t elapsed_ms, int32_t target_left,
                                    int32_t target_right)
{
  const MotorTelemetry_t *telemetry = Motor_GetTelemetry();
  char line[180];
  (void)snprintf(line, sizeof(line),
                 "%lu,%u,%ld,%ld,%ld,%ld,%ld,%ld,%d,%d,%ld,%ld\r\n",
                 (unsigned long)elapsed_ms,
                 (unsigned int)MOTOR_EXPERIMENT_MODE,
                 (long)target_left,
                 (long)target_right,
                 (long)MotorTask_RoundFloat(telemetry->left_raw_mm_s),
                 (long)MotorTask_RoundFloat(telemetry->right_raw_mm_s),
                 (long)MotorTask_RoundFloat(telemetry->left_filtered_mm_s),
                 (long)MotorTask_RoundFloat(telemetry->right_filtered_mm_s),
                 telemetry->left_pwm,
                 telemetry->right_pwm,
                 (long)telemetry->left_total,
                 (long)telemetry->right_total);
  MotorTask_SendText(line);
}

static int32_t MotorTask_RoundFloat(float value)
{
  if (value >= 0.0f)
  {
    return (int32_t)(value + 0.5f);
  }
  return (int32_t)(value - 0.5f);
}

/* USER CODE END Application */

