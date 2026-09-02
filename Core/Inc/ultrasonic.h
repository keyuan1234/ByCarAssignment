#ifndef __ULTRASONIC_H
#define __ULTRASONIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os2.h"
#include "stm32f1xx_hal.h"
#include <stdint.h>

/* User tuning entry: obstacle distance in millimetres. */
#ifndef ULTRASONIC_AVOID_DISTANCE_MM
#define ULTRASONIC_AVOID_DISTANCE_MM          450U
#endif

#ifndef ULTRASONIC_REVERSE_DURATION_MS
#define ULTRASONIC_REVERSE_DURATION_MS        500U
#endif

#ifndef ULTRASONIC_MEASUREMENT_PERIOD_MS
#define ULTRASONIC_MEASUREMENT_PERIOD_MS      60U
#endif

#define ULTRASONIC_ECHO_TIMEOUT_MS            30U
#define ULTRASONIC_RESULT_THREAD_FLAG         0x00000001U

typedef struct
{
  uint32_t timestamp_ms;
  uint16_t pulse_us;
  uint16_t distance_mm;
  uint8_t valid;
} UltrasonicSample_t;

void Ultrasonic_Init(osThreadId_t notify_thread);
uint8_t Ultrasonic_StartMeasurement(void);
uint8_t Ultrasonic_GetResult(UltrasonicSample_t *sample);
void Ultrasonic_AbortMeasurement(void);
void Ultrasonic_HandleTimeoutIRQ(TIM_HandleTypeDef *htim);

#ifdef __cplusplus
}
#endif

#endif /* __ULTRASONIC_H */
