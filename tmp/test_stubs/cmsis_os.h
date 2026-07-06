#ifndef TEST_CMSIS_OS_H
#define TEST_CMSIS_OS_H
#include <stdint.h>
typedef void *osThreadId_t;
typedef int32_t osPriority_t;
typedef struct { const char *name; uint32_t stack_size; osPriority_t priority; } osThreadAttr_t;
#define osPriorityNormal 0
void *osThreadNew(void (*func)(void *), void *argument, const osThreadAttr_t *attr);
void osDelay(uint32_t ticks);
uint32_t osKernelGetTickCount(void);
int32_t osDelayUntil(uint32_t ticks);
#endif
