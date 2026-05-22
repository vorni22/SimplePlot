#include "Keyboard.h"

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

KeyboardMatrix::KeyboardMatrix(const uint8_t* rowPins, uint8_t rowCount,
							   const uint8_t* colPins, uint8_t colCount)
	: rows_(rowPins),
	  cols_(colPins),
	  rowCount_(rowCount),
	  colCount_(colCount),
	  pending_(false),
	  rowMask_(0),
	  colMask_(0),
	  state_(0) {}

void KeyboardMatrix::begin() {
	rowMask_ = 0;
	colMask_ = 0;

	for (uint8_t i = 0; i < rowCount_; ++i) {
		rowMask_ |= (uint8_t)(1u << rows_[i]);
	}
	for (uint8_t i = 0; i < colCount_; ++i) {
		colMask_ |= (uint8_t)(1u << cols_[i]);
	}

	// Rows = outputs low (idle), columns = inputs with pull-ups (Port K only).
	DDRK |= rowMask_;
	DDRK &= (uint8_t)~colMask_;
	PORTK |= colMask_;
	PORTK &= (uint8_t)~rowMask_;

	// Enable PCINT on column pins (PCINT16..23 -> PCMSK2).
	PCICR |= (1 << PCIE2);
	PCMSK2 |= colMask_;
}

void KeyboardMatrix::onPinChange() {
	pending_ = true;
}

void KeyboardMatrix::poll() {
	if (!pending_) {
		return;
	}

	pending_ = false;
	scan();
}

bool KeyboardMatrix::isPressed(uint8_t row, uint8_t col) const {
	if (row >= rowCount_ || col >= colCount_) {
		return false;
	}

	uint8_t bitIndex = (uint8_t)(row * colCount_ + col);
	return (state_ & (uint16_t)(1u << bitIndex)) != 0;
}

uint16_t KeyboardMatrix::getStateMask() const {
	return state_;
}

void KeyboardMatrix::printPressed(const KeyboardMatrix& keyboard) {
	for (uint8_t r = 0; r < keyboard.rowCount_; ++r) {
		for (uint8_t c = 0; c < keyboard.colCount_; ++c) {
			if (keyboard.isPressed(r, c)) {
				printf("K%u%u pressed\n", r, c);
			}
		}
	}
}

void KeyboardMatrix::scan() {
	state_ = 0;

	for (uint8_t r = 0; r < rowCount_; ++r) {
		PORTK |= rowMask_;
		PORTK &= (uint8_t)~(1u << rows_[r]);

		_delay_us(5);

		uint8_t colRead = (uint8_t)((~PINK) & colMask_);
		for (uint8_t c = 0; c < colCount_; ++c) {
			if (colRead & (uint8_t)(1u << cols_[c])) {
				uint8_t bitIndex = (uint8_t)(r * colCount_ + c);
				state_ |= (uint16_t)(1u << bitIndex);
			}
		}
	}

	PORTK &= (uint8_t)~rowMask_;
}