#include <LiquidCrystal_I2C.h>
#include <TimerOne.h>
#include <ArduLog.h>
#include <ArduButton.h>
#include <ArduTimer.h>
#include <EEPROM.h>

#define DEBUG

#define UNSIGNED_LONG_MAX_VALUE 4294967295UL

#define MILLIS_IN_DAY 86400000
#define MILLIS_IN_HOUR 3600000
#define MILLIS_IN_MINUTE 60000

#define SECONDS_IN_DAY 86400
#define SECONDS_IN_HOUR 3600

#define DEFAULT_PUMP_TURN_ON_INTERVAL_MILLIS 259200000UL // 3 дня
#define DEFAULT_PUMP_RUN_INTERVAL_MILLIS     30000UL

#define WATERING_ANIMATION_LENGTH                  2
#define WATERING_ANIMATION_FRAME_SIZE              8
#define WATERING_ANIMATION_FRAME_INTERVAL_MILLIS 500

#define LCD_BACKLIGHT_TIMEOUT_MILLIS 10000
#define LCD_BLINK_INTERVAL_MILLIS      500

#define SETUP_STEP_PUMP_TURN_ON_INTERVAL_DAYS     0
#define SETUP_STEP_PUMP_TURN_ON_INTERVAL_HOURS    1
#define SETUP_STEP_PUMP_TURN_ON_INTERVAL_MINUTES  2
#define SETUP_STEP_PUMP_TURN_OFF_INTERVAL_MINUTES 3
#define SETUP_STEP_PUMP_TURN_OFF_INTERVAL_SECONDS 4

#define BUTTON_SET_PIN   4
#define BUTTON_LEFT_PIN  7
#define BUTTON_RIGHT_PIN 8

#define PUMP_PIN 2

#ifdef DEBUG
ArduLogger logger("AutomaticWatering");
#endif

const byte WATERING_ANIMATION[][WATERING_ANIMATION_FRAME_SIZE][8] = {
  {
    {B00000, B00001, B00011, B00110, B01100, B11000, B10001, B10011},
    {B11100, B00111, B00011, B00111, B01111, B11111, B11111, B11111},
    {B00000, B00000, B10000, B11000, B11100, B11110, B11111, B11110},
    {B00000, B00000, B00000, B00000, B00000, B00000, B01000, B01100},
    {B01111, B00111, B00011, B00001, B00000, B00000, B00000, B00000},
    {B11111, B11111, B11111, B11111, B11111, B11111, B01111, B00111},
    {B11100, B11000, B10001, B10111, B11110, B11100, B10000, B00000},
    {B01110, B11111, B10000, B00001, B00001, B00000, B00100, B00100}
  },
  {
    {B00000, B00001, B00011, B00110, B01100, B11000, B10001, B10011},
    {B11100, B00111, B00011, B00111, B01111, B11111, B11111, B11111},
    {B00000, B00000, B10000, B11000, B11100, B11110, B11111, B11110},
    {B00000, B00000, B00000, B00000, B00000, B00000, B01000, B01100},
    {B01111, B00111, B00011, B00001, B00000, B00000, B00000, B00000},
    {B11111, B11111, B11111, B11111, B11111, B11111, B01111, B00111},
    {B11100, B11000, B10001, B10111, B11110, B11100, B10000, B00000},
    {B01110, B11111, B10000, B00100, B00100, B00000, B00001, B00001}
  }
};

const unsigned long CRC_TABLE[16] = {
  0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
  0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
  0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
  0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
};

ArduTimer pumpTurnOnTimer(DEFAULT_PUMP_TURN_ON_INTERVAL_MILLIS);
ArduTimer pumpRunTimer(DEFAULT_PUMP_RUN_INTERVAL_MILLIS);
ArduTimer wateringAnimationTimer(WATERING_ANIMATION_FRAME_INTERVAL_MILLIS);
ArduTimer lcdBacklightTimer(LCD_BACKLIGHT_TIMEOUT_MILLIS);
ArduTimer lcdBlinkTimer(LCD_BLINK_INTERVAL_MILLIS);

ArduButton setButton(BUTTON_SET_PIN);
ArduButton leftButton(BUTTON_LEFT_PIN);
ArduButton rightButton(BUTTON_RIGHT_PIN);

LiquidCrystal_I2C lcd(0x3F, 16, 2);
boolean lcdBacklightEnabled = true;
boolean lcdBlinkState = true;

boolean pumpRunning = false;
byte wateringAnimationFrameIndex = 0;

boolean setupMode = false;
byte setupStep = SETUP_STEP_PUMP_TURN_ON_INTERVAL_DAYS;

boolean setButtonHeld = false;
boolean leftButtonHeld = false;
boolean rightButtonHeld = false;

void displayTimeUntilNextWatering(unsigned long time, unsigned int blinkCount = 0);

void setup() {
  #ifdef DEBUG
  Serial.begin(9600);
  #endif

  pinMode(PUMP_PIN, OUTPUT);
  pinMode(BUTTON_SET_PIN, INPUT_PULLUP);
  pinMode(BUTTON_LEFT_PIN, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT_PIN, INPUT_PULLUP);

  // Set timer interval to 10 ms
  Timer1.initialize(10000);
  Timer1.attachInterrupt(updateButtons);

  lcd.init();
  lcd.backlight();

  checkEeprom();

  unsigned long pumpTurnOnInterval;
  EEPROM.get(0, pumpTurnOnInterval);
  // Защита от мусора в EEPROM: интервал включения 1 минута .. 30 суток.
  if (pumpTurnOnInterval < MILLIS_IN_MINUTE || pumpTurnOnInterval > 30UL * MILLIS_IN_DAY) {
    pumpTurnOnInterval = DEFAULT_PUMP_TURN_ON_INTERVAL_MILLIS;
  }
  #ifdef DEBUG
  logger.debug("Stored pump turn on interval (ms): " + String(pumpTurnOnInterval));
  #endif
  pumpTurnOnTimer.setIntervalMillis(pumpTurnOnInterval);

  unsigned long pumpRunInterval;
  EEPROM.get(4, pumpRunInterval);
  // Защита от мусора в EEPROM: интервал работы помпы 1 секунда .. 1 час.
  if (pumpRunInterval < 1000UL || pumpRunInterval > MILLIS_IN_HOUR) {
    pumpRunInterval = DEFAULT_PUMP_RUN_INTERVAL_MILLIS;
  }
  #ifdef DEBUG
  logger.debug("Stored pump run interval (ms): " + String(pumpRunInterval));
  #endif
  pumpRunTimer.setIntervalMillis(pumpRunInterval);
}

void loop() {
  updateLcdBacklight();

  handleSetButtonClick();
  handleLeftButtonClick();
  handleRightButtonClick();

  handleSetButtonHold();
  handleLeftButtonHold();
  handleRightButtonHold();
  
  updatePump();
  updateSetupScreen();
}

void updateButtons() {
  setButton.update();
  leftButton.update();
  rightButton.update();
}

void updateLcdBacklight() {
  boolean buttonPressed = isAnyButtonPressed();
  if (lcdBacklightEnabled) {
    if (buttonPressed) {
      lcdBacklightTimer.reset();
    } else if (lcdBacklightTimer.isWentOff()) {
      lcd.noBacklight();
      lcdBacklightEnabled = false;
    }
  } else if (buttonPressed) {
    lcd.backlight();
    lcdBacklightEnabled = true;
    lcdBacklightTimer.reset();
  }
}

void handleSetButtonClick() {
  if (setupMode && setButton.isClicked()) {
    resetLcdBlinkTimer();
    setupStep++;
    if (setupStep > SETUP_STEP_PUMP_TURN_OFF_INTERVAL_SECONDS) {
      setupStep = 0;
      lcd.clear();
      storePumpTurnOnInterval();
      storePumpTurnOffInterval();
      setupMode = false;
    }
  }
}

void handleSetButtonHold() {
  if (setButtonHeld) {
    setButtonHeld = !setButton.isReleased();
  }

  if (!pumpRunning && !setButtonHeld && setButton.isHeld()) {
    setButtonHeld = true;
    lcd.clear();
    // Discard any previous interactions with buttons
    leftButton.reset();
    rightButton.reset();

    if (setupMode) {
      setupMode = false;
      storePumpTurnOnInterval();
      storePumpTurnOffInterval();
    } else {
      resetLcdBlinkTimer();
      setupStep = 0;
      setupMode = true;
    }
  }
}

void handleLeftButtonClick() {
  if (setupMode && leftButton.isClicked()) {
    resetLcdBlinkTimer();
    if (setupStep < SETUP_STEP_PUMP_TURN_OFF_INTERVAL_MINUTES) {
      unsigned long interval = pumpTurnOnTimer.getIntervalMillis();

      if (setupStep == SETUP_STEP_PUMP_TURN_ON_INTERVAL_DAYS) {
        if (interval >= MILLIS_IN_DAY) {
          interval -= MILLIS_IN_DAY;
        }
      } else if (setupStep == SETUP_STEP_PUMP_TURN_ON_INTERVAL_HOURS) {
        if (interval >= MILLIS_IN_HOUR) {
          unsigned int hours = interval % MILLIS_IN_DAY / MILLIS_IN_HOUR;
          if (hours > 0) {
            interval -= MILLIS_IN_HOUR;
          }
        }
      } else if (interval >= MILLIS_IN_MINUTE) {
        unsigned long minutes = interval % MILLIS_IN_HOUR / MILLIS_IN_MINUTE;
        if (minutes > 0) {
          interval -= MILLIS_IN_MINUTE;
        }
      }

      if (interval == 0) {
        interval = MILLIS_IN_MINUTE;
      }
      pumpTurnOnTimer.setIntervalMillis(interval);
    } else {
      unsigned long interval = pumpRunTimer.getIntervalMillis();

      if (setupStep == SETUP_STEP_PUMP_TURN_OFF_INTERVAL_MINUTES) {
        if (interval >= MILLIS_IN_MINUTE) {
          interval -= MILLIS_IN_MINUTE;
        }
      } else if (interval >= 1000) {
        unsigned long seconds = interval % MILLIS_IN_MINUTE / 1000;
        if (seconds > 0) {
          interval -= 1000;
        }
      }

      if (interval == 0) {
        interval = 1000;
      }
      pumpRunTimer.setIntervalMillis(interval);
    }
  }
}

// Resets pump turn on timer
void handleLeftButtonHold() {
  if (!setupMode) {
    if (leftButtonHeld) {
      leftButtonHeld = !leftButton.isReleased();
    }

    if (!pumpRunning && !leftButtonHeld && leftButton.isHeld()) {
      leftButtonHeld = true;
      pumpTurnOnTimer.reset();
      displayTimeUntilNextWatering(pumpTurnOnTimer.getRemainingTimeMillis(), 3);
      // Reset timer again, since while we were blinking LCD, time went forward
      pumpTurnOnTimer.reset();
      #ifdef DEBUG
      logger.debug("Pump turn on timer is reset");
      #endif
    }
  }
}

void handleRightButtonClick() {
  if (setupMode && rightButton.isClicked()) {
    resetLcdBlinkTimer();
    if (setupStep < SETUP_STEP_PUMP_TURN_OFF_INTERVAL_MINUTES) {
      unsigned long interval = pumpTurnOnTimer.getIntervalMillis();
      if (setupStep == SETUP_STEP_PUMP_TURN_ON_INTERVAL_DAYS) {
        if (UNSIGNED_LONG_MAX_VALUE - interval >= MILLIS_IN_DAY) {
          interval += MILLIS_IN_DAY;
        }
      } else if (setupStep == SETUP_STEP_PUMP_TURN_ON_INTERVAL_HOURS) {
        if (UNSIGNED_LONG_MAX_VALUE - interval >= MILLIS_IN_HOUR) {
          unsigned int hours = interval % MILLIS_IN_DAY / MILLIS_IN_HOUR;
          if (hours < 23) {
            interval += MILLIS_IN_HOUR;
          }
        }
      } else if (UNSIGNED_LONG_MAX_VALUE - interval >= MILLIS_IN_MINUTE) {
        unsigned long minutes = interval % MILLIS_IN_HOUR / MILLIS_IN_MINUTE;
        if (minutes < 59) {
          interval += MILLIS_IN_MINUTE;
        }
      }
      pumpTurnOnTimer.setIntervalMillis(interval);
    } else {
      unsigned long interval = pumpRunTimer.getIntervalMillis();
      if (setupStep == SETUP_STEP_PUMP_TURN_OFF_INTERVAL_MINUTES) {
        if (UNSIGNED_LONG_MAX_VALUE - interval >= MILLIS_IN_MINUTE) {
          unsigned long minutes = interval % MILLIS_IN_HOUR / MILLIS_IN_MINUTE;
          if (minutes < 59) {
            interval += MILLIS_IN_MINUTE;
          }
        }
      } else if (UNSIGNED_LONG_MAX_VALUE - interval >= 1000) {
        unsigned long seconds = interval % MILLIS_IN_MINUTE / 1000;
        if (seconds < 59) {
          interval += 1000;
        }
      }
      pumpRunTimer.setIntervalMillis(interval);
    }
  }
}

// Turns on pump forcibly and resets pump turn on timer
void handleRightButtonHold() {
  if (!setupMode) {
    if (rightButtonHeld) {
      rightButtonHeld = !rightButton.isReleased();
    }

    if (!pumpRunning && !rightButtonHeld && rightButton.isHeld()) {
      rightButtonHeld = true;
      pumpTurnOnTimer.reset();
      #ifdef DEBUG
      logger.debug("Forced pump turning on");
      #endif
      turnOnPump();
    }
  }
}

void updatePump() {
  if (!setupMode) {
    if (!pumpRunning) {
      if (isTimeToTurnOnPump()) {
        #ifdef DEBUG
        logger.debug("Time to turn on pump");
        #endif
        turnOnPump();
      } else {
        displayTimeUntilNextWatering(pumpTurnOnTimer.getRemainingTimeMillis());
      }
    } else {
      if (isTimeToTurnOffPump()) {
        #ifdef DEBUG
        logger.debug("Time to turn off pump");
        #endif
        turnOffPump();
      } else {
        displayWateringAnimation();
      }
    }
  }
}

void updateSetupScreen() {
  if (setupMode) {
    lcd.setCursor(0, 0);
    String text;

    if (setupStep < SETUP_STEP_PUMP_TURN_OFF_INTERVAL_MINUTES) {
      // Интервал полива:
      lcd.print("\xA5\xBD\xBF\x65p\xB3\x61\xBB \xBEo\xBB\xB8\xB3\x61:");
      lcd.setCursor(0, 1);

      byte blinkingTimeUnits;
      if (setupStep == SETUP_STEP_PUMP_TURN_ON_INTERVAL_DAYS) {
        blinkingTimeUnits = B1;
      } else if (setupStep == SETUP_STEP_PUMP_TURN_ON_INTERVAL_HOURS) {
        blinkingTimeUnits = B10;
      } else {
        blinkingTimeUnits = B100;
      }

      text = timeIntervalToString(pumpTurnOnTimer.getIntervalMillis(), B111, blinkingTimeUnits);
    } else {
      // Время полива:
      lcd.print("Bpe\xBC\xC7 \xBEo\xBB\xB8\xB3\x61:   ");
      lcd.setCursor(0, 1);

      byte blinkingTimeUnits;
      if (setupStep == SETUP_STEP_PUMP_TURN_OFF_INTERVAL_MINUTES) {
        blinkingTimeUnits = B100;
      } else {
        blinkingTimeUnits = B1000;
      }

      text = timeIntervalToString(pumpRunTimer.getIntervalMillis(), B1100, blinkingTimeUnits);
    }

    lcd.print(text);
  }
}

boolean isAnyButtonPressed() {
  return setButton.isPressed() || leftButton.isPressed() || rightButton.isPressed();
}

boolean isTimeToTurnOnPump() {
  return pumpTurnOnTimer.isWentOff();
}

boolean isTimeToTurnOffPump() {
  return pumpRunTimer.isWentOff();
}

void turnOnPump() {
  if (!pumpRunning) {
    digitalWrite(PUMP_PIN, HIGH);
    pumpRunning = true;
    pumpRunTimer.reset();
    lcd.clear();
    #ifdef DEBUG
    logger.debug("Pump is turned on");
    #endif
  }
}

void turnOffPump() {
  if (pumpRunning) {
    digitalWrite(PUMP_PIN, LOW);
    pumpRunning = false;
    lcd.clear();
    #ifdef DEBUG
    logger.debug("Pump is turned off");
    #endif
  }
}

void displayTimeUntilNextWatering(unsigned long time, unsigned int blinkCount) {
  lcd.setCursor(0, 0);
  // Полив через:
  lcd.print("\xA8o\xBB\xB8\xB3 \xC0\x65p\x65\xB7:");
  String text = timeIntervalToString(time, B1111, 0);
  if (blinkCount == 0) {
    lcd.setCursor(0, 1);
    lcd.print(text);
  } else {
    for (int i = 0; i < blinkCount; i++) {
      lcd.setCursor(0, 1);
      lcd.print(text);
      delay(250);
      lcd.setCursor(0, 1);
      lcd.print("                ");
      delay(250);
    }
  }
}

void displayWateringAnimation() {
  if (wateringAnimationTimer.isWentOff()) {
    byte (*currentFrame)[8] = WATERING_ANIMATION[wateringAnimationFrameIndex++];
    if (wateringAnimationFrameIndex == WATERING_ANIMATION_LENGTH) {
      wateringAnimationFrameIndex = 0;
    }

    for (int i = 0; i < WATERING_ANIMATION_FRAME_SIZE; i++) {
      lcd.createChar(i, currentFrame[i]);
    }

    lcd.setCursor(0, 0);
    // Время
    lcd.print("Bpe\xBC\xC7");
    
    lcd.setCursor(11, 0);
    for (int i = 0; i < WATERING_ANIMATION_FRAME_SIZE / 2; i++) {
      lcd.write(i);
    }

    lcd.setCursor(0, 1);
    // поливать!
    lcd.print("\xBEo\xBB\xB8\xB3\x61\xBF\xC4! ");
    lcd.setCursor(11, 1);
    for (int i = WATERING_ANIMATION_FRAME_SIZE / 2; i < WATERING_ANIMATION_FRAME_SIZE; i++) {
      lcd.write(i);
    }
  }
}

String timeIntervalToString(unsigned long timeIntervalMillis, byte includedTimeUnits, byte blinkingTimeUnits) {
  String result = "";
  int width = 0;


  if (lcdBlinkTimer.isWentOff()) {
    lcdBlinkState = !lcdBlinkState;
  }

  unsigned long seconds = round(timeIntervalMillis / 1000.0);

  if (bitRead(includedTimeUnits, 0) == 1) {
    unsigned int days = seconds / SECONDS_IN_DAY;
    seconds %= SECONDS_IN_DAY;
    if (bitRead(blinkingTimeUnits, 0) == 1 && !lcdBlinkState) {
      result += (days > 9 ? "  " : " ");
    } else {
      result += String(days, DEC);
    }
    result += "\xE3 "; // д
    if (days < 10) {
      width += 3; 
    } else {
      width += 4;
    }
  }

  if (bitRead(includedTimeUnits, 1) == 1) {
    unsigned int hours = seconds / SECONDS_IN_HOUR;
    seconds %= SECONDS_IN_HOUR;
    if (bitRead(blinkingTimeUnits, 1) == 1 && !lcdBlinkState) {
      result += (hours > 9 ? "  " : " ");
    } else {
      result += String(hours, DEC);
    }
    result += "\xC0 "; // ч
    if (hours < 10) {
      width += 3; 
    } else {
      width += 4;
    }
  }

  if (bitRead(includedTimeUnits, 2) == 1) {
    unsigned long minutes = seconds / 60;
    seconds %= 60;
    if (bitRead(blinkingTimeUnits, 2) == 1 && !lcdBlinkState) {
      result += (minutes > 9 ? "  " : " ");
    } else {
      result += String(minutes, DEC);
    }
    result += "\xBC "; // м
    if (minutes < 10) {
      width += 3; 
    } else {
      width += 4;
    }
  }

  if (bitRead(includedTimeUnits, 3) == 1) {
    if (bitRead(blinkingTimeUnits, 3) == 1 && !lcdBlinkState) {
      result += (seconds > 9 ? "  " : " ");
    } else {
      result += String(seconds, DEC);
    }
    result += "c";
    if (seconds < 10) {
      width += 2; 
    } else {
      width += 3;
    }
  }

  for (int i = 0; i < 16 - width; i++) {
    result += " ";
  }

  return result;
}

void resetLcdBlinkTimer() {
  lcdBlinkState = true;
  lcdBlinkTimer.reset();
}

void checkEeprom() {
  unsigned long calculatedCrc = calculateEepromCrc();
  unsigned long storedCrc;
  EEPROM.get(EEPROM.length() - 4, storedCrc);
  if (storedCrc != calculatedCrc) {
    #ifdef DEBUG
    logger.debug("Stored EEPROM CRC does not match calculated EEPROM CRC");
    #endif
    // Store default values and recalculate CRC
    EEPROM.put(0, DEFAULT_PUMP_TURN_ON_INTERVAL_MILLIS);
    EEPROM.put(4, DEFAULT_PUMP_RUN_INTERVAL_MILLIS);
    updateEepromCrc();
  }
}

void storePumpTurnOnInterval() {
  EEPROM.put(0, pumpTurnOnTimer.getIntervalMillis());
  updateEepromCrc();
}

void storePumpTurnOffInterval() {
  EEPROM.put(4, pumpRunTimer.getIntervalMillis());
  updateEepromCrc();
}

void updateEepromCrc() {
  EEPROM.put(EEPROM.length() - 4, calculateEepromCrc());
}

unsigned long calculateEepromCrc() {
  // Стандартный CRC-32: начальное значение 0xFFFFFFFF, табличный расчёт по полубайтам,
  // финальная инверсия выполняется один раз после прохода по всем байтам.
  unsigned long crc = ~0L;
  for (int i = 0; i < EEPROM.length() - 4; i++) {
    crc = CRC_TABLE[(crc ^ EEPROM[i]) & 0x0F] ^ (crc >> 4);
    crc = CRC_TABLE[(crc ^ (EEPROM[i] >> 4)) & 0x0F] ^ (crc >> 4);
  }
  return ~crc;
}
