/// \file ArduTimer.h
/// \brief Программный таймер с произвольным интервалом, основанный на millis().
///
/// Особенности:
///  - неблокирующий: проверка истечения интервала выполняется методом isWentOff();
///  - корректно работает при переполнении millis() (раз в ~49.7 суток) за счёт свойств беззнаковой арифметики;
///  - при последовательных пропусках срабатываний интервал «догоняет» текущее время, сохраняя ритм без накопления
///    ошибки.
///
/// \author Roman Chigvintsev

#pragma once

#include <Arduino.h>

/// \brief Программный таймер на основе millis().
class ArduTimer {
private:
	unsigned long _time;
	unsigned long _intervalMillis;
public:
	/// \brief Конструктор.
	/// \param intervalMillis Интервал срабатывания в миллисекундах.
	ArduTimer(unsigned long intervalMillis);

	/// \brief Сообщает, сработал ли таймер с момента предыдущего вызова.
	///
	/// Возвращает true, если с момента последнего срабатывания или сброса прошло не меньше интервала. При интервале 0
	/// всегда возвращает true.
	boolean isWentOff(void);

	/// \brief Возвращает оставшееся до срабатывания время в миллисекундах.
	///
	/// При интервале 0 всегда возвращает 0.
	unsigned long getRemainingTimeMillis(void);

	/// \brief Возвращает текущий интервал срабатывания в миллисекундах.
	unsigned long getIntervalMillis(void);

	/// \brief Устанавливает новый интервал срабатывания.
	///
	/// Если новое значение отличается от текущего, таймер сбрасывается.
	///
	/// \param intervalMillis Новый интервал в миллисекундах.
	void setIntervalMillis(unsigned long intervalMillis);

	/// \brief Сбрасывает таймер, начиная отсчёт с текущего момента.
	void reset(void);
};
