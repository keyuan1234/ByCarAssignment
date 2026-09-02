#include "ultrasonic.h"
#include "main.h"
#include "tim.h"
#include <string.h>

#define ULTRASONIC_TRIGGER_PULSE_US     15U
#define ULTRASONIC_MIN_ECHO_US          100U
#define ULTRASONIC_MAX_ECHO_US          29999U

typedef enum
{
  ULTRASONIC_STATE_IDLE = 0,
  ULTRASONIC_STATE_WAIT_RISING,
  ULTRASONIC_STATE_WAIT_FALLING,
  ULTRASONIC_STATE_RESULT_READY
} UltrasonicState_t;

static volatile UltrasonicState_t ultrasonic_state;
static volatile uint8_t ultrasonic_result_ready;
static UltrasonicSample_t ultrasonic_result;
static osThreadId_t ultrasonic_notify_thread;

static void Ultrasonic_StopTimer(void)
{
  (void)HAL_TIM_IC_Stop_IT(&htim2, TIM_CHANNEL_2);
  (void)HAL_TIM_Base_Stop_IT(&htim2);
  __HAL_TIM_SET_COUNTER(&htim2, 0U);
  __HAL_TIM_SET_CAPTUREPOLARITY(&htim2, TIM_CHANNEL_2,
                                TIM_INPUTCHANNELPOLARITY_RISING);
}

static void Ultrasonic_NotifyTask(void)
{
  if (ultrasonic_notify_thread != NULL)
  {
    (void)osThreadFlagsSet(ultrasonic_notify_thread,
                           ULTRASONIC_RESULT_THREAD_FLAG);
  }
}

static void Ultrasonic_Complete(uint16_t pulse_us, uint8_t valid)
{
  Ultrasonic_StopTimer();
  ultrasonic_result.timestamp_ms = HAL_GetTick();
  ultrasonic_result.pulse_us = pulse_us;
  ultrasonic_result.valid = valid;
  ultrasonic_result.distance_mm = (valid != 0U) ?
    (uint16_t)(((uint32_t)pulse_us * 17U) / 100U) : 0U;
  __DMB();
  ultrasonic_result_ready = 1U;
  ultrasonic_state = ULTRASONIC_STATE_RESULT_READY;
  Ultrasonic_NotifyTask();
}

void Ultrasonic_Init(osThreadId_t notify_thread)
{
  Ultrasonic_StopTimer();
  HAL_GPIO_WritePin(ULTRASONIC_TRIG_GPIO_Port, ULTRASONIC_TRIG_Pin,
                    GPIO_PIN_RESET);
  (void)memset(&ultrasonic_result, 0, sizeof(ultrasonic_result));
  ultrasonic_result_ready = 0U;
  ultrasonic_notify_thread = notify_thread;
  ultrasonic_state = ULTRASONIC_STATE_IDLE;
}

uint8_t Ultrasonic_StartMeasurement(void)
{
  HAL_StatusTypeDef status;

  if (ultrasonic_state != ULTRASONIC_STATE_IDLE)
  {
    return 0U;
  }

  ultrasonic_result_ready = 0U;
  __HAL_TIM_DISABLE(&htim2);
  __HAL_TIM_SET_COUNTER(&htim2, 0U);
  __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE | TIM_FLAG_CC2);
  __HAL_TIM_SET_CAPTUREPOLARITY(&htim2, TIM_CHANNEL_2,
                                TIM_INPUTCHANNELPOLARITY_RISING);
  ultrasonic_state = ULTRASONIC_STATE_WAIT_RISING;

  /* Base interrupt supplies the 30 ms timeout.  Start it before the input
     capture because HAL_TIM_Base_Start_IT owns the timer-wide state. */
  status = HAL_TIM_Base_Start_IT(&htim2);
  if (status != HAL_OK)
  {
    ultrasonic_state = ULTRASONIC_STATE_IDLE;
    return 0U;
  }
  status = HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_2);
  if (status != HAL_OK)
  {
    (void)HAL_TIM_Base_Stop_IT(&htim2);
    ultrasonic_state = ULTRASONIC_STATE_IDLE;
    return 0U;
  }

  HAL_GPIO_WritePin(ULTRASONIC_TRIG_GPIO_Port, ULTRASONIC_TRIG_Pin,
                    GPIO_PIN_SET);
  while (__HAL_TIM_GET_COUNTER(&htim2) < ULTRASONIC_TRIGGER_PULSE_US)
  {
    /* TIM2 runs at 1 MHz; this short wait only shapes the trigger pulse. */
  }
  HAL_GPIO_WritePin(ULTRASONIC_TRIG_GPIO_Port, ULTRASONIC_TRIG_Pin,
                    GPIO_PIN_RESET);
  return 1U;
}

uint8_t Ultrasonic_GetResult(UltrasonicSample_t *sample)
{
  uint32_t primask;

  if (sample == NULL)
  {
    return 0U;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  if (ultrasonic_result_ready == 0U)
  {
    if (primask == 0U)
    {
      __enable_irq();
    }
    return 0U;
  }
  __DMB();
  *sample = ultrasonic_result;
  ultrasonic_result_ready = 0U;
  ultrasonic_state = ULTRASONIC_STATE_IDLE;
  if (primask == 0U)
  {
    __enable_irq();
  }
  return 1U;
}

void Ultrasonic_AbortMeasurement(void)
{
  if ((ultrasonic_state == ULTRASONIC_STATE_WAIT_RISING) ||
      (ultrasonic_state == ULTRASONIC_STATE_WAIT_FALLING))
  {
    Ultrasonic_Complete(0U, 0U);
  }
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  uint16_t pulse_us;

  if ((htim == NULL) || (htim->Instance != TIM2) ||
      (htim->Channel != HAL_TIM_ACTIVE_CHANNEL_2))
  {
    return;
  }

  if (ultrasonic_state == ULTRASONIC_STATE_WAIT_RISING)
  {
    __HAL_TIM_SET_COUNTER(htim, 0U);
    __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_2,
                                  TIM_INPUTCHANNELPOLARITY_FALLING);
    ultrasonic_state = ULTRASONIC_STATE_WAIT_FALLING;
  }
  else if (ultrasonic_state == ULTRASONIC_STATE_WAIT_FALLING)
  {
    pulse_us = (uint16_t)HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
    Ultrasonic_Complete(pulse_us,
      (uint8_t)(((pulse_us >= ULTRASONIC_MIN_ECHO_US) &&
                 (pulse_us <= ULTRASONIC_MAX_ECHO_US)) ? 1U : 0U));
  }
}

void Ultrasonic_HandleTimeoutIRQ(TIM_HandleTypeDef *htim)
{
  if ((htim == NULL) || (htim->Instance != TIM2))
  {
    return;
  }

  if ((ultrasonic_state == ULTRASONIC_STATE_WAIT_RISING) ||
      (ultrasonic_state == ULTRASONIC_STATE_WAIT_FALLING))
  {
    Ultrasonic_Complete(0U, 0U);
  }
}
