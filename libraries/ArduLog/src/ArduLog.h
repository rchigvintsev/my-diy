#pragma once

#include <Arduino.h>

#define ARDU_LOG_MILLIS_IN_DAY 86400000
#define ARDU_LOG_MILLIS_IN_HOUR 3600000
#define ARDU_LOG_MILLIS_IN_MINUTE 60000

enum class ArduLogLevel {
	TRACE,
	DEBUG,
	INFO,
	WARN,
	ERROR,
	OFF
};

class ArduLogger {
private:
	String _name;
	ArduLogLevel _effectiveLevel;
	
	boolean isOff(void);
	void log(const String& message, ArduLogLevel level);
public:
	ArduLogger(const String& name, ArduLogLevel effectiveLevel = ArduLogLevel::DEBUG);
	void trace(const String& message);
	void debug(const String& message);
	void info(const String& message);
	void warn(const String& message);
	void error(const String& message);
};
