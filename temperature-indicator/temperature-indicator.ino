/// @file temperature-indicator.ino
/// @brief Индикатор окружающей температуры на трёхцветном светодиоде.
///
/// Состав схемы:
///  - Arduino Uno в качестве контроллера;
///  - аналоговый датчик температуры TMP36GSZ;
///  - трёхцветный светодиод с общим катодом;
///  - токоограничивающие резисторы 220 Ом (3 шт.)
///
/// Подключение:
///   Вывод VOUT датчика подключён к @ref PIN_TMP, его VCC — к 5V, GND — к GND. Общий катод светодиода подключён к GND,
///   а аноды красного, зелёного и синего цветов через резисторы — соответственно к @ref PIN_LED_RED, @ref PIN_LED_GREEN
///   и @ref PIN_LED_BLUE.
///
/// Описание работы:
///   При температуре @ref TMP_LOW °C и ниже светодиод светится синим. В диапазоне от @ref TMP_LOW до @ref TMP_HI °C
///   цвет плавно меняется от синего к красному.
///
/// @author Roman Chigvintsev

const int PIN_LED_RED   = 9;
const int PIN_LED_GREEN = 10;
const int PIN_LED_BLUE  = 11;

const int PIN_TMP = A0;

// Число отсчётов АЦП для усреднения и подавления шума датчика.
const byte ADC_SAMPLES = 16;

// Низкая температура, которой соответствует синий цвет светодиода.
const int TMP_LOW    = 25;
// Высокая температура, которой соответствует красный цвет светодиода.
const int TMP_HI     = 30;
// Коррекция показаний датчика.
const float TMP_CORR = -1.7;

// Сглаженная температура для плавного перехода между цветами.
float filteredTemperature = TMP_LOW;

void setup() {
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);

  Serial.begin(9600);
}

void loop() {
  float voltage = readVoltage();
  float temperature = filterTemperature(voltageToTemperature(voltage));
  int pwm = temperatureToPwm(temperature);

  int red = pwm; // Для красной составляющей зависимость прямая.
  int blue = 255 - pwm; // Для синей составляющей зависимость обратная.
  setColor(red, 0, blue);

  delay(100);
}

float readVoltage() {
  // Усредняем несколько отсчётов АЦП.
  unsigned long sum = 0;
  for (byte sample = 0; sample < ADC_SAMPLES; sample++) {
    sum += analogRead(PIN_TMP);
  }
  float avg = static_cast<float>(sum) / ADC_SAMPLES;

  // Преобразуем результат в напряжение.
  return avg * 5.0 / 1023.0f;
}

float voltageToTemperature(float voltage) {
  // Преобразуем считанное напряжение в температуру в °C, дополнительно применяя коррекцию показаний датчика. Датчик
  // TMP36GSZ выдаёт 10 мВ на каждый °C, при этом при 0°C на выходе будет примерно 500 мВ.
  return (voltage - 0.5) * 100.0 + TMP_CORR;
}

float filterTemperature(float temperature) {
  // Применяем экспоненциальное сглаживание, чтобы переход от одного цвета к другому был плавным. При таком подходе 90%
  // остаётся от предыдущего сглаженного значения и 10% берётся от нового измерения.
  return (filteredTemperature = filteredTemperature * 0.9 + temperature * 0.1);
}

int temperatureToPwm(float temperature) {
  // Преобразуем считанную температуру в диапазон ШИМ от 0 до 255.
  int pwm = (temperature - TMP_LOW) * 255.0 / (TMP_HI - TMP_LOW);
  return constrain(pwm, 0, 255);
}

void setColor(int red, int green, int blue) {
  analogWrite(PIN_LED_RED, red);
  analogWrite(PIN_LED_GREEN, green);
  analogWrite(PIN_LED_BLUE, blue);
}

void printTemperature(float temperature) {
  Serial.print(temperature);
  Serial.println("°C");
}
