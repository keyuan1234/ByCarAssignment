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
#include <stdlib.h>
#include <string.h>
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
#define ENCODER_PPR              2000        /* 编码器脉冲/转（4倍频后理论值） */
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

/* 开环/闭环控制 */
volatile uint8_t control_mode = 0;       /* 0=开环, 1=闭环 */
volatile float openloop_pwm_A = 0;       /* 开环电机A PWM [-100, 100] */
volatile float openloop_pwm_B = 0;       /* 开环电机B PWM [-100, 100] */

/* 编码器累计脉冲（用于CALIB校验） */
volatile int32_t encoder_accum_A = 0;
volatile int32_t encoder_accum_B = 0;

/* 数据记录 */
volatile uint8_t logging_enabled = 0;    /* 0=停止记录, 1=记录中 */
volatile uint32_t log_start_tick = 0;    /* 记录开始时的tick */
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
float PID_Incremental(PID_HandleTypeDef *pid, float error);
void PID_Reset(PID_HandleTypeDef *pid);
void UART_CommandParse(void);
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
  printf("System initialized. Mode:OPEN\r\n");
  for(;;)
  {
    /* 检查串口命令 */
    UART_CommandParse();

    /* 每10次（1秒）打印一次状态 */
    if (++print_counter >= 10) {
      print_counter = 0;
      if (control_mode == 0) {
        /* 开环模式输出：包含编码器实时脉冲数 */
        printf("M:OPEN | PWMA:%.0f%% SPD_A:%.1f ENC_A:%ld | PWMB:%.0f%% SPD_B:%.1f ENC_B:%ld mm/s\r\n",
               openloop_pwm_A, current_speed_A, (long)encoder_count_A,
               openloop_pwm_B, current_speed_B, (long)encoder_count_B);
      } else {
        /* 闭环模式输出：包含编码器实时脉冲数 */
        printf("M:CLOSE | TgtA:%.1f SPD_A:%.1f ENC_A:%ld | TgtB:%.1f SPD_B:%.1f ENC_B:%ld mm/s\r\n",
               speed_target_A, current_speed_A, (long)encoder_count_A,
               speed_target_B, current_speed_B, (long)encoder_count_B);
      }
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
  float pwm_a, pwm_b;
  uint32_t cmp1, cmp2, cmp3, cmp4;
  for(;;)
  {
    /* 根据模式选择PWM来源 */
    if (control_mode == 0) {    /* 开环模式：直接使用串口设置的PWM值 */
      pwm_a = openloop_pwm_A;
      pwm_b = openloop_pwm_B;
    } else {                    /* 闭环模式：使用PID输出 */
      pwm_a = motor_pwm_A;
      pwm_b = motor_pwm_B;
    }
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
  int16_t last_raw_A = 0, last_raw_B = 0;
  int16_t raw_A, raw_B;
  int32_t delta_A, delta_B;
  float speed_A, speed_B;
  float error_A, error_B;
  float output_A, output_B;
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

    /* 累计编码器脉冲（用于CALIB校验） */
    encoder_accum_A += delta_A;
    encoder_accum_B += delta_B;

    /* 速度换算：脉冲增量 → 脉冲/秒 → mm/s */
    speed_A = (delta_A / SPEED_DT) / ENCODER_PPR * WHEEL_CIRCUMFERENCE_MM;
    speed_B = (delta_B / SPEED_DT) / ENCODER_PPR * WHEEL_CIRCUMFERENCE_MM;
    current_speed_A = speed_A;
    current_speed_B = speed_B;

    if (control_mode == 1) {  /* 闭环模式：增量式PI */
      error_A = speed_target_A - speed_A;
      error_B = speed_target_B - speed_B;
      output_A = PID_Incremental(&pid_A, error_A);
      output_B = PID_Incremental(&pid_B, error_B);
      motor_pwm_A = output_A;
      motor_pwm_B = output_B;
    }

    /* 数据记录：CSV格式，每50ms（5个周期）输出一行 */
    if (logging_enabled) {
      static uint8_t log_counter = 0;
      if (++log_counter >= 5) {
        log_counter = 0;
        uint32_t t = osKernelSysTick() - log_start_tick;
        printf("LOG,%lu,%.1f,%.1f,%.1f,%ld,%.1f,%.1f,%.1f,%ld\r\n",
               t, speed_target_A, speed_A,
               (control_mode==1 ? motor_pwm_A : openloop_pwm_A),
               (long)encoder_count_A,
               speed_target_B, speed_B,
               (control_mode==1 ? motor_pwm_B : openloop_pwm_B),
               (long)encoder_count_B);
      }
    }
    osDelay(10);
  }
  /* USER CODE END StartTaskControlSpeed */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* 串口命令解析（在defaultTask中每100ms调用一次） */
void UART_CommandParse(void)
{
  static char cmd_buf[64];
  static uint8_t idx = 0;
  char ch;

  while (uart_rx_available() > 0) {
    ch = (char)uart_rx_get_char();

    if (ch == '\r' || ch == '\n') {
      if (idx > 0) {
        cmd_buf[idx] = '\0';
        idx = 0;

        /* --- 命令匹配 --- */

        /* MODE 0 / MODE 1 */
        if (strncmp(cmd_buf, "MODE ", 5) == 0) {
          int m = atoi(cmd_buf + 5);
          if (m == 0) {
            control_mode = 0;
            printf(">> Open-loop mode\r\n");
          } else if (m == 1) {
            control_mode = 1;
            PID_Reset(&pid_A);
            PID_Reset(&pid_B);
            motor_pwm_A = 0;
            motor_pwm_B = 0;
            printf(">> Closed-loop mode\r\n");
          }
        }
        /* PWM A <value> / PWM B <value> */
        else if (strncmp(cmd_buf, "PWM A ", 6) == 0) {
          openloop_pwm_A = (float)atof(cmd_buf + 6);
          printf(">> PWM_A = %.1f%%\r\n", openloop_pwm_A);
        }
        else if (strncmp(cmd_buf, "PWM B ", 6) == 0) {
          openloop_pwm_B = (float)atof(cmd_buf + 6);
          printf(">> PWM_B = %.1f%%\r\n", openloop_pwm_B);
        }
        /* CALIB — 打印编码器累计脉冲 */
        else if (strcmp(cmd_buf, "CALIB") == 0) {
          printf(">> Encoder A: %ld, Encoder B: %ld pulses\r\n",
                 (long)encoder_accum_A, (long)encoder_accum_B);
        }
        /* KP / KI */
        else if (strncmp(cmd_buf, "KP ", 3) == 0) {
          float v = (float)atof(cmd_buf + 3);
          pid_A.Kp = pid_B.Kp = v;
          printf(">> Kp = %.3f\r\n", v);
        }
        else if (strncmp(cmd_buf, "KI ", 3) == 0) {
          float v = (float)atof(cmd_buf + 3);
          pid_A.Ki = pid_B.Ki = v;
          printf(">> Ki = %.3f\r\n", v);
        }
        /* TARGET <value> — 同时设置A/B目标速度 mm/s */
        else if (strncmp(cmd_buf, "TARGET ", 7) == 0) {
          float v = (float)atof(cmd_buf + 7);
          speed_target_A = v;
          speed_target_B = v;
          printf(">> Target = %.1f mm/s\r\n", v);
        }
        /* START_LOG — 开始数据记录 */
        else if (strcmp(cmd_buf, "START_LOG") == 0) {
          PID_Reset(&pid_A);
          PID_Reset(&pid_B);
          motor_pwm_A = 0;
          motor_pwm_B = 0;
          log_start_tick = osKernelSysTick();
          logging_enabled = 1;
          printf(">> Log started. CSV header: tick,tgtA,spdA,pwmA,encA,tgtB,spdB,pwmB,encB\r\n");
          printf("LOG_HEADER,tick,tgtA,spdA,pwmA,encA,tgtB,spdB,pwmB,encB\r\n");
        }
        /* STOP_LOG — 停止数据记录 */
        else if (strcmp(cmd_buf, "STOP_LOG") == 0) {
          logging_enabled = 0;
          printf(">> Log stopped\r\n");
        }
        /* STEP <value> — 阶跃响应测试（自动从0跳变） */
        else if (strncmp(cmd_buf, "STEP ", 5) == 0) {
          float v = (float)atof(cmd_buf + 5);
          /* 先开始记录，再设置目标速度 */
          PID_Reset(&pid_A);
          PID_Reset(&pid_B);
          motor_pwm_A = 0;
          motor_pwm_B = 0;
          log_start_tick = osKernelSysTick();
          logging_enabled = 1;
          speed_target_A = v;
          speed_target_B = v;
          printf(">> Step test: target = %.1f mm/s, logging started\r\n", v);
          printf("LOG_HEADER,tick,tgtA,spdA,pwmA,encA,tgtB,spdB,pwmB,encB\r\n");
        }
        /* 未知命令 */
        else {
          printf(">> Unknown: %s\r\n", cmd_buf);
        }
      }
    }
    else if (idx < (int)sizeof(cmd_buf) - 1) {
      cmd_buf[idx++] = ch;
    }
    /* 缓冲区满了就丢弃 */
  }
}

float PID_Calculate(PID_HandleTypeDef *pid, float error, float dt)
{
  float output;
  /* 位置式PID（保留作参考，闭环使用增量式PI） */
  float integral_temp = pid->output;  /* output字段替代原integral */
  integral_temp += error * dt;
  if (integral_temp > pid->output_max) {
    integral_temp = pid->output_max;
  } else if (integral_temp < pid->output_min) {
    integral_temp = pid->output_min;
  }
  output = pid->Kp * error + pid->Ki * integral_temp + pid->Kd * (error - pid->last_error) / dt;
  pid->last_error = error;
  pid->output = integral_temp;
  if (output > pid->output_max) {
    output = pid->output_max;
  } else if (output < pid->output_min) {
    output = pid->output_min;
  }
  return output;
}

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
