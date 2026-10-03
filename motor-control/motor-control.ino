/// @file motor-control.ino
/// @brief Управление двумя коллекторными моторами через модуль TB6612FNG на Arduino Uno/Nano.
///
/// Потенциометр задаёт общее направление и скорость обоих моторов: в центре — короткое торможение, по сторонам —
/// вращение в разные стороны. Показания A0 опрашиваются каждые 100 мс; ШИМ меняется от 0 до 255. Внешние библиотеки
/// не требуются.
///
/// Модуль TB6612FNG с двумя рядами по 8 контактов, вид сверху: надпись TB6612FNG читается прямо; слева контакты 1–8
/// сверху вниз, справа — 16–9 сверху вниз.
///
///                +------------------------+
///  VM      1  ---|                        |--- 16  PWMA
///  VCC     2  ---|                        |--- 15  AIN2
///  GND     3  ---|       TB6612FNG        |--- 14  AIN1
///  A1      4  ---|                        |--- 13  STBY
///  A2      5  ---|                        |--- 12  BIN1
///  B1      6  ---|                        |--- 11  BIN2
///  B2      7  ---|                        |--- 10  PWMB
///  GND     8  ---|                        |---  9  GND
///                +------------------------+
///
/// Подключение Arduino -> контакты модуля:
///   D9 -> PWMA (16), D2 -> AIN1 (14), D3 -> AIN2 (15);
///   левый мотор -> A1 (4), A2 (5).
///   D10 -> PWMB (10), D4 -> BIN1 (12), D5 -> BIN2 (11);
///   правый мотор -> B1 (6), B2 (7).
///   A0 -> средний контакт потенциометра; крайние -> 5V и GND.
///   5V -> VCC (2) и STBY (13); STBY должен быть HIGH: скетч им не управляет.
///   Отдельный источник питания моторов -> VM (1); его минус, GND Arduino и GND модуля (3, 8, 9) соедините вместе.
///
/// Это расположение относится к указанному 16-контактному модулю, а не к микросхеме SSOP24. У другого модуля сверяйте
/// маркировку.
/// Для микросхемы рабочие напряжения VCC 2,7–5,5 В и VM 4,5–13,5 В; учитывайте допустимый ток каналов и пусковой ток
/// моторов. Не питайте моторы от цифровых выводов Arduino.
const int PIN_MOTOR_LEFT_PWM   = 9;
const int PIN_MOTOR_LEFT_CTRL1 = 2;
const int PIN_MOTOR_LEFT_CTRL2 = 3;

const int PIN_MOTOR_RIGHT_PWM   = 10;
const int PIN_MOTOR_RIGHT_CTRL1 = 4;
const int PIN_MOTOR_RIGHT_CTRL2 = 5;

const int PIN_POT = 0;

/// Рабочий диапазон потенциометра и расчёт центра; мёртвая зона ±50 отсчётов АЦП.
const int POT_LOWER_BOUND = 5;
const int POT_UPPER_BOUND = 980;
const int POT_MIDDLE      = (POT_UPPER_BOUND - POT_LOWER_BOUND) / 2;

class Motor {
  private:
    int pwmPin;
    int ctrl1Pin;
    int ctrl2Pin;

  public:
    Motor(int pwmPin, int ctrl1Pin, int ctrl2Pin) {
      this->pwmPin = pwmPin;
      this->ctrl1Pin = ctrl1Pin;
      this->ctrl2Pin = ctrl2Pin;

      pinMode(pwmPin, OUTPUT);
      pinMode(ctrl1Pin, OUTPUT);
      pinMode(ctrl2Pin, OUTPUT);
    }

    /// Свободный ход: оба входа направления и ШИМ в LOW.
    void stop() {
      digitalWrite(this->pwmPin, LOW);
      digitalWrite(this->ctrl1Pin, LOW);
      digitalWrite(this->ctrl2Pin, LOW);
    }

    /// Короткое торможение: оба входа направления и ШИМ в HIGH.
    void brake() {
      digitalWrite(this->pwmPin, LOW);
      digitalWrite(this->ctrl1Pin, HIGH);
      digitalWrite(this->ctrl2Pin, HIGH);
      digitalWrite(this->pwmPin, HIGH);
    }

    /// Направление IN1=HIGH, IN2=LOW; скорость ограничена диапазоном ШИМ 0–255.
    void forward(int pwm) {
      digitalWrite(this->pwmPin, LOW);
      digitalWrite(this->ctrl1Pin, HIGH);
      digitalWrite(this->ctrl2Pin, LOW);
      analogWrite(this->pwmPin, constrain(pwm, 0, 255));
    }

    /// Направление IN1=LOW, IN2=HIGH; скорость ограничена диапазоном ШИМ 0–255.
    void reverse(int pwm) {
      digitalWrite(this->pwmPin, LOW);
      digitalWrite(this->ctrl1Pin, LOW);
      digitalWrite(this->ctrl2Pin, HIGH);
      analogWrite(this->pwmPin, constrain(pwm, 0, 255));
    }
};

Motor leftMotor(PIN_MOTOR_LEFT_PWM, PIN_MOTOR_LEFT_CTRL1, PIN_MOTOR_LEFT_CTRL2);
Motor rightMotor(PIN_MOTOR_RIGHT_PWM, PIN_MOTOR_RIGHT_CTRL1, PIN_MOTOR_RIGHT_CTRL2);

void setup() {
  leftMotor.brake();
  rightMotor.brake();
}

void loop() {
  int val = analogRead(PIN_POT);
  if (val > (POT_MIDDLE + 50)) {
    int pwm = map(val, POT_MIDDLE + 50, POT_UPPER_BOUND, 0, 255);
    leftMotor.forward(pwm);
    rightMotor.forward(pwm);
  } else if (val < (POT_MIDDLE - 50)) {
    int pwm = map(val, POT_MIDDLE - 50, POT_LOWER_BOUND, 0, 255);
    leftMotor.reverse(pwm);
    rightMotor.reverse(pwm);
  } else {
    leftMotor.brake();
    rightMotor.brake();
  }
  delay(100);
}
