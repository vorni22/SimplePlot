#ifndef ADC_H
#define ADC_H

#include <stdint.h>

void adc_init_freerun_interrupt(void);
uint16_t adc_get_channel(uint8_t channel);

#endif