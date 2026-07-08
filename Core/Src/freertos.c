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
#include "MPU6050.h"
#include "usart.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define APP_EXPERIMENT_MOTOR           0U
#define APP_EXPERIMENT_MPU             1U

#ifndef APP_EXPERIMENT_MODE
#define APP_EXPERIMENT_MODE            APP_EXPERIMENT_MOTOR
#endif

#define MOTOR_TASK_PERIOD_MS          10U
#define OPEN_LOOP_RUN_MS              3000U
#define OPEN_LOOP_STOP_MS             1000U
#define DEADZONE_DWELL_MS             1000U
#define DEADZONE_MAX_PERCENT          50U
#define DEADZONE_MIN_WINDOW_COUNTS    3U
#define PI_STEP_DELAY_MS              1000U
#define PI_TARGET_MM_S                200

#define MPU_TASK_PERIOD_MS            20U
#define MPU_GRAVITY_MS2               9.80665f
#define MPU_RAD_TO_DEG                57.2957795f
#define MPU_COMP_ALPHA                0.98f

#ifndef MPU_SAMPLE_STATE
#define MPU_SAMPLE_STATE              "static"
#endif

#ifndef MPU_PITCH_OUTPUT_SIGN
#define MPU_PITCH_OUTPUT_SIGN         1
#endif

#ifndef MPU_BODY_PITCH_FROM_DMP_ROLL
#define MPU_BODY_PITCH_FROM_DMP_ROLL  1U
#endif

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
/* Definitions for mpuTask */
osThreadId_t mpuTaskHandle;
const osThreadAttr_t mpuTask_attributes = {
  .name = "mpuTask",
  .stack_size = 768 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void App_SendText(const char *text);
static void MotorTask_SendText(const char *text);
static void MotorTask_SendBanner(float kp, float ki);
static void MotorTask_SendTelemetry(uint32_t elapsed_ms, int32_t target_left,
                                    int32_t target_right);
static int32_t MotorTask_RoundFloat(float value);
static void MPUTask_SendBanner(void);
static void MPUTask_SendTelemetry(uint32_t elapsed_ms, const char *state,
                                  const short *accel_raw, const short *gyro_raw,
                                  float accel_sens, float gyro_sens,
                                  float pitch_acc, float pitch_comp);
static void MPUTask_FormatFloat(char *out, size_t out_size, float value,
                                uint8_t decimals);
static int32_t MPUTask_RoundFloat(float value);

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartmotorTask(void *argument);
void StartMPUTask(void *argument);

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

#if APP_EXPERIMENT_MODE == APP_EXPERIMENT_MOTOR
  /* creation of motorTask */
  motorTaskHandle = osThreadNew(StartmotorTask, NULL, &motorTask_attributes);
#elif APP_EXPERIMENT_MODE == APP_EXPERIMENT_MPU
  App_SendText("#mpu6050,mode_selected\r\n");
  /* creation of mpuTask */
  mpuTaskHandle = osThreadNew(StartMPUTask, NULL, &mpuTask_attributes);
  if (mpuTaskHandle == NULL) {
    App_SendText("#mpu6050,error,task_create_failed\r\n");
  }
#else
#error "APP_EXPERIMENT_MODE must be APP_EXPERIMENT_MOTOR or APP_EXPERIMENT_MPU"
#endif

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

/* USER CODE BEGIN Header_StartMPUTask */
/**
* @brief Function implementing the mpuTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartMPUTask */
void StartMPUTask(void *argument)
{
  /* USER CODE BEGIN StartMPUTask */
  uint32_t next_wake;
  uint32_t elapsed_ms = 0U;
  uint32_t previous_ms = 0U;
  float gyro_sens = 131.0f;
  unsigned short accel_sens = 16384U;
  float pitch_comp = 0.0f;
  uint8_t pitch_initialized = 0U;
  int init_ok;
  int retry_count = 0;

  (void)argument;

  App_SendText("#mpu6050,task_start\r\n");
  App_SendText("#mpu6050,C10B,PB14=SCL,PB15=SDA,USART1=115200\r\n");
  App_SendText("#mpu6050,init_begin\r\n");
  i2cInit();
  i2cUnstick();
  osDelay(300);
  init_ok = MPU_init();
  while ((init_ok == 0) && (retry_count < 5))
  {
    retry_count++;
    App_SendText("#mpu6050,init_retry\r\n");
    i2cUnstick();
    osDelay(300);
    init_ok = MPU_init();
  }

  if (init_ok == 0)
  {
    App_SendText("#mpu6050,error,init_failed\r\n");
    for (;;)
    {
      osDelay(1000);
    }
  }

  (void)mpu_get_accel_sens(&accel_sens);
  (void)mpu_get_gyro_sens(&gyro_sens);
  App_SendText("#mpu6050,init_ok\r\n");
  MPUTask_SendBanner();
  next_wake = osKernelGetTickCount();

  for (;;)
  {
    short accel_raw[3] = {0};
    short gyro_raw[3] = {0};
    unsigned long timestamp = 0UL;
    float ax_g;
    float ay_g;
    float az_g;
    float gx_dps;
    float gy_dps;
    float dt_s;
    float pitch_acc;

    MPU_getdata();
    if (mpu_get_accel_reg(accel_raw, &timestamp) != 0)
    {
      accel_raw[0] = ax;
      accel_raw[1] = ay;
      accel_raw[2] = az;
    }
    if (mpu_get_gyro_reg(gyro_raw, &timestamp) != 0)
    {
      gyro_raw[0] = gx;
      gyro_raw[1] = gy;
      gyro_raw[2] = gz;
    }

    ax_g = (float)accel_raw[0] / (float)accel_sens;
    ay_g = (float)accel_raw[1] / (float)accel_sens;
    az_g = (float)accel_raw[2] / (float)accel_sens;
    gx_dps = (float)gyro_raw[0] / gyro_sens;
    gy_dps = (float)gyro_raw[1] / gyro_sens;
    dt_s = (elapsed_ms == 0U) ? ((float)MPU_TASK_PERIOD_MS / 1000.0f) :
           ((float)(elapsed_ms - previous_ms) / 1000.0f);

#if MPU_BODY_PITCH_FROM_DMP_ROLL
    pitch_acc = (float)(-atan2(ay_g, sqrt((ax_g * ax_g) + (az_g * az_g))) * MPU_RAD_TO_DEG);
#else
    pitch_acc = (float)(atan2(-ax_g, sqrt((ay_g * ay_g) + (az_g * az_g))) * MPU_RAD_TO_DEG);
#endif
    if (pitch_initialized == 0U)
    {
      pitch_comp = pitch_acc;
      pitch_initialized = 1U;
    }
    else
    {
#if MPU_BODY_PITCH_FROM_DMP_ROLL
      pitch_comp = (MPU_COMP_ALPHA * (pitch_comp + (gx_dps * dt_s))) +
                   ((1.0f - MPU_COMP_ALPHA) * pitch_acc);
#else
      pitch_comp = (MPU_COMP_ALPHA * (pitch_comp + (gy_dps * dt_s))) +
                   ((1.0f - MPU_COMP_ALPHA) * pitch_acc);
#endif
    }

    MPUTask_SendTelemetry(elapsed_ms, MPU_SAMPLE_STATE, accel_raw, gyro_raw,
                          (float)accel_sens, gyro_sens,
                          pitch_acc * (float)MPU_PITCH_OUTPUT_SIGN,
                          pitch_comp * (float)MPU_PITCH_OUTPUT_SIGN);

    previous_ms = elapsed_ms;
    elapsed_ms += MPU_TASK_PERIOD_MS;
    next_wake += MPU_TASK_PERIOD_MS;
    (void)osDelayUntil(next_wake);
  }
  /* USER CODE END StartMPUTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
static void App_SendText(const char *text)
{
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), 100U);
}

static void MotorTask_SendText(const char *text)
{
  App_SendText(text);
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

static void MPUTask_SendBanner(void)
{
  App_SendText("#mpu6050,C10B,PB14=SCL,PB15=SDA,USART1=115200\r\n");
  App_SendText("#config,period_ms=20,rate_hz=50,comp_alpha=0.98,pitch_sign=");
#if MPU_PITCH_OUTPUT_SIGN < 0
  App_SendText("-1\r\n");
#else
  App_SendText("1\r\n");
#endif
#if MPU_BODY_PITCH_FROM_DMP_ROLL
  App_SendText("#config,body_pitch_axis=dmp_roll\r\n");
#else
  App_SendText("#config,body_pitch_axis=dmp_pitch\r\n");
#endif
  App_SendText("ms,state,ax_raw,ay_raw,az_raw,gx_raw,gy_raw,gz_raw,");
  App_SendText("ax_g,ay_g,az_g,ax_ms2,ay_ms2,az_ms2,");
  App_SendText("gx_dps,gy_dps,gz_dps,pitch_acc,pitch_comp,pitch_dmp,roll_dmp,yaw_dmp\r\n");
}

static void MPUTask_SendTelemetry(uint32_t elapsed_ms, const char *state,
                                  const short *accel_raw, const short *gyro_raw,
                                  float accel_sens, float gyro_sens,
                                  float pitch_acc, float pitch_comp)
{
  char line[280];
  char ax_g_text[18];
  char ay_g_text[18];
  char az_g_text[18];
  char ax_ms2_text[18];
  char ay_ms2_text[18];
  char az_ms2_text[18];
  char gx_dps_text[18];
  char gy_dps_text[18];
  char gz_dps_text[18];
  char pitch_acc_text[18];
  char pitch_comp_text[18];
  char pitch_dmp_text[18];
  char roll_dmp_text[18];
  char yaw_dmp_text[18];
  float ax_g = (float)accel_raw[0] / accel_sens;
  float ay_g = (float)accel_raw[1] / accel_sens;
  float az_g = (float)accel_raw[2] / accel_sens;
  float gx_dps = (float)gyro_raw[0] / gyro_sens;
  float gy_dps = (float)gyro_raw[1] / gyro_sens;
  float gz_dps = (float)gyro_raw[2] / gyro_sens;

  MPUTask_FormatFloat(ax_g_text, sizeof(ax_g_text), ax_g, 5U);
  MPUTask_FormatFloat(ay_g_text, sizeof(ay_g_text), ay_g, 5U);
  MPUTask_FormatFloat(az_g_text, sizeof(az_g_text), az_g, 5U);
  MPUTask_FormatFloat(ax_ms2_text, sizeof(ax_ms2_text), ax_g * MPU_GRAVITY_MS2, 4U);
  MPUTask_FormatFloat(ay_ms2_text, sizeof(ay_ms2_text), ay_g * MPU_GRAVITY_MS2, 4U);
  MPUTask_FormatFloat(az_ms2_text, sizeof(az_ms2_text), az_g * MPU_GRAVITY_MS2, 4U);
  MPUTask_FormatFloat(gx_dps_text, sizeof(gx_dps_text), gx_dps, 4U);
  MPUTask_FormatFloat(gy_dps_text, sizeof(gy_dps_text), gy_dps, 4U);
  MPUTask_FormatFloat(gz_dps_text, sizeof(gz_dps_text), gz_dps, 4U);
  MPUTask_FormatFloat(pitch_acc_text, sizeof(pitch_acc_text), pitch_acc, 3U);
  MPUTask_FormatFloat(pitch_comp_text, sizeof(pitch_comp_text), pitch_comp, 3U);
#if MPU_BODY_PITCH_FROM_DMP_ROLL
  MPUTask_FormatFloat(pitch_dmp_text, sizeof(pitch_dmp_text),
                      fAY * (float)MPU_PITCH_OUTPUT_SIGN, 3U);
  MPUTask_FormatFloat(roll_dmp_text, sizeof(roll_dmp_text), fAX, 3U);
#else
  MPUTask_FormatFloat(pitch_dmp_text, sizeof(pitch_dmp_text),
                      fAX * (float)MPU_PITCH_OUTPUT_SIGN, 3U);
  MPUTask_FormatFloat(roll_dmp_text, sizeof(roll_dmp_text), fAY, 3U);
#endif
  MPUTask_FormatFloat(yaw_dmp_text, sizeof(yaw_dmp_text), fAZ, 3U);

  (void)snprintf(line, sizeof(line),
                 "%lu,%s,%d,%d,%d,%d,%d,%d,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\r\n",
                 (unsigned long)elapsed_ms,
                 state,
                 accel_raw[0], accel_raw[1], accel_raw[2],
                 gyro_raw[0], gyro_raw[1], gyro_raw[2],
                 ax_g_text, ay_g_text, az_g_text,
                 ax_ms2_text, ay_ms2_text, az_ms2_text,
                 gx_dps_text, gy_dps_text, gz_dps_text,
                 pitch_acc_text, pitch_comp_text, pitch_dmp_text,
                 roll_dmp_text, yaw_dmp_text);
  App_SendText(line);
}

static void MPUTask_FormatFloat(char *out, size_t out_size, float value,
                                uint8_t decimals)
{
  int32_t scale = 1;
  int32_t scaled;
  int32_t whole;
  int32_t fraction;
  uint8_t i;

  for (i = 0U; i < decimals; i++)
  {
    scale *= 10;
  }

  scaled = MPUTask_RoundFloat(value * (float)scale);
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

static int32_t MPUTask_RoundFloat(float value)
{
  if (value >= 0.0f)
  {
    return (int32_t)(value + 0.5f);
  }
  return (int32_t)(value - 0.5f);
}

/* USER CODE END Application */

