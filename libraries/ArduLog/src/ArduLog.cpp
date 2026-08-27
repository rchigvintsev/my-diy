/// \file ArduLog.cpp
/// \brief Реализация простого логгера с уровнями для Arduino.
///
/// \author Roman Chigvintsev

#include "ArduLog.h"

ArduLog::ArduLog(const String& name, ArduLogLevel effectiveLevel) {
	_name = name;
	_effectiveLevel = effectiveLevel;
}

void ArduLog::trace(const String& message) {
	if (!isOff() && _effectiveLevel == ArduLogLevel::TRACE) {
		log(message, ArduLogLevel::TRACE);
	}
}

void ArduLog::debug(const String& message) {
	if (!isOff() && (_effectiveLevel == ArduLogLevel::TRACE || _effectiveLevel == ArduLogLevel::DEBUG)) {
		log(message, ArduLogLevel::DEBUG);
	}
}

void ArduLog::info(const String& message) {
	if (!isOff() && (_effectiveLevel == ArduLogLevel::TRACE || _effectiveLevel == ArduLogLevel::DEBUG
	    || _effectiveLevel == ArduLogLevel::INFO)) {
		log(message, ArduLogLevel::INFO);
	}
}

void ArduLog::warn(const String& message) {
	if (!isOff() && (_effectiveLevel == ArduLogLevel::TRACE || _effectiveLevel == ArduLogLevel::DEBUG
	    || _effectiveLevel == ArduLogLevel::INFO || _effectiveLevel == ArduLogLevel::WARN)) {
		log(message, ArduLogLevel::WARN);
	}
}

void ArduLog::error(const String& message) {
	if (!isOff()) {
		log(message, ArduLogLevel::ERROR);
	}
}

boolean ArduLog::isOff(void) {
	return _effectiveLevel == ArduLogLevel::OFF;
}

void ArduLog::log(const String& message, ArduLogLevel level) {
	String resultMessage = "";

	// Разбиваем millis() на компоненты «часы:минуты:секунды.миллисекунды».
	unsigned long ms = millis();

	unsigned long hours = ms / ARDU_LOG_MILLIS_IN_HOUR;
	ms -= hours * ARDU_LOG_MILLIS_IN_HOUR;

	unsigned long minutes = ms / ARDU_LOG_MILLIS_IN_MINUTE;
	ms -= minutes * ARDU_LOG_MILLIS_IN_MINUTE;

	unsigned long seconds = ms / 1000;
	ms -= seconds * 1000;

	// Форматируем временную метку с ведущими нулями.
	if (hours < 10) {
		resultMessage += "0";
	}
	resultMessage += String(hours) + ":";
	if (minutes < 10) {
		resultMessage += "0";
	}
	resultMessage += String(minutes) + ":";
	if (seconds < 10) {
		resultMessage += "0";
	}
	resultMessage += String(seconds) + ".";
	if (ms < 100) {
		resultMessage += "0";
		if (ms < 10) {
			resultMessage += "0";
		}
	}
	resultMessage += String(ms) + " ";

	switch (level) {
		case ArduLogLevel::TRACE:
			resultMessage += "TRACE ";
			break;
		case ArduLogLevel::DEBUG:
			resultMessage += "DEBUG ";
			break;
		case ArduLogLevel::INFO:
			resultMessage += "INFO ";
			break;
		case ArduLogLevel::WARN:
			resultMessage += "WARN ";
			break;
		case ArduLogLevel::ERROR:
			resultMessage += "ERROR ";
			break;
	}

	Serial.println(resultMessage + _name + " - " + message);
}
