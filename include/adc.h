#ifndef __ADC_H__
#define __ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern ADC_HandleTypeDef hadc1;

void MX_ADC1_Init(void);
uint16_t ADC1_ReadChannel(uint32_t channel);
uint16_t Joystick_ReadX(void);
uint16_t Joystick_ReadY(void);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */
