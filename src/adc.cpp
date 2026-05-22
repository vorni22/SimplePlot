#include "adc.h"
#include <avr/io.h>
#include <avr/interrupt.h>

static volatile uint16_t adc_values[4] = {0, 0, 0, 0};
static volatile uint8_t current_channel = 0;

void adc_init_freerun_interrupt(void) {
    // AVcc reference, start at ADC0
    ADMUX = (1 << REFS0) | 0;

    // Enable ADC, interrupt, prescaler 128
    ADCSRA = (1 << ADEN) | (1 << ADIE) |
             (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

    // Trigger source not used (single conversion in ISR)
    ADCSRB = 0;

    // Start first conversion
    ADCSRA |= (1 << ADSC);
}

uint16_t adc_get_channel(uint8_t channel) {
    channel &= 0x03; // only 0..3
    return adc_values[channel];
}

ISR(ADC_vect) {
    uint16_t value = ADC;

    adc_values[current_channel] = value;

    current_channel++;
    if (current_channel >= 4) {
        current_channel = 0;
    }

    // Switch channel for next conversion
    ADMUX = (ADMUX & 0xF0) | current_channel;

    // Start next conversion after changing channel
    ADCSRA |= (1 << ADSC);
}