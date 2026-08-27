/// \file ArduButton.cpp
/// \brief Реализация тактовой кнопки с подавлением дребезга и распознаванием клика и удержания.
///
/// \author Roman Chigvintsev

#include "ArduButton.h"

ArduButton::ArduButton(byte pin) {
	_pin = pin;
	_state = ARDU_BUTTON_STATE_RELEASED;
	_debounceState = false;
	_debounceTime = 0;
	_clickCounter = 0;
	_debounceTimeoutMillis = ARDU_BUTTON_DEFAULT_DEBOUNCE_TIMEOUT_MILLIS;
	_holdTimeoutMillis = ARDU_BUTTON_DEFAULT_HOLD_TIMEOUT_MILLIS;
}

void ArduButton::update(void) {
	unsigned long now = millis();

	// Кнопка считается нажатой при логическом 0 на входе (схема с INPUT_PULLUP).
	boolean state = !digitalRead(_pin);
	if (state && _state != ARDU_BUTTON_STATE_PRESSED && _state != ARDU_BUTTON_STATE_HELD) {
		if (!_debounceState) {
			// Первый «нажатый» отсчёт после отпускания: запускаем подавление дребезга.
			_debounceState = true;
			_debounceTime = now;
			_clickCounter = 0;
		} else if (now - _debounceTime >= _debounceTimeoutMillis) {
			// Дребезг закончился, фиксируем стабильное нажатие.
			_state = ARDU_BUTTON_STATE_PRESSED;
		}
	}

	if (!state && _state != ARDU_BUTTON_STATE_RELEASED) {
		_debounceState = false;
		// Кнопка отпущена после короткого нажатия — засчитываем клик. Если был переход в HELD, клик не засчитывается.
		if (_state == ARDU_BUTTON_STATE_PRESSED) {
			_clickCounter = 1;
		}
		_state = ARDU_BUTTON_STATE_RELEASED;
	}

	// Долгое удержание — переходим в HELD; клик уже не будет засчитан после отпускания.
	if (_state == ARDU_BUTTON_STATE_PRESSED && now - _debounceTime >= _holdTimeoutMillis) {
		_state = ARDU_BUTTON_STATE_HELD;
	}
}

boolean ArduButton::isClicked(void) {
	if (_clickCounter == 1) {
		_clickCounter = 0;
		return true;
	}
	return false;
}

boolean ArduButton::isPressed(void) {
	return _state == ARDU_BUTTON_STATE_PRESSED;
}

boolean ArduButton::isReleased(void) {
	return _state == ARDU_BUTTON_STATE_RELEASED;
}

boolean ArduButton::isHeld(void) {
	return _state == ARDU_BUTTON_STATE_HELD;
}

void ArduButton::setDebounceTimeoutMillis(unsigned int debounceTimeoutMillis) {
	_debounceTimeoutMillis = debounceTimeoutMillis;
}

void ArduButton::setHoldTimeoutMillis(unsigned int holdTimeoutMillis) {
	_holdTimeoutMillis = holdTimeoutMillis;
}

void ArduButton::reset(void) {
	_state = ARDU_BUTTON_STATE_RELEASED;
	_debounceState = false;
	_debounceTime = 0;
	_clickCounter = 0;
}
