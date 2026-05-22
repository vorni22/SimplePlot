#include <avr/io.h>
#include <util/delay.h>
#include "spi.h"

void SPI_init(){

    /* TODO1: set MOSI and SCK output, all others input */
    DDRB |= (1<<SPI_MOSI) | (1<<SPI_SCK);

    /* TODO1: set SS PIN as output and set it on HIGH */
    DDRB |= (1<<SPI_CS);
    PORTB |= (1<<SPI_CS);

    /* TODO1: enable SPI, set it as Master, set clock rate at fosc/16 */
    SPCR |= (1<<SPE) | (1<<MSTR) | (1<<SPR0);
}

uint8_t SPI_exchange(uint8_t data){
    // TODO1: send a byte of data to the slave and return the response byte received from him in this transmission
    SPDR = data;
    while(bit_is_clear(SPSR, SPIF))
    ;
    return SPDR;
}

