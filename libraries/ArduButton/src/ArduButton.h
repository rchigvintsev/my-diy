/// \file ArduButton.h
/// \brief Обёртка над тактовой кнопкой с подавлением дребезга и распознаванием клика и удержания.
///
/// Особенности:
///  - вход с подтяжкой к питанию (INPUT_PULLUP); кнопка считается нажатой при логическом 0 на пине;
///  - программное подавление дребезга по таймауту;
///  - три состояния: «отпущена», «нажата», «удерживается»;
///  - метод isClicked() возвращает true ровно один раз после полного цикла «нажатие — отпускание» (если не было
///    перехода в HELD);
///  - метод update() должен вызываться регулярно (например, по прерыванию таймера).
///
/// \author Roman Chigvintsev

#pragma once

#include <Arduino.h>

/// Кнопка отпущена.
#define ARDU_BUTTON_STATE_RELEASED 0
/// Кнопка нажата, но удержание ещё не зарегистрировано.
#define ARDU_BUTTON_STATE_PRESSED  1
/// Кнопка удерживается (нажата дольше порога удержания).
#define ARDU_BUTTON_STATE_HELD     2

/// Таймаут подавления дребезга по умолчанию, мс.
#define ARDU_BUTTON_DEFAULT_DEBOUNCE_TIMEOUT_MILLIS  50
/// Порог удержания по умолчанию, мс.
#define ARDU_BUTTON_DEFAULT_HOLD_TIMEOUT_MILLIS     500

/// \brief Тактовая кнопка с подавлением дребезга и распознаванием клика и удержания.
class ArduButton {
private:
	byte _pin;
	int _state;
	boolean _debounceState;
	unsigned long _debounceTime;
	byte _clickCounter;
	unsigned int _debounceTimeoutMillis;
	unsigned int _holdTimeoutMillis;
public:
	/// \brief Конструктор.
	/// \param pin Номер пина, к которому подключена кнопка (INPUT_PULLUP).
	ArduButton(byte pin);

	/// \brief Обновляет состояние кнопки по текущему уровню на пине.
	///
	/// Должен вызываться регулярно (минимум каждые несколько миллисекунд), например, из обработчика прерывания таймера.
	void update(void);

	/// \brief Сообщает, был ли зарегистрирован клик с момента предыдущего вызова.
	///
	/// Возвращает true ровно один раз после полного цикла «нажатие — отпускание», не перешедшего в удержание.
	/// Внутренний счётчик кликов сбрасывается.
	boolean isClicked(void);

	/// \brief Сообщает, находится ли кнопка в состоянии «нажата» (но ещё не удерживается).
	boolean isPressed(void);

	/// \brief Сообщает, находится ли кнопка в состоянии «отпущена».
	boolean isReleased(void);

	/// \brief Сообщает, находится ли кнопка в состоянии «удерживается».
	boolean isHeld(void);

	/// \brief Устанавливает таймаут подавления дребезга в миллисекундах.
	/// \param debounceTimeoutMillis Новое значение таймаута, мс.
	void setDebounceTimeoutMillis(unsigned int debounceTimeoutMillis);

	/// \brief Устанавливает порог удержания в миллисекундах.
	/// \param holdTimeoutMillis Новое значение порога, мс.
	void setHoldTimeoutMillis(unsigned int holdTimeoutMillis);

	/// \brief Сбрасывает внутреннее состояние кнопки.
	///
	/// Полезно после смены контекста (например, входа или выхода из режима настройки), чтобы не подхватить случайные
	/// клики или удержания, оставшиеся «в очереди».
	void reset(void);
};
