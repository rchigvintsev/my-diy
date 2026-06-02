/// @file automatic-watering.ino
/// @brief Прошивка для автоматического полива растений на Arduino Nano.
///
/// Описание устройства:
///  - таймер периодически включает помпу для полива на заданный интервал;
///  - режим настройки позволяет редактировать интервал между поливами
///    (дни / часы / минуты) и длительность полива (минуты / секунды);
///  - значения параметров сохраняются в EEPROM и защищены CRC-32;
///  - на LCD 16x2 выводится время до следующего полива, во время полива —
///    анимация лейки;
///  - три тактовые кнопки (SET / LEFT / RIGHT) с поддержкой клика и удержания;
///  - подсветка LCD автоматически гаснет через таймаут бездействия.
///
/// Подключение:
///  - LCD 16x2 — по I²C (адрес 0x3F);
///  - транзистор помпы — PUMP_PIN;
///  - кнопки — BUTTON_SET_PIN, BUTTON_LEFT_PIN, BUTTON_RIGHT_PIN
///    (INPUT_PULLUP, активные при замыкании на GND);
///  - опрос кнопок выполняется по прерыванию Timer1 каждые 10 мс.
///
/// @author Roman Chigvintsev

#include <LiquidCrystal_I2C.h>
#include <TimerOne.h>
#include <ArduLog.h>
#include <ArduButton.h>
#include <ArduTimer.h>
#include <EEPROM.h>

/// Включает Serial-логирование. Закомментировать для production-сборки.
#define DEBUG

/// Максимальное значение типа unsigned long (для контроля переполнения).
#define UNSIGNED_LONG_MAX_VALUE 4294967295UL

#define MILLIS_IN_DAY    86400000
#define MILLIS_IN_HOUR   3600000
#define MILLIS_IN_MINUTE 60000
#define MILLIS_IN_SECOND 1000

#define SECONDS_IN_DAY    86400
#define SECONDS_IN_HOUR   3600
#define SECONDS_IN_MINUTE 60

// Адреса параметров в EEPROM.
#define EEPROM_ADDR_PUMP_TURN_ON_INTERVAL 0
#define EEPROM_ADDR_PUMP_RUN_INTERVAL     4
#define EEPROM_ADDR_CRC                   (EEPROM.length() - 4)

#define DEFAULT_PUMP_TURN_ON_INTERVAL_MILLIS 259200000UL // 3 дня
#define DEFAULT_PUMP_RUN_INTERVAL_MILLIS     30000UL

#define WATERING_ANIMATION_LENGTH                  2
#define WATERING_ANIMATION_FRAME_SIZE              8
#define WATERING_ANIMATION_FRAME_INTERVAL_MILLIS 500

#define LCD_BACKLIGHT_TIMEOUT_MILLIS 10000
#define LCD_BLINK_INTERVAL_MILLIS      500

/// Полупериод мигания времени при сбросе таймера полива удержанием левой кнопки, мс.
#define WATERING_TIME_BLINK_HALF_PERIOD_MILLIS 250

// Шаги мастера настройки. Порядок шагов задаётся возрастанием значений.
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

/// Кадры анимации «Лейка».
///
/// Структура: [номер_кадра][индекс_символа_8x5][строка_5x1].
/// Каждый кадр состоит из 8 пользовательских символов LCD, расположенных
/// в две строки по 4 символа.
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

/// Таблица для вычисления CRC-32 по полубайтам.
///
/// Полином 0xEDB88320 (зеркальный CRC-32, IEEE 802.3).
const unsigned long CRC_TABLE[16] = {
  0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
  0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
  0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
  0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
};

// Таймеры устройства.
ArduTimer pumpTurnOnTimer(DEFAULT_PUMP_TURN_ON_INTERVAL_MILLIS);
ArduTimer pumpRunTimer(DEFAULT_PUMP_RUN_INTERVAL_MILLIS);
ArduTimer wateringAnimationTimer(WATERING_ANIMATION_FRAME_INTERVAL_MILLIS);
ArduTimer lcdBacklightTimer(LCD_BACKLIGHT_TIMEOUT_MILLIS);
ArduTimer lcdBlinkTimer(LCD_BLINK_INTERVAL_MILLIS);

// Кнопки управления.
ArduButton setButton(BUTTON_SET_PIN);
ArduButton leftButton(BUTTON_LEFT_PIN);
ArduButton rightButton(BUTTON_RIGHT_PIN);

LiquidCrystal_I2C lcd(0x3F, 16, 2);
boolean lcdBacklightEnabled = true;
boolean lcdBlinkState = true;

boolean pumpRunning = false;
byte wateringAnimationFrameIndex = 0;

// Режим настройки и текущий шаг мастера.
boolean setupMode = false;
byte setupStep = SETUP_STEP_PUMP_TURN_ON_INTERVAL_DAYS;

// Флаги, указывающие на то, что удержание кнопки уже было обработано.
// Защищают от повторных срабатываний на одно и то же физическое удержание.
boolean setButtonHeld = false;
boolean leftButtonHeld = false;
boolean rightButtonHeld = false;

void displayTimeUntilNextWatering(unsigned long time, unsigned int blinkCount = 0);

/// Точка входа: инициализирует периферию и восстанавливает параметры из EEPROM.
void setup() {
  #ifdef DEBUG
  Serial.begin(9600);
  #endif

  pinMode(PUMP_PIN, OUTPUT);
  pinMode(BUTTON_SET_PIN, INPUT_PULLUP);
  pinMode(BUTTON_LEFT_PIN, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT_PIN, INPUT_PULLUP);

  // Опрос кнопок по прерыванию Timer1 каждые 10 мс независимо от блокировок в loop().
  Timer1.initialize(10000);
  Timer1.attachInterrupt(updateButtons);

  lcd.init();
  lcd.backlight();

  checkEeprom();

  unsigned long pumpTurnOnInterval;
  EEPROM.get(EEPROM_ADDR_PUMP_TURN_ON_INTERVAL, pumpTurnOnInterval);
  // Защита от мусора в EEPROM: интервал включения 1 минута .. 30 суток.
  if (pumpTurnOnInterval < MILLIS_IN_MINUTE || pumpTurnOnInterval > 30UL * MILLIS_IN_DAY) {
    pumpTurnOnInterval = DEFAULT_PUMP_TURN_ON_INTERVAL_MILLIS;
  }
  #ifdef DEBUG
  logger.debug("Stored pump turn on interval (ms): " + String(pumpTurnOnInterval));
  #endif
  pumpTurnOnTimer.setIntervalMillis(pumpTurnOnInterval);

  unsigned long pumpRunInterval;
  EEPROM.get(EEPROM_ADDR_PUMP_RUN_INTERVAL, pumpRunInterval);
  // Защита от мусора в EEPROM: интервал работы помпы 1 секунда .. 1 час.
  if (pumpRunInterval < MILLIS_IN_SECOND || pumpRunInterval > MILLIS_IN_HOUR) {
    pumpRunInterval = DEFAULT_PUMP_RUN_INTERVAL_MILLIS;
  }
  #ifdef DEBUG
  logger.debug("Stored pump run interval (ms): " + String(pumpRunInterval));
  #endif
  pumpRunTimer.setIntervalMillis(pumpRunInterval);
}

/// Главный цикл прошивки: обрабатывает ввод, обновляет помпу и экран.
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

/// Обработчик прерывания Timer1: опрашивает физическое состояние кнопок.
///
/// Вызывается каждые 10 мс независимо от блокировок в loop(), что обеспечивает
/// корректное распознавание клика и удержания.
void updateButtons() {
  setButton.update();
  leftButton.update();
  rightButton.update();
}

/// Управляет подсветкой LCD: гасит её после таймаута бездействия и включает
/// при нажатии любой кнопки.
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

/// Клик кнопки SET в режиме настройки: переход к следующему шагу мастера.
///
/// После последнего шага мастер автоматически закрывается, а параметры
/// сохраняются в EEPROM.
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

/// Удержание кнопки SET: открывает или закрывает режим настройки.
///
/// При выходе из режима настройки параметры сохраняются в EEPROM.
/// Действие запрещено, если помпа в данный момент работает.
void handleSetButtonHold() {
  if (setButtonHeld) {
    setButtonHeld = !setButton.isReleased();
  }

  if (!pumpRunning && !setButtonHeld && setButton.isHeld()) {
    setButtonHeld = true;
    lcd.clear();
    // Сбрасываем предыдущие взаимодействия с кнопками, чтобы вход/выход
    // из режима настройки не подхватил случайные клики или удержания.
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

/// Клик левой кнопки в режиме настройки: уменьшает значение в текущем поле.
///
/// Применяет минимально допустимые ограничения: интервал полива не может стать
/// равным нулю (минимум — 1 минута), длительность полива — минимум 1 секунда.
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
          unsigned long hours = interval % MILLIS_IN_DAY / MILLIS_IN_HOUR;
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
      } else if (interval >= MILLIS_IN_SECOND) {
        unsigned long seconds = interval % MILLIS_IN_MINUTE / MILLIS_IN_SECOND;
        if (seconds > 0) {
          interval -= MILLIS_IN_SECOND;
        }
      }

      if (interval == 0) {
        interval = MILLIS_IN_SECOND;
      }
      pumpRunTimer.setIntervalMillis(interval);
    }
  }
}

/// Удержание левой кнопки: сбрасывает таймер до следующего полива.
///
/// На экране трижды мигает обновлённое время до полива — обратная связь
/// пользователю. Действие запрещено в режиме настройки и при работающей помпе.
void handleLeftButtonHold() {
  if (!setupMode) {
    if (leftButtonHeld) {
      leftButtonHeld = !leftButton.isReleased();
    }

    if (!pumpRunning && !leftButtonHeld && leftButton.isHeld()) {
      leftButtonHeld = true;
      pumpTurnOnTimer.reset();
      displayTimeUntilNextWatering(pumpTurnOnTimer.getRemainingTimeMillis(), 3);
      // Сбрасываем таймер ещё раз: пока мигал LCD, время шло вперёд.
      pumpTurnOnTimer.reset();
      #ifdef DEBUG
      logger.debug("Pump turn on timer is reset");
      #endif
    }
  }
}

/// Клик правой кнопки в режиме настройки: увеличивает значение в текущем поле.
///
/// Применяет ограничения: часы 0..23, минуты и секунды 0..59. Дополнительно
/// проверяется отсутствие переполнения unsigned long.
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
          unsigned long hours = interval % MILLIS_IN_DAY / MILLIS_IN_HOUR;
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
      } else if (UNSIGNED_LONG_MAX_VALUE - interval >= MILLIS_IN_SECOND) {
        unsigned long seconds = interval % MILLIS_IN_MINUTE / MILLIS_IN_SECOND;
        if (seconds < 59) {
          interval += MILLIS_IN_SECOND;
        }
      }
      pumpRunTimer.setIntervalMillis(interval);
    }
  }
}

/// Удержание правой кнопки: принудительно включает помпу и сбрасывает таймер полива.
///
/// Используется для ручного запуска полива. Действие запрещено в режиме
/// настройки и при уже работающей помпе.
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

/// Основная логика помпы: включает по таймеру, выключает по таймеру,
/// обновляет соответствующий экран.
///
/// В режиме настройки экран не обновляется — управление отдано мастеру.
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

/// Отрисовывает экран мастера настройки.
///
/// В зависимости от шага мастера показывает интервал между поливами либо
/// длительность полива. Поле текущего шага мигает.
void updateSetupScreen() {
  if (setupMode) {
    lcd.setCursor(0, 0);
    String text;

    if (setupStep < SETUP_STEP_PUMP_TURN_OFF_INTERVAL_MINUTES) {
      // «Интервал полива:»
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
      // «Время полива:»
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

/// Возвращает true, если хотя бы одна кнопка нажата в данный момент.
boolean isAnyButtonPressed() {
  return setButton.isPressed() || leftButton.isPressed() || rightButton.isPressed();
}

/// Возвращает true, если истёк интервал между поливами.
boolean isTimeToTurnOnPump() {
  return pumpTurnOnTimer.isWentOff();
}

/// Возвращает true, если истекла длительность работы помпы.
boolean isTimeToTurnOffPump() {
  return pumpRunTimer.isWentOff();
}

/// Включает помпу, сбрасывает таймер длительности полива и очищает экран.
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

/// Выключает помпу и очищает экран.
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

/// Выводит на LCD время до следующего полива.
///
/// При \p blinkCount > 0 функция блокирует loop() на \p blinkCount × 500 мс,
/// однако обслуживание кнопок (по прерыванию Timer1) и подсветки LCD
/// продолжается.
///
/// \param time       Оставшееся время до полива в миллисекундах.
/// \param blinkCount Число миганий значения времени; 0 означает «не мигать».
void displayTimeUntilNextWatering(unsigned long time, unsigned int blinkCount) {
  lcd.setCursor(0, 0);
  // «Полив через:»
  lcd.print("\xA8o\xBB\xB8\xB3 \xC0\x65p\x65\xB7:");
  String text = timeIntervalToString(time, B1111, 0);
  if (blinkCount == 0) {
    lcd.setCursor(0, 1);
    lcd.print(text);
    return;
  }

  // Неблокирующее мигание: во время ожидания продолжаем обслуживать подсветку.
  // Прерывание Timer1 продолжает обновлять состояния кнопок независимо.
  for (unsigned int i = 0; i < blinkCount; i++) {
    lcd.setCursor(0, 1);
    lcd.print(text);
    waitNonBlocking(WATERING_TIME_BLINK_HALF_PERIOD_MILLIS);
    lcd.setCursor(0, 1);
    lcd.print("                ");
    waitNonBlocking(WATERING_TIME_BLINK_HALF_PERIOD_MILLIS);
  }
}

/// Неблокирующее ожидание заданной длительности.
///
/// Во время ожидания продолжает обслуживаться подсветка LCD (updateLcdBacklight),
/// а опрос кнопок продолжается из прерывания Timer1 параллельно.
///
/// \param durationMillis Длительность ожидания в миллисекундах.
void waitNonBlocking(unsigned long durationMillis) {
  unsigned long start = millis();
  while (millis() - start < durationMillis) {
    updateLcdBacklight();
  }
}

/// Отрисовывает анимацию лейки во время полива.
///
/// Кадры сменяются с интервалом WATERING_ANIMATION_FRAME_INTERVAL_MILLIS.
/// Используются 8 пользовательских символов LCD (createChar).
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
    // «Время»
    lcd.print("Bpe\xBC\xC7");

    lcd.setCursor(11, 0);
    for (int i = 0; i < WATERING_ANIMATION_FRAME_SIZE / 2; i++) {
      lcd.write(i);
    }

    lcd.setCursor(0, 1);
    // «поливать!»
    lcd.print("\xBEo\xBB\xB8\xB3\x61\xBF\xC4! ");
    lcd.setCursor(11, 1);
    for (int i = WATERING_ANIMATION_FRAME_SIZE / 2; i < WATERING_ANIMATION_FRAME_SIZE; i++) {
      lcd.write(i);
    }
  }
}

/// Форматирует интервал времени в строку длиной 16 символов для LCD.
///
/// Мигание единиц синхронизировано с lcdBlinkTimer (полупериод 500 мс).
///
/// \param timeIntervalMillis Интервал в миллисекундах.
/// \param includedTimeUnits  Битовая маска включаемых единиц: бит 0 — дни,
///                           бит 1 — часы, бит 2 — минуты, бит 3 — секунды.
/// \param blinkingTimeUnits  Битовая маска мигающих единиц (та же раскладка битов).
/// \return Строка длиной ровно 16 символов с дополнением пробелами справа.
String timeIntervalToString(unsigned long timeIntervalMillis, byte includedTimeUnits, byte blinkingTimeUnits) {
  String result = "";
  int width = 0;

  if (lcdBlinkTimer.isWentOff()) {
    lcdBlinkState = !lcdBlinkState;
  }

  unsigned long seconds = round(timeIntervalMillis / (float) MILLIS_IN_SECOND);

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
    unsigned long minutes = seconds / SECONDS_IN_MINUTE;
    seconds %= SECONDS_IN_MINUTE;
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

/// Сбрасывает таймер мигания LCD и форсирует «видимое» состояние.
///
/// Применяется при действиях пользователя, чтобы свежее значение в редактируемом
/// поле гарантированно было видно (не оказалось в фазе «спрятано»).
void resetLcdBlinkTimer() {
  lcdBlinkState = true;
  lcdBlinkTimer.reset();
}

/// Проверяет целостность EEPROM по CRC-32.
///
/// Если контрольная сумма не совпадает, в EEPROM записываются значения
/// по умолчанию и CRC пересчитывается. Это защищает от случайного «мусора»
/// при первом запуске или повреждении EEPROM.
void checkEeprom() {
  unsigned long calculatedCrc = calculateEepromCrc();
  unsigned long storedCrc;
  EEPROM.get(EEPROM_ADDR_CRC, storedCrc);
  if (storedCrc != calculatedCrc) {
    #ifdef DEBUG
    logger.debug("Stored EEPROM CRC does not match calculated EEPROM CRC");
    #endif
    // Записываем значения по умолчанию и пересчитываем CRC.
    EEPROM.put(EEPROM_ADDR_PUMP_TURN_ON_INTERVAL, DEFAULT_PUMP_TURN_ON_INTERVAL_MILLIS);
    EEPROM.put(EEPROM_ADDR_PUMP_RUN_INTERVAL, DEFAULT_PUMP_RUN_INTERVAL_MILLIS);
    updateEepromCrc();
  }
}

/// Сохраняет текущий интервал включения помпы в EEPROM и обновляет CRC.
void storePumpTurnOnInterval() {
  EEPROM.put(EEPROM_ADDR_PUMP_TURN_ON_INTERVAL, pumpTurnOnTimer.getIntervalMillis());
  updateEepromCrc();
}

/// Сохраняет текущую длительность работы помпы в EEPROM и обновляет CRC.
void storePumpTurnOffInterval() {
  EEPROM.put(EEPROM_ADDR_PUMP_RUN_INTERVAL, pumpRunTimer.getIntervalMillis());
  updateEepromCrc();
}

/// Пересчитывает и сохраняет контрольную сумму EEPROM.
void updateEepromCrc() {
  EEPROM.put(EEPROM_ADDR_CRC, calculateEepromCrc());
}

/// Вычисляет CRC-32 по всем байтам EEPROM, кроме области самой контрольной суммы.
///
/// Стандартный CRC-32: начальное значение 0xFFFFFFFF, табличный расчёт
/// по полубайтам, финальная инверсия выполняется один раз после прохода
/// по всем байтам.
///
/// \return 32-битная контрольная сумма EEPROM.
unsigned long calculateEepromCrc() {
  unsigned long crc = ~0L;
  for (int i = 0; i < EEPROM_ADDR_CRC; i++) {
    crc = CRC_TABLE[(crc ^ EEPROM[i]) & 0x0F] ^ (crc >> 4);
    crc = CRC_TABLE[(crc ^ (EEPROM[i] >> 4)) & 0x0F] ^ (crc >> 4);
  }
  return ~crc;
}
