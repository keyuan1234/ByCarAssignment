#ifndef __IMU_H
#define __IMU_H

#ifdef __cplusplus
extern "C" {
#endif

#include "balance_types.h"

#define IMU_SAMPLE_PERIOD_MS        5U

uint8_t Imu_Init(void);
void Imu_ResetFilter(void);
uint8_t Imu_ReadRaw(ImuRaw_t *raw);
uint8_t Imu_UpdateComplementary5ms(BalanceSample_t *sample);

#ifdef __cplusplus
}
#endif

#endif /* __IMU_H */
