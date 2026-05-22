#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

class KeyboardMatrix {
public:
    KeyboardMatrix(const uint8_t* rowPins, uint8_t rowCount,
                   const uint8_t* colPins, uint8_t colCount);

    void begin();
    void onPinChange();     // ISR hook: sets a flag
    void poll();            // call in loop; scans if flag set
    bool isPressed(uint8_t row, uint8_t col) const;
    uint16_t getStateMask() const;
    static void printPressed(const KeyboardMatrix& keyboard);

private:
    void scan();

    const uint8_t* rows_;
    const uint8_t* cols_;
    uint8_t rowCount_;
    uint8_t colCount_;
    volatile bool pending_;
    uint8_t rowMask_;
    uint8_t colMask_;
    uint16_t state_;
};

#endif