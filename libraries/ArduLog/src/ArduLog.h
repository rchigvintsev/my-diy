/// \file ArduLog.h
/// \brief Простой логгер для Arduino с уровнями TRACE / DEBUG / INFO / WARN / ERROR.
///
/// Особенности:
///  - вывод в Serial; ответственность за инициализацию Serial.begin() лежит
///    на пользователе;
///  - формат записи: «ЧЧ:ММ:СС.ммм УРОВЕНЬ ИМЯ - сообщение»;
///  - уровень логирования задаётся для каждого логгера в конструкторе;
///  - сообщения принимаются по const-ссылке для экономии RAM.
///
/// \author Roman Chigvintsev

#pragma once

#include <Arduino.h>

#define ARDU_LOG_MILLIS_IN_DAY    86400000
#define ARDU_LOG_MILLIS_IN_HOUR   3600000
#define ARDU_LOG_MILLIS_IN_MINUTE 60000

/// \brief Уровни логирования.
///
/// Порядок (от наиболее подробного к наименее подробному): TRACE, DEBUG, INFO,
/// WARN, ERROR. Уровень OFF полностью отключает логирование.
enum class ArduLogLevel {
	TRACE,
	DEBUG,
	INFO,
	WARN,
	ERROR,
	OFF
};

/// \brief Логгер с именем и настраиваемым эффективным уровнем.
class ArduLogger {
private:
	String _name;
	ArduLogLevel _effectiveLevel;

	/// \brief Сообщает, что эффективный уровень — OFF.
	boolean isOff(void);

	/// \brief Форматирует и выводит сообщение в Serial.
	/// \param message Текст сообщения.
	/// \param level   Уровень логирования сообщения.
	void log(const String& message, ArduLogLevel level);
public:
	/// \brief Конструктор.
	/// \param name           Имя логгера, попадает в каждое сообщение.
	/// \param effectiveLevel Эффективный уровень логирования (по умолчанию DEBUG).
	ArduLogger(const String& name, ArduLogLevel effectiveLevel = ArduLogLevel::DEBUG);

	/// \brief Логирует сообщение уровня TRACE.
	/// \param message Текст сообщения.
	void trace(const String& message);

	/// \brief Логирует сообщение уровня DEBUG.
	/// \param message Текст сообщения.
	void debug(const String& message);

	/// \brief Логирует сообщение уровня INFO.
	/// \param message Текст сообщения.
	void info(const String& message);

	/// \brief Логирует сообщение уровня WARN.
	/// \param message Текст сообщения.
	void warn(const String& message);

	/// \brief Логирует сообщение уровня ERROR.
	/// \param message Текст сообщения.
	void error(const String& message);
};
