#include "TestSD.h"

#include <stdio.h>
#include <avr/io.h>
#include <avr/delay.h>

FATFS test_fs;

static DWORD logWritePos = 0;
static DWORD logReadPos = 0;

void test_sd() {
    int mount = pf_mount(&test_fs);
    if (mount != FR_OK) {
        // Asteptati un timp si reincercati
        printf("SD Card Mount Failed. Retrying...\r\n");
        _delay_ms(1000);
    } else {
        printf("SD Card Mounted Successfully.\r\n");
        _delay_ms(2000);
    }

    char line[32];
    WORD bw;
    WORD br;
    char c;
    uint8_t i = 0;

    pf_open("data.txt");
    pf_lseek(logWritePos);
    pf_write("HELLO_SD", 9, &bw);
    printf("Wrote %d bytes\n", bw);
    pf_write(0, 0, &bw);

    pf_open("data.txt");
    pf_lseek(logReadPos);

    while (i < sizeof(line) - 1) {
        pf_read(&c, 1, &br);

        if (br == 0 || c == '\0') {
            break;
        }

        logReadPos++;

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            break;
        }

        line[i++] = c;
    }

    line[i] = '\0';

    printf("READ: <%s>\n", line);
}