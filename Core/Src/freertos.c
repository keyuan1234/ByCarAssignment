/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  */
/* USER CODE END Header */

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* USER CODE BEGIN Includes */
#include "balance_control.h"
#include "bluetooth_control.h"
#include "imu.h"
#include "line_tracking.h"
#include "motor_control.h"
#include "oled_dashboard.h"
#include "ultrasonic.h"
#include "usart.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PTD */
typedef struct
{
  uint32_t start_ms;
  uint16_t reverse_speed_mm_s;
  uint8_t active;
} AppUltrasonicAvoidance_t;

typedef struct
{
  uint32_t now_ms;
  uint32_t data_ready_missed;
  uint16_t loop_dt_ms;
  BalanceSample_t sample;
  MotorTelemetry_t motor;
  BalanceOutput_t output;
  BluetoothCommand_t bluetooth;
  UltrasonicSample_t ultrasonic;
  uint8_t grayscale_code;
  uint8_t avoidance_active;
} AppTelemetryFrame_t;
/* USER CODE END PTD */

/* USER CODE BEGIN PD */
#define BALANCE_TELEMETRY_PERIOD_MS    100U
#define OLED_DISPLAY_PERIOD_MS         200U
#define IMU_DATA_READY_THREAD_FLAG     0x00000001U
#define CONTROL_IMU_SAMPLES_PER_STEP   2U
#define BLUETOOTH_RX_QUEUE_DEPTH       32U
#define BLUETOOTH_COMMAND_QUEUE_DEPTH  4U
#define BLUETOOTH_NORMAL_BAUD          9600U
#define BLUETOOTH_AT_STARTUP_DELAY_MS  250U
#define BLUETOOTH_AT_TIMEOUT_MS        300U
#define BLUETOOTH_AT_COMMAND_DELAY_MS  50U
#define BLUETOOTH_AT_RESET_DELAY_MS    200U
#define APP_PI_F                       3.14159265358979323846f
/* USER CODE END PD */

/* USER CODE BEGIN Variables */
static volatile uint32_t app_imu_data_ready_sequence;
static uint8_t app_bluetooth_rx_byte;
static volatile uint8_t app_bluetooth_rx_enabled;
/* USER CODE END Variables */

/* Definitions for ControlTask */
osThreadId_t ControlTaskHandle;
const osThreadAttr_t ControlTask_attributes = {
  .name = "ControlTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};

/* Definitions for TelemetryTask */
osThreadId_t TelemetryTaskHandle;
const osThreadAttr_t TelemetryTask_attributes = {
  .name = "TelemetryTask",
  .stack_size = 384 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

/* Definitions for BluetoothTask */
osThreadId_t BluetoothTaskHandle;
const osThreadAttr_t BluetoothTask_attributes = {
  .name = "BluetoothTask",
  .stack_size = 384 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Definitions for UltrasonicTask */
osThreadId_t UltrasonicTaskHandle;
const osThreadAttr_t UltrasonicTask_attributes = {
  .name = "UltrasonicTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};

/* Definitions for TelemetryQueue */
osMessageQueueId_t TelemetryQueueHandle;
const osMessageQueueAttr_t TelemetryQueue_attributes = {
  .name = "TelemetryQueue"
};

/* Definitions for BluetoothRxQueue */
osMessageQueueId_t BluetoothRxQueueHandle;
const osMessageQueueAttr_t BluetoothRxQueue_attributes = {
  .name = "BluetoothRxQueue"
};

/* Definitions for BluetoothCommandQueue */
osMessageQueueId_t BluetoothCommandQueueHandle;
const osMessageQueueAttr_t BluetoothCommandQueue_attributes = {
  .name = "BluetoothCommandQueue"
};

/* Latest completed ultrasonic sample; depth one intentionally replaces stale
   measurements instead of delaying the balance-control consumer. */
osMessageQueueId_t UltrasonicQueueHandle;
const osMessageQueueAttr_t UltrasonicQueue_attributes = {
  .name = "UltrasonicQueue"
};

/* Serialize USART1 debug output from the Bluetooth and telemetry tasks. */
osMutexId_t Uart1TxMutexHandle;
const osMutexAttr_t Uart1TxMutex_attributes = {
  .name = "Uart1TxMutex"
};

/* USER CODE BEGIN FunctionPrototypes */
static void App_SendText(const char *text);
static void App_SendBluetoothCommand(uint8_t byte,
                                     const BluetoothCommand_t *command);
static void App_SetStoppedOutput(BalanceOutput_t *output,
                                 BalanceState_t state);
static void App_BuildBalanceSetpoint(const BluetoothCommand_t *command,
                                     BalanceSetpoint_t *setpoint);
static void App_ApplyLineTracking(const BluetoothCommand_t *command,
                                  uint8_t grayscale_code,
                                  BluetoothCommand_t *tracking_command);
static void App_ApplyUltrasonicAvoidance(
  const BluetoothCommand_t *command,
  const UltrasonicSample_t *sample,
  uint8_t new_sample,
  uint32_t now_ms,
  AppUltrasonicAvoidance_t *avoidance,
  BluetoothCommand_t *effective_command);
static void App_UltrasonicPublishSample(const UltrasonicSample_t *sample);
static void App_QueueTelemetry(uint32_t now_ms,
                               uint16_t loop_dt_ms,
                               uint32_t data_ready_missed,
                                const BalanceSample_t *sample,
                                const MotorTelemetry_t *telemetry,
                                const BalanceOutput_t *output,
                                const BluetoothCommand_t *bluetooth,
                                const UltrasonicSample_t *ultrasonic,
                                uint8_t grayscale_code,
                                uint8_t avoidance_active);
static void App_SendTelemetry(const AppTelemetryFrame_t *frame);
static void App_FormatFloat(char *out, size_t out_size, float value,
                            uint8_t decimals);
static int32_t App_RoundFloat(float value);
static const char *App_StateText(BalanceState_t state);
static HAL_StatusTypeDef App_BluetoothSetBaud(uint32_t baud_rate);
static uint8_t App_BluetoothAtExchange(const char *request,
                                       const char *expected,
                                       uint32_t timeout_ms,
                                       BluetoothAtResponse_t *response);
static void App_BluetoothConfigureJdy33(void);
static void App_BluetoothPublishCommand(const BluetoothCommand_t *command);
static uint8_t App_ReadGrayscaleCode(void);
/* USER CODE END FunctionPrototypes */

void StartControlTask(void *argument);
void StartTelemetryTask(void *argument);
void StartBluetoothTask(void *argument);
void StartUltrasonicTask(void *argument);

void MX_FREERTOS_Init(void);

void MX_FREERTOS_Init(void)
{
  /* USER CODE BEGIN Init */
  app_imu_data_ready_sequence = 0U;
  app_bluetooth_rx_enabled = 0U;
  /* USER CODE END Init */

  Uart1TxMutexHandle = osMutexNew(&Uart1TxMutex_attributes);
  if (Uart1TxMutexHandle == NULL)
  {
    App_SendText("#key,error=uart1_mutex_create_failed\r\n");
  }

  /* creation of TelemetryQueue */
  TelemetryQueueHandle = osMessageQueueNew(4, sizeof(AppTelemetryFrame_t),
                                            &TelemetryQueue_attributes);
  if (TelemetryQueueHandle == NULL)
  {
    App_SendText("#key,error=telemetry_queue_create_failed\r\n");
  }

  BluetoothRxQueueHandle = osMessageQueueNew(
    BLUETOOTH_RX_QUEUE_DEPTH, sizeof(uint8_t), &BluetoothRxQueue_attributes);
  BluetoothCommandQueueHandle = osMessageQueueNew(
    BLUETOOTH_COMMAND_QUEUE_DEPTH, sizeof(BluetoothCommand_t),
    &BluetoothCommandQueue_attributes);
  UltrasonicQueueHandle = osMessageQueueNew(
    1U, sizeof(UltrasonicSample_t), &UltrasonicQueue_attributes);
  if ((BluetoothRxQueueHandle == NULL) ||
      (BluetoothCommandQueueHandle == NULL) ||
      (UltrasonicQueueHandle == NULL))
  {
    App_SendText("#key,error=queue_create_failed\r\n");
  }

  /* creation of ControlTask */
  ControlTaskHandle = osThreadNew(StartControlTask, NULL,
                                  &ControlTask_attributes);
  if (ControlTaskHandle == NULL)
  {
    App_SendText("#key,error=control_task_create_failed\r\n");
  }

  /* creation of TelemetryTask */
  TelemetryTaskHandle = NULL;
  if (TelemetryQueueHandle != NULL)
  {
    TelemetryTaskHandle = osThreadNew(StartTelemetryTask, NULL,
                                      &TelemetryTask_attributes);
  }
  if (TelemetryTaskHandle == NULL)
  {
    App_SendText("#key,error=telemetry_task_create_failed\r\n");
  }


  BluetoothTaskHandle = NULL;
  if ((BluetoothRxQueueHandle != NULL) &&
      (BluetoothCommandQueueHandle != NULL))
  {
    BluetoothTaskHandle = osThreadNew(StartBluetoothTask, NULL,
                                      &BluetoothTask_attributes);
  }
  if (BluetoothTaskHandle == NULL)
  {
    App_SendText("#bt,error=task_create_failed\r\n");
  }

  UltrasonicTaskHandle = NULL;
  if (UltrasonicQueueHandle != NULL)
  {
    UltrasonicTaskHandle = osThreadNew(StartUltrasonicTask, NULL,
                                       &UltrasonicTask_attributes);
  }
  if (UltrasonicTaskHandle == NULL)
  {
    App_SendText("#key,error=ultrasonic_task_create_failed\r\n");
  }

  /* USER CODE BEGIN RTOS_THREADS */
  /* USER CODE END RTOS_THREADS */
}

/* USER CODE BEGIN Header_StartControlTask */
/**
  * @brief PB9 MPU6050 data-ready synchronized balance control task.
  * @param argument Not used.
  */
/* USER CODE END Header_StartControlTask */
void StartControlTask(void *argument)
{
  /* USER CODE BEGIN StartControlTask */
  BalanceController_t controller;
  BalanceSample_t sample;
  BalanceOutput_t output;
  BalanceSetpoint_t setpoint;
  BluetoothCommand_t bluetooth_command;
  BluetoothCommand_t tracking_command;
  BluetoothCommand_t effective_command;
  BluetoothCommand_t pending_bluetooth_command;
  UltrasonicSample_t ultrasonic_sample;
  UltrasonicSample_t pending_ultrasonic_sample;
  AppUltrasonicAvoidance_t avoidance;
  const MotorTelemetry_t *telemetry;
  uint32_t current_sequence;
  uint32_t last_sequence;
  uint32_t last_sample_ms;
  uint32_t last_control_ms = 0U;
  uint32_t last_telemetry_ms = 0U;
  uint32_t data_ready_missed = 0U;
  uint16_t loop_dt_ms = 0U;
  uint8_t grayscale_code;
  uint8_t control_divider = 0U;

  (void)argument;
  (void)memset(&sample, 0, sizeof(sample));
  (void)memset(&setpoint, 0, sizeof(setpoint));
  (void)memset(&ultrasonic_sample, 0, sizeof(ultrasonic_sample));
  (void)memset(&avoidance, 0, sizeof(avoidance));
  BluetoothCommand_Init(&bluetooth_command);
  tracking_command = bluetooth_command;
  effective_command = bluetooth_command;
  grayscale_code = App_ReadGrayscaleCode();
  App_SetStoppedOutput(&output, BALANCE_STATE_TIMEOUT);
  Motor_Init();
  BalanceControl_Init(&controller);

  if (Imu_Init() == 0U)
  {
    App_SetStoppedOutput(&output, BALANCE_STATE_IMU_ERROR);
    Motor_Brake();
    App_SendText("#key,imu=error\r\n");
    for (;;)
    {
      telemetry = Motor_GetTelemetry();
      sample.timestamp_ms = HAL_GetTick();
      sample.valid = 0U;
      grayscale_code = App_ReadGrayscaleCode();
      App_QueueTelemetry(sample.timestamp_ms, 0U, data_ready_missed,
                          &sample, telemetry, &output, &bluetooth_command,
                          &ultrasonic_sample, grayscale_code,
                          avoidance.active);
      osDelay(BALANCE_TELEMETRY_PERIOD_MS);
    }
  }

  App_SendText("#key,imu=ok\r\n");
  last_sequence = app_imu_data_ready_sequence;
  (void)osThreadFlagsClear(IMU_DATA_READY_THREAD_FLAG);
  last_sample_ms = HAL_GetTick();

  for (;;)
  {
    uint32_t flags;
    uint32_t now_ms;
    uint32_t sequence_delta;
    uint32_t sample_dt_ms;
    uint8_t timing_fault = 0U;

    flags = osThreadFlagsWait(IMU_DATA_READY_THREAD_FLAG, osFlagsWaitAny,
                              BALANCE_SAMPLE_TIMEOUT_MS);
    now_ms = HAL_GetTick();

    if ((flags & osFlagsError) != 0U)
    {
      BalanceControl_Reset(&controller);
      Motor_Brake();
      App_SetStoppedOutput(&output, BALANCE_STATE_TIMEOUT);
      control_divider = 0U;
      last_control_ms = 0U;
      loop_dt_ms = 0U;
      telemetry = Motor_GetTelemetry();
      if ((now_ms - last_telemetry_ms) >= BALANCE_TELEMETRY_PERIOD_MS)
      {
        App_QueueTelemetry(now_ms, loop_dt_ms, data_ready_missed,
                           &sample, telemetry, &output, &bluetooth_command,
                           &ultrasonic_sample, grayscale_code,
                           avoidance.active);
        last_telemetry_ms = now_ms;
      }
      continue;
    }

    current_sequence = app_imu_data_ready_sequence;
    sequence_delta = current_sequence - last_sequence;
    last_sequence = current_sequence;
    if (sequence_delta != 1U)
    {
      data_ready_missed += (sequence_delta > 1U) ?
                           (sequence_delta - 1U) : 1U;
      timing_fault = 1U;
    }

    sample_dt_ms = now_ms - last_sample_ms;
    last_sample_ms = now_ms;
    (void)Imu_UpdateKalman5ms(&sample);
    sample.data_ready_sequence = current_sequence;
    sample.sample_dt_ms = (sample_dt_ms > 65535U) ? 65535U :
                          (uint16_t)sample_dt_ms;
    Encoder_Update5ms();
    telemetry = Motor_GetTelemetry();

    if (sample.valid == 0U)
    {
      BalanceControl_Reset(&controller);
      Motor_Brake();
      App_SetStoppedOutput(&output, BALANCE_STATE_IMU_ERROR);
      control_divider = 0U;
      last_control_ms = 0U;
      loop_dt_ms = 0U;
    }
    else if (timing_fault != 0U)
    {
      BalanceControl_Reset(&controller);
      Motor_Brake();
      App_SetStoppedOutput(&output, BALANCE_STATE_OVERRUN);
      control_divider = 0U;
      last_control_ms = 0U;
      loop_dt_ms = 0U;
    }
    else
    {
      control_divider++;
      if (control_divider >= CONTROL_IMU_SAMPLES_PER_STEP)
      {
        uint32_t control_dt_ms;
        uint8_t new_ultrasonic_sample = 0U;

        control_divider = 0U;
        if (last_control_ms != 0U)
        {
          control_dt_ms = now_ms - last_control_ms;
          loop_dt_ms = (control_dt_ms > 65535U) ? 65535U :
                       (uint16_t)control_dt_ms;
        }
        last_control_ms = now_ms;

        if (BluetoothCommandQueueHandle != NULL)
        {
          while (osMessageQueueGet(BluetoothCommandQueueHandle,
                                   &pending_bluetooth_command, NULL,
                                   0U) == osOK)
          {
            bluetooth_command = pending_bluetooth_command;
          }
        }
        BluetoothCommand_ApplyTimeout(&bluetooth_command, now_ms);
        grayscale_code = App_ReadGrayscaleCode();
        App_ApplyLineTracking(&bluetooth_command, grayscale_code,
                              &tracking_command);
        if (UltrasonicQueueHandle != NULL)
        {
          while (osMessageQueueGet(UltrasonicQueueHandle,
                                   &pending_ultrasonic_sample, NULL,
                                   0U) == osOK)
          {
            ultrasonic_sample = pending_ultrasonic_sample;
            new_ultrasonic_sample = 1U;
          }
        }
        App_ApplyUltrasonicAvoidance(&tracking_command, &ultrasonic_sample,
                                     new_ultrasonic_sample, now_ms,
                                     &avoidance, &effective_command);
        App_BuildBalanceSetpoint(&effective_command, &setpoint);
        output = BalanceControl_Update(&controller, &sample,
                                       telemetry->left_delta,
                                       telemetry->right_delta,
                                       &setpoint);
        if (output.state == BALANCE_STATE_RUNNING)
        {
          Motor_SetPWM(output.left_pwm, output.right_pwm);
        }
        else
        {
          Motor_Brake();
        }
        telemetry = Motor_GetTelemetry();
      }
    }

    if ((now_ms - last_telemetry_ms) >= BALANCE_TELEMETRY_PERIOD_MS)
    {
      App_QueueTelemetry(now_ms, loop_dt_ms, data_ready_missed,
                         &sample, telemetry, &output, &bluetooth_command,
                         &ultrasonic_sample, grayscale_code,
                         avoidance.active);
      last_telemetry_ms = now_ms;
    }
  }
  /* USER CODE END StartControlTask */
}

/* USER CODE BEGIN Header_StartTelemetryTask */
/**
  * @brief Low-priority asynchronous UART telemetry task.
  * @param argument Not used.
  */
/* USER CODE END Header_StartTelemetryTask */
void StartTelemetryTask(void *argument)
{
  /* USER CODE BEGIN StartTelemetryTask */
  AppTelemetryFrame_t frame;
  OledDashboardData_t dashboard;
  uint32_t last_display_ms = 0U;

  (void)argument;
  (void)memset(&frame, 0, sizeof(frame));
  (void)memset(&dashboard, 0, sizeof(dashboard));
  App_SendText("#key,ms,bt,age,drive,turn,speed,target,pitch,gyro,enc_l,enc_r,b_pwm,v_pwm,pwm_l,pwm_r,state,loop,missed,ultra_mm,ultra_valid,avoid\r\n");
  for (;;)
  {
    if (osMessageQueueGet(TelemetryQueueHandle, &frame, NULL,
                          osWaitForever) == osOK)
    {
      App_SendTelemetry(&frame);
      if ((last_display_ms == 0U) ||
          ((uint32_t)(frame.now_ms - last_display_ms) >=
           OLED_DISPLAY_PERIOD_MS))
      {
        dashboard.bluetooth_name = BLUETOOTH_DEVICE_NAME;
        dashboard.pitch_deg = frame.sample.pitch_deg;
        dashboard.gyro_pitch_dps = frame.sample.gyro_pitch_dps;
        dashboard.left_speed_mm_s = frame.motor.left_filtered_mm_s;
        dashboard.right_speed_mm_s = frame.motor.right_filtered_mm_s;
        dashboard.left_pwm = frame.motor.left_pwm;
        dashboard.right_pwm = frame.motor.right_pwm;
        dashboard.distance_mm = frame.ultrasonic.distance_mm;
        dashboard.state = frame.output.state;
        dashboard.grayscale_code = frame.grayscale_code;
        dashboard.line_mode_active = (uint8_t)(
          (frame.bluetooth.control_mode == BLUETOOTH_MODE_LINE_TRACKING) ?
          1U : 0U);
        dashboard.bluetooth_active = frame.bluetooth.active;
        dashboard.distance_valid = frame.ultrasonic.valid;
        dashboard.avoidance_active = frame.avoidance_active;
        OLED_RenderDashboard(&dashboard);
        last_display_ms = frame.now_ms;
      }
    }
  }
  /* USER CODE END StartTelemetryTask */
}

void StartBluetoothTask(void *argument)
{
  BluetoothCommand_t command;
  uint8_t byte;

  (void)argument;
  BluetoothCommand_Init(&command);
  App_BluetoothConfigureJdy33();

  for (;;)
  {
    if (osMessageQueueGet(BluetoothRxQueueHandle, &byte, NULL,
                          osWaitForever) == osOK)
    {
      if (BluetoothCommand_ProcessByte(&command, byte, HAL_GetTick()) != 0U)
      {
        App_BluetoothPublishCommand(&command);
        App_SendBluetoothCommand(byte, &command);
      }
    }
  }
}

/* USER CODE BEGIN Header_StartUltrasonicTask */
/**
  * @brief Periodic ultrasonic trigger task; echo completion is NVIC-driven.
  * @param argument Not used.
  */
/* USER CODE END Header_StartUltrasonicTask */
void StartUltrasonicTask(void *argument)
{
  /* USER CODE BEGIN StartUltrasonicTask */
  UltrasonicSample_t sample;
  uint32_t next_measurement_tick;

  (void)argument;
  (void)memset(&sample, 0, sizeof(sample));
  Ultrasonic_Init(osThreadGetId());
  next_measurement_tick = osKernelGetTickCount();

  for (;;)
  {
    uint32_t flags;

    (void)osThreadFlagsClear(ULTRASONIC_RESULT_THREAD_FLAG);
    if (Ultrasonic_StartMeasurement() != 0U)
    {
      flags = osThreadFlagsWait(ULTRASONIC_RESULT_THREAD_FLAG,
                                osFlagsWaitAny,
                                ULTRASONIC_ECHO_TIMEOUT_MS + 5U);
      if ((flags & osFlagsError) != 0U)
      {
        /* NVIC update interrupt is the normal timeout path.  This task-side
           abort is only a fail-safe if an interrupt is unexpectedly lost. */
        Ultrasonic_AbortMeasurement();
      }
      if (Ultrasonic_GetResult(&sample) != 0U)
      {
        App_UltrasonicPublishSample(&sample);
      }
    }
    else
    {
      (void)memset(&sample, 0, sizeof(sample));
      sample.timestamp_ms = HAL_GetTick();
      App_UltrasonicPublishSample(&sample);
    }

    next_measurement_tick += ULTRASONIC_MEASUREMENT_PERIOD_MS;
    if (osDelayUntil(next_measurement_tick) != osOK)
    {
      next_measurement_tick = osKernelGetTickCount();
      osDelay(ULTRASONIC_MEASUREMENT_PERIOD_MS);
    }
  }
  /* USER CODE END StartUltrasonicTask */
}

/* USER CODE BEGIN Application */
static uint8_t App_ReadGrayscaleCode(void)
{
  uint8_t u1_high;
  uint8_t u2_high;
  uint8_t u3_high;
  uint8_t u4_high;

  u1_high = (HAL_GPIO_ReadPin(GRAYSCALE_U1_GPIO_Port,
                             GRAYSCALE_U1_Pin) == GPIO_PIN_SET) ? 1U : 0U;
  u2_high = (HAL_GPIO_ReadPin(GRAYSCALE_U2_GPIO_Port,
                             GRAYSCALE_U2_Pin) == GPIO_PIN_SET) ? 1U : 0U;
  u3_high = (HAL_GPIO_ReadPin(GRAYSCALE_U3_GPIO_Port,
                             GRAYSCALE_U3_Pin) == GPIO_PIN_SET) ? 1U : 0U;
  u4_high = (HAL_GPIO_ReadPin(GRAYSCALE_U4_GPIO_Port,
                             GRAYSCALE_U4_Pin) == GPIO_PIN_SET) ? 1U : 0U;

  return LineTracking_PackCode(u1_high, u2_high, u3_high, u4_high);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == INT_Pin)
  {
    app_imu_data_ready_sequence++;
    if (ControlTaskHandle != NULL)
    {
      (void)osThreadFlagsSet(ControlTaskHandle, IMU_DATA_READY_THREAD_FLAG);
    }
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if ((huart != NULL) && (huart->Instance == USART3))
  {
    if (BluetoothRxQueueHandle != NULL)
    {
      (void)osMessageQueuePut(BluetoothRxQueueHandle,
                              &app_bluetooth_rx_byte, 0U, 0U);
    }
    if (app_bluetooth_rx_enabled != 0U)
    {
      (void)HAL_UART_Receive_IT(&huart3, &app_bluetooth_rx_byte, 1U);
    }
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if ((huart != NULL) && (huart->Instance == USART3) &&
      (app_bluetooth_rx_enabled != 0U))
  {
    (void)HAL_UART_Receive_IT(&huart3, &app_bluetooth_rx_byte, 1U);
  }
}

static void App_SendText(const char *text)
{
  uint8_t mutex_acquired = 0U;

  if (text == NULL)
  {
    return;
  }
  if ((Uart1TxMutexHandle != NULL) &&
      (osKernelGetState() == osKernelRunning))
  {
    if (osMutexAcquire(Uart1TxMutexHandle, 100U) != osOK)
    {
      return;
    }
    mutex_acquired = 1U;
  }
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)text,
                           (uint16_t)strlen(text), 100U);
  if (mutex_acquired != 0U)
  {
    (void)osMutexRelease(Uart1TxMutexHandle);
  }
}

static void App_SendBluetoothCommand(uint8_t byte,
                                     const BluetoothCommand_t *command)
{
  char line[168];
  const char *drive;
  const char *turn;
  const char *mode;

  if (command == NULL)
  {
    return;
  }
  drive = (command->drive_direction == BLUETOOTH_DRIVE_FORWARD) ? "F" :
          ((command->drive_direction == BLUETOOTH_DRIVE_BACKWARD) ? "B" : "S");
  turn = (command->turn_direction == BLUETOOTH_TURN_LEFT) ? "L" :
         ((command->turn_direction == BLUETOOTH_TURN_RIGHT) ? "R" : "N");
  mode = (command->control_mode == BLUETOOTH_MODE_LINE_TRACKING) ?
         "LINE" : "MAN";
  (void)snprintf(line, sizeof(line),
                 "#bt,rx=0x%02X,mode=%s,drive=%s,turn=%s,speed=%u,line_speed=%u,gear=%u,timeout=%lums\r\n",
                 (unsigned int)byte, mode, drive, turn,
                 (unsigned int)command->speed_mm_s,
                 (unsigned int)command->line_speed_mm_s,
                 (unsigned int)command->turn_divisor,
                 (unsigned long)command->timeout_ms);
  App_SendText(line);
}

static void App_SetStoppedOutput(BalanceOutput_t *output,
                                 BalanceState_t state)
{
  if (output == NULL)
  {
    return;
  }
  (void)memset(output, 0, sizeof(*output));
  output->state = state;
}

static void App_ApplyLineTracking(const BluetoothCommand_t *command,
                                  uint8_t grayscale_code,
                                  BluetoothCommand_t *tracking_command)
{
  LineTrackingMotion_t motion;

  if ((command == NULL) || (tracking_command == NULL))
  {
    return;
  }

  *tracking_command = *command;
  if (command->control_mode != BLUETOOTH_MODE_LINE_TRACKING)
  {
    return;
  }

  LineTracking_BuildMotion(grayscale_code, &motion);
  tracking_command->drive_direction = motion.drive_direction;
  tracking_command->turn_direction = motion.turn_direction;
  tracking_command->turn_divisor = motion.turn_divisor;
  tracking_command->speed_mm_s = command->line_speed_mm_s;
}

static void App_ApplyUltrasonicAvoidance(
  const BluetoothCommand_t *command,
  const UltrasonicSample_t *sample,
  uint8_t new_sample,
  uint32_t now_ms,
  AppUltrasonicAvoidance_t *avoidance,
  BluetoothCommand_t *effective_command)
{
  if ((command == NULL) || (sample == NULL) || (avoidance == NULL) ||
      (effective_command == NULL))
  {
    return;
  }

  *effective_command = *command;

  /* A deliberate stop remains the highest-level motion command. */
  if ((command->drive_direction == BLUETOOTH_DRIVE_STOP) &&
      (command->turn_direction == BLUETOOTH_TURN_NONE))
  {
    avoidance->active = 0U;
  }

  if ((avoidance->active != 0U) &&
      ((uint32_t)(now_ms - avoidance->start_ms) >=
       ULTRASONIC_REVERSE_DURATION_MS))
  {
    avoidance->active = 0U;
  }

  if ((avoidance->active == 0U) && (new_sample != 0U) &&
      (sample->valid != 0U) &&
      (sample->distance_mm <= ULTRASONIC_AVOID_DISTANCE_MM) &&
      (command->drive_direction == BLUETOOTH_DRIVE_FORWARD))
  {
    avoidance->start_ms = now_ms;
    avoidance->reverse_speed_mm_s = command->speed_mm_s;
    avoidance->active = 1U;
  }

  if (avoidance->active != 0U)
  {
    effective_command->drive_direction = BLUETOOTH_DRIVE_BACKWARD;
    effective_command->turn_direction = BLUETOOTH_TURN_NONE;
    effective_command->speed_mm_s = avoidance->reverse_speed_mm_s;
  }
}

static void App_UltrasonicPublishSample(const UltrasonicSample_t *sample)
{
  UltrasonicSample_t dropped_sample;

  if ((sample == NULL) || (UltrasonicQueueHandle == NULL))
  {
    return;
  }
  if (osMessageQueuePut(UltrasonicQueueHandle, sample, 0U, 0U) != osOK)
  {
    (void)osMessageQueueGet(UltrasonicQueueHandle, &dropped_sample, NULL, 0U);
    (void)osMessageQueuePut(UltrasonicQueueHandle, sample, 0U, 0U);
  }
}

static void App_BuildBalanceSetpoint(const BluetoothCommand_t *command,
                                     BalanceSetpoint_t *setpoint)
{
  float counts_per_wheel;
  uint8_t turn_divisor;

  if (setpoint == NULL)
  {
    return;
  }
  (void)memset(setpoint, 0, sizeof(*setpoint));
  if (command == NULL)
  {
    return;
  }

  counts_per_wheel = ((float)command->speed_mm_s *
                      MOTOR_ENCODER_SAMPLE_PERIOD_S *
                      MOTOR_ENCODER_COUNTS_PER_REV) /
                     (APP_PI_F * MOTOR_WHEEL_DIAMETER_MM);
  setpoint->wheel_speed_sum_target_counts =
    (float)command->drive_direction * 2.0f * counts_per_wheel;

  turn_divisor = command->turn_divisor;
  if (turn_divisor == 0U)
  {
    turn_divisor = BLUETOOTH_TURN_DIVISOR_SLOW;
  }
  setpoint->turn_rate_target_dps =
    (float)command->turn_direction * BLUETOOTH_TURN_RATE_DPS /
    (float)turn_divisor;
  setpoint->translation_active = (uint8_t)(
    (command->drive_direction != BLUETOOTH_DRIVE_STOP) ? 1U : 0U);
}

static HAL_StatusTypeDef App_BluetoothSetBaud(uint32_t baud_rate)
{
  HAL_StatusTypeDef status;

  app_bluetooth_rx_enabled = 0U;
  (void)HAL_UART_AbortReceive(&huart3);
  huart3.Init.BaudRate = baud_rate;
  status = HAL_UART_Init(&huart3);
  if (status != HAL_OK)
  {
    return status;
  }
  if (BluetoothRxQueueHandle != NULL)
  {
    (void)osMessageQueueReset(BluetoothRxQueueHandle);
  }
  app_bluetooth_rx_enabled = 1U;
  status = HAL_UART_Receive_IT(&huart3, &app_bluetooth_rx_byte, 1U);
  if (status != HAL_OK)
  {
    app_bluetooth_rx_enabled = 0U;
  }
  return status;
}

static uint8_t App_BluetoothAtExchange(const char *request,
                                       const char *expected,
                                       uint32_t timeout_ms,
                                       BluetoothAtResponse_t *response)
{
  BluetoothAtResponse_t local_response;
  BluetoothAtResponse_t *active_response = response;
  uint32_t start_ms;
  uint8_t byte;
  uint8_t matched = 0U;

  if ((request == NULL) || (expected == NULL) ||
      (BluetoothRxQueueHandle == NULL))
  {
    return 0U;
  }
  if (active_response == NULL)
  {
    active_response = &local_response;
  }
  BluetoothAtResponse_Init(active_response);
  while (osMessageQueueGet(BluetoothRxQueueHandle, &byte, NULL, 0U) == osOK)
  {
    /* Drain bytes left by the previous AT response without resetting in ISR. */
  }
  if (HAL_UART_Transmit(&huart3, (uint8_t *)request,
                        (uint16_t)strlen(request), 100U) != HAL_OK)
  {
    return 0U;
  }

  start_ms = HAL_GetTick();
  while ((uint32_t)(HAL_GetTick() - start_ms) < timeout_ms)
  {
    uint32_t elapsed = (uint32_t)(HAL_GetTick() - start_ms);
    uint32_t remaining;

    if (elapsed >= timeout_ms)
    {
      break;
    }
    remaining = timeout_ms - elapsed;

    if (osMessageQueueGet(BluetoothRxQueueHandle, &byte, NULL,
                          remaining) != osOK)
    {
      break;
    }
    BluetoothAtResponse_Push(active_response, byte);
    if (BluetoothAtResponse_Contains(active_response, expected) != 0U)
    {
      matched = 1U;
    }
    /* Keep the complete response line, not only its prefix (for example
       "+NAME=").  The name bytes arrive after the prefix. */
    if ((matched != 0U) && (byte == (uint8_t)'\n'))
    {
      return 1U;
    }
  }
  return matched;
}

static void App_BluetoothConfigureJdy33(void)
{
  BluetoothAtResponse_t response;
  uint8_t at_ready;
  uint8_t spp_name_matches = 0U;
  uint8_t ble_name_matches = 0U;

  /*
   * The WHEELTEC module exposes STATE/RXD/TXD/GND/VCC/PWRC and uses the
   * dual-mode JDY-33 command set.  Unlike HC-05 it accepts AT commands at the normal
   * 9600 baud while it is not connected.  PWRC is a low-active wake/command
   * pin and is not the HC-05 KEY pin.
   */
  App_SendText("#bt,at=probe,module=JDY-33,baud=9600,name="
               BLUETOOTH_DEVICE_NAME "\r\n");
  if (App_BluetoothSetBaud(BLUETOOTH_NORMAL_BAUD) != HAL_OK)
  {
    App_SendText("#bt,at=uart_error,fallback=9600\r\n");
    return;
  }
  osDelay(BLUETOOTH_AT_STARTUP_DELAY_MS);

  at_ready = App_BluetoothAtExchange("AT+VERSION\r\n", "+VERSION=",
                                     BLUETOOTH_AT_TIMEOUT_MS, &response);
  if (at_ready == 0U)
  {
    at_ready = App_BluetoothAtExchange("AT\r\n", "+OK",
                                       BLUETOOTH_AT_TIMEOUT_MS, &response);
  }
  if (at_ready != 0U)
  {
    App_SendText("#bt,at=ready,module=JDY-33\r\n");

    if (App_BluetoothAtExchange("AT+NAME\r\n", "+NAME=",
                                BLUETOOTH_AT_TIMEOUT_MS, &response) != 0U)
    {
      spp_name_matches = BluetoothAtResponse_Contains(
        &response, BLUETOOTH_DEVICE_NAME);
    }
    if (spp_name_matches == 0U)
    {
      App_SendText("#bt,name=spp,set,target=" BLUETOOTH_DEVICE_NAME "\r\n");
      (void)App_BluetoothAtExchange("AT+NAME" BLUETOOTH_DEVICE_NAME "\r\n",
                                    "+OK", BLUETOOTH_AT_TIMEOUT_MS, NULL);
      osDelay(BLUETOOTH_AT_COMMAND_DELAY_MS);
      (void)App_BluetoothAtExchange("AT+NAME\r\n", "+NAME=",
                                    BLUETOOTH_AT_TIMEOUT_MS, &response);
      spp_name_matches = BluetoothAtResponse_Contains(
        &response, BLUETOOTH_DEVICE_NAME);
    }
    App_SendText((spp_name_matches != 0U) ?
                 "#bt,name=spp,value=ByCar321,status=ok\r\n" :
                 "#bt,name=spp,value=ByCar321,status=verify_failed\r\n");

    if (App_BluetoothAtExchange("AT+NAMB\r\n", "+NAME=",
                                BLUETOOTH_AT_TIMEOUT_MS, &response) != 0U)
    {
      ble_name_matches = BluetoothAtResponse_Contains(
        &response, BLUETOOTH_DEVICE_NAME);
    }
    if (ble_name_matches == 0U)
    {
      App_SendText("#bt,name=ble,set,target=" BLUETOOTH_DEVICE_NAME "\r\n");
      (void)App_BluetoothAtExchange("AT+NAMB" BLUETOOTH_DEVICE_NAME "\r\n",
                                    "+OK", BLUETOOTH_AT_TIMEOUT_MS, NULL);
      osDelay(BLUETOOTH_AT_COMMAND_DELAY_MS);
      (void)App_BluetoothAtExchange("AT+NAMB\r\n", "+NAME=",
                                    BLUETOOTH_AT_TIMEOUT_MS, &response);
      ble_name_matches = BluetoothAtResponse_Contains(
        &response, BLUETOOTH_DEVICE_NAME);
    }
    App_SendText((ble_name_matches != 0U) ?
                 "#bt,name=ble,value=ByCar321,status=ok\r\n" :
                 "#bt,name=ble,value=ByCar321,status=verify_failed\r\n");

    if ((App_BluetoothAtExchange("AT+BAUD\r\n", "+BAUD=",
                                 BLUETOOTH_AT_TIMEOUT_MS, &response) == 0U) ||
        (BluetoothAtResponse_Contains(&response, "+BAUD=4") == 0U))
    {
      App_SendText("#bt,baud=set,value=9600\r\n");
      (void)App_BluetoothAtExchange("AT+BAUD4\r\n", "+OK",
                                    BLUETOOTH_AT_TIMEOUT_MS, NULL);
    }
    (void)App_BluetoothAtExchange("AT+RESET\r\n", "+OK",
                                  BLUETOOTH_AT_TIMEOUT_MS, NULL);
    osDelay(BLUETOOTH_AT_RESET_DELAY_MS);
  }
  else
  {
    App_SendText("#bt,at=not_detected,module=JDY-33,fallback=remote\r\n");
  }

  App_SendText("#bt,mode=remote,baud=9600,raw_timeout=500ms,on_timeout=10000ms\r\n");
}

static void App_BluetoothPublishCommand(const BluetoothCommand_t *command)
{
  BluetoothCommand_t dropped_command;

  if ((command == NULL) || (BluetoothCommandQueueHandle == NULL))
  {
    return;
  }
  if (osMessageQueuePut(BluetoothCommandQueueHandle, command, 0U, 0U) != osOK)
  {
    (void)osMessageQueueGet(BluetoothCommandQueueHandle, &dropped_command,
                            NULL, 0U);
    (void)osMessageQueuePut(BluetoothCommandQueueHandle, command, 0U, 0U);
  }
}

static void App_QueueTelemetry(uint32_t now_ms,
                               uint16_t loop_dt_ms,
                               uint32_t data_ready_missed,
                               const BalanceSample_t *sample,
                               const MotorTelemetry_t *telemetry,
                               const BalanceOutput_t *output,
                               const BluetoothCommand_t *bluetooth,
                               const UltrasonicSample_t *ultrasonic,
                               uint8_t grayscale_code,
                               uint8_t avoidance_active)
{
  AppTelemetryFrame_t frame;
  AppTelemetryFrame_t dropped_frame;

  if ((TelemetryQueueHandle == NULL) || (sample == NULL) ||
      (telemetry == NULL) || (output == NULL) || (bluetooth == NULL) ||
      (ultrasonic == NULL))
  {
    return;
  }

  frame.now_ms = now_ms;
  frame.loop_dt_ms = loop_dt_ms;
  frame.data_ready_missed = data_ready_missed;
  frame.sample = *sample;
  frame.motor = *telemetry;
  frame.output = *output;
  frame.bluetooth = *bluetooth;
  frame.ultrasonic = *ultrasonic;
  frame.grayscale_code = grayscale_code;
  frame.avoidance_active = avoidance_active;
  if (osMessageQueuePut(TelemetryQueueHandle, &frame, 0U, 0U) != osOK)
  {
    (void)osMessageQueueGet(TelemetryQueueHandle, &dropped_frame, NULL, 0U);
    (void)osMessageQueuePut(TelemetryQueueHandle, &frame, 0U, 0U);
  }
}

static void App_SendTelemetry(const AppTelemetryFrame_t *frame)
{
  char line[256];
  char pitch[18];
  char gyro[18];
  char target[18];
  BalanceSetpoint_t debug_setpoint;
  const char *bluetooth_state;
  const char *drive;
  const char *turn;
  uint32_t command_age_ms;

  if (frame == NULL)
  {
    return;
  }
  App_FormatFloat(pitch, sizeof(pitch), frame->sample.pitch_deg, 2U);
  App_FormatFloat(gyro, sizeof(gyro), frame->sample.gyro_pitch_dps, 2U);
  App_BuildBalanceSetpoint(&frame->bluetooth, &debug_setpoint);
  App_FormatFloat(target, sizeof(target),
                  debug_setpoint.wheel_speed_sum_target_counts, 1U);
  bluetooth_state = (frame->bluetooth.active != 0U) ? "LINK" : "IDLE";
  drive = (frame->bluetooth.drive_direction == BLUETOOTH_DRIVE_FORWARD) ? "F" :
          ((frame->bluetooth.drive_direction == BLUETOOTH_DRIVE_BACKWARD) ? "B" : "S");
  turn = (frame->bluetooth.turn_direction == BLUETOOTH_TURN_LEFT) ? "L" :
         ((frame->bluetooth.turn_direction == BLUETOOTH_TURN_RIGHT) ? "R" : "N");
  command_age_ms = (frame->bluetooth.last_command_ms == 0U) ? 0U :
                   (frame->now_ms - frame->bluetooth.last_command_ms);

  (void)snprintf(line, sizeof(line),
                 "#key,%lu,%s,%lu,%s,%s,%u,%s,%s,%s,%d,%d,%ld,%ld,%d,%d,%s,%u,%lu,%u,%u,%u\r\n",
                 (unsigned long)frame->now_ms,
                 bluetooth_state,
                 (unsigned long)command_age_ms,
                 drive,
                 turn,
                 (unsigned int)frame->bluetooth.speed_mm_s,
                 target,
                 pitch,
                 gyro,
                 frame->motor.left_delta,
                 frame->motor.right_delta,
                 (long)frame->output.balance_pwm,
                 (long)frame->output.velocity_pwm,
                 frame->motor.left_pwm,
                 frame->motor.right_pwm,
                 App_StateText(frame->output.state),
                 (unsigned int)frame->loop_dt_ms,
                 (unsigned long)frame->data_ready_missed,
                 (unsigned int)frame->ultrasonic.distance_mm,
                 (unsigned int)frame->ultrasonic.valid,
                 (unsigned int)frame->avoidance_active);
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
    case BALANCE_STATE_OVERRUN:
      return "OVERRUN";
    case BALANCE_STATE_STOPPED:
    default:
      return "STOPPED";
  }
}
/* USER CODE END Application */
