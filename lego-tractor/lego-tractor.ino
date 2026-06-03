#include <IBusBM.h>

// #define DEBUG

const byte MOTOR_A = 0;
const byte MOTOR_B = 1;

const byte MOTOR_COUNT = 2;

const int MOTOR_STATE_BRAKE   = 0;
const int MOTOR_STATE_FORWARD = 1;
const int MOTOR_STATE_REVERSE = -1;

const int DUTY_MIN_VALUE  = -255;
const int DUTY_MAX_VALUE  = 255;
const int DUTY_STOP_VALUE = 0;
const byte DUTY_ABSOLUTE_MAX_VALUE = 255;

const int CHANNEL_READ_TIMEOUT_VALUE = 0;
const int CHANNEL_MIN_VALUE = 1000;
const int CHANNEL_MAX_VALUE = 2000;
const int CHANNEL_CORRECTION = 40;
const int CHANNEL_CENTER_VALUE = CHANNEL_MIN_VALUE + (CHANNEL_MAX_VALUE - CHANNEL_MIN_VALUE) / 2;
const unsigned long CHANNEL_PULSE_TIMEOUT_US = 30000UL;

const int STICK_DEAD_ZONE_RADIUS = 15;
const int STICK_DEAD_ZONE_MIN = CHANNEL_CENTER_VALUE - STICK_DEAD_ZONE_RADIUS;
const int STICK_DEAD_ZONE_MAX = CHANNEL_CENTER_VALUE + STICK_DEAD_ZONE_RADIUS;

const byte BATTERY_STATUS_UNKNOWN    = 0;
const byte BATTERY_STATUS_OK         = 1;
const byte BATTERY_STATUS_LOW        = 2;
const byte BATTERY_STATUS_DISCHARGED = 3;

const unsigned long BATTERY_UPDATE_INTERVAL_MS = 100UL;
const byte BATTERY_READ_COUNT = 32;
const float BATTERY_VOLTAGE_DIVIDER_RATIO = 3.0;
const float BATTERY_OK_MIN_VOLTAGE = 6.5;
const float BATTERY_LOW_MIN_VOLTAGE = 6.0;
const float BATTERY_VOLTAGE_ROUND_FACTOR = 10.0;
const float BATTERY_TELEMETRY_SCALE = 100.0;
const byte BATTERY_TELEMETRY_SENSOR_INDEX = 1;
const byte BATTERY_LOW_GREEN_PWM = 128;

const float ADC_REFERENCE_VOLTAGE = 5.0;
const float ADC_RESOLUTION = 1024.0;

const float GEAR_TRAIN_COMPENSATION_BASE   = 40.0;
const float GEAR_TRAIN_COMPENSATION_FACTOR = 0.16;

const byte ENA_PIN  = 3;
const byte ENB_PIN  = 5;
const byte MC1A_PIN = 7;
const byte MC2A_PIN = 6;
const byte MC1B_PIN = 2;
const byte MC2B_PIN = 4;

const byte CH1_PIN = 10;
const byte CH2_PIN = 11;
const byte CH5_PIN = 12;

const byte LIGHTS_PIN = 13;
const byte BATTERY_SENSOR_PIN = A0;
const byte BATTERY_INDICATOR_R_PIN = 8;
const byte BATTERY_INDICATOR_G_PIN = 9;

struct MotorPins {
  byte enablePin;
  byte controlPin1;
  byte controlPin2;
  bool compensateGearTrain;
};

const MotorPins MOTORS[MOTOR_COUNT] = {
  {ENA_PIN, MC1A_PIN, MC2A_PIN, false},
  {ENB_PIN, MC1B_PIN, MC2B_PIN, true}
};

IBusBM IBus;

int motorStates[MOTOR_COUNT] = {MOTOR_STATE_BRAKE, MOTOR_STATE_BRAKE};
unsigned long batteryTimer = 0;
bool lightsOn = false;
byte batteryStatus = BATTERY_STATUS_UNKNOWN;

void updateMotors();
void updateLights();
void setLights(bool enabled);
void updateBattery(bool force);
byte getBatteryStatus(float voltage);
float readBatteryVoltage();
void setBatteryStatus(byte status);
void brakeMotors();
void driveOrBrakeMotor(byte motor, int duty);
int getMotorState(int duty);
void applyMotorState(MotorPins pins, int state);
int compensateGearTrain(int duty);
int readChannel(byte channel);
const char* getChannelName(byte channel);
int readStick(byte channel, int minValue, int maxValue);
bool readSwitch(byte channel);

void setup() {
  Serial.begin(115200);

  IBus.begin(Serial);
  IBus.addSensor(IBUSS_EXTV);

  pinMode(CH1_PIN, INPUT);
  pinMode(CH2_PIN, INPUT);
  pinMode(CH5_PIN, INPUT);
  pinMode(LIGHTS_PIN, OUTPUT);
  pinMode(BATTERY_INDICATOR_R_PIN, OUTPUT);
  pinMode(BATTERY_INDICATOR_G_PIN, OUTPUT);
  for (byte motor = MOTOR_A; motor < MOTOR_COUNT; motor++) {
    pinMode(MOTORS[motor].enablePin, OUTPUT);
    pinMode(MOTORS[motor].controlPin1, OUTPUT);
    pinMode(MOTORS[motor].controlPin2, OUTPUT);
  }

  setLights(false);
  brakeMotors();
  updateBattery(true);
}

void loop() {
  IBus.loop();

  updateBattery(false);
  updateLights();
  if (batteryStatus == BATTERY_STATUS_DISCHARGED) {
    brakeMotors();
    return;
  }
  updateMotors();

  #ifdef DEBUG
  delay(500);
  #endif
}

void updateMotors() {
  int x = readStick(CH1_PIN, DUTY_MIN_VALUE, DUTY_MAX_VALUE);
  int y = readStick(CH2_PIN, DUTY_MIN_VALUE, DUTY_MAX_VALUE);
  
  int rightDuty = constrain(y + x, DUTY_MIN_VALUE, DUTY_MAX_VALUE);
  int leftDuty = constrain(y - x, DUTY_MIN_VALUE, DUTY_MAX_VALUE);

  #ifdef DEBUG
  Serial.print("Right duty: ");
  Serial.println(rightDuty);
  Serial.print("Left duty: ");
  Serial.println(leftDuty);
  #endif

  driveOrBrakeMotor(MOTOR_A, leftDuty);
  driveOrBrakeMotor(MOTOR_B, rightDuty);
}

void updateLights() {
  bool newLightsOn = readSwitch(CH5_PIN);
  if (lightsOn != newLightsOn) {
    setLights(newLightsOn);
  }
}

void setLights(bool enabled) {
  lightsOn = enabled;
  digitalWrite(LIGHTS_PIN, lightsOn ? HIGH : LOW);
}

void updateBattery(bool force) {
  unsigned long now = millis();
  if (!force && now - batteryTimer < BATTERY_UPDATE_INTERVAL_MS) {
    return;
  }

  batteryTimer = now;
  float voltage = readBatteryVoltage();
  setBatteryStatus(getBatteryStatus(voltage));
  IBus.setSensorMeasurement(BATTERY_TELEMETRY_SENSOR_INDEX, (uint16_t) round(voltage * BATTERY_TELEMETRY_SCALE));
}


byte getBatteryStatus(float voltage) {
  if (voltage > BATTERY_OK_MIN_VOLTAGE) {
    return BATTERY_STATUS_OK;
  }
  if (voltage > BATTERY_LOW_MIN_VOLTAGE) {
    return BATTERY_STATUS_LOW;
  }
  return BATTERY_STATUS_DISCHARGED;
}

float readBatteryVoltage() {
  float sum = 0.0;
  float conversionFactor = ADC_REFERENCE_VOLTAGE / ADC_RESOLUTION * BATTERY_VOLTAGE_DIVIDER_RATIO;
  for (byte i = 0; i < BATTERY_READ_COUNT; i++) {
    sum += (float) analogRead(BATTERY_SENSOR_PIN) * conversionFactor;
  }
  return round(sum / (float) BATTERY_READ_COUNT * BATTERY_VOLTAGE_ROUND_FACTOR) / BATTERY_VOLTAGE_ROUND_FACTOR;
}

void setBatteryStatus(byte status) {
  if (batteryStatus == status) {
    return;
  }

  batteryStatus = status;
  if (status == BATTERY_STATUS_OK) {
    digitalWrite(BATTERY_INDICATOR_R_PIN, LOW);
    digitalWrite(BATTERY_INDICATOR_G_PIN, HIGH);
  } else if (status == BATTERY_STATUS_LOW) {
    digitalWrite(BATTERY_INDICATOR_R_PIN, HIGH);
    analogWrite(BATTERY_INDICATOR_G_PIN, BATTERY_LOW_GREEN_PWM);
  } else {
    digitalWrite(BATTERY_INDICATOR_R_PIN, HIGH);
    digitalWrite(BATTERY_INDICATOR_G_PIN, LOW);
  }
}

void brakeMotors() {
  for (byte motor = MOTOR_A; motor < MOTOR_COUNT; motor++) {
    driveOrBrakeMotor(motor, DUTY_STOP_VALUE);
  }
}

void driveOrBrakeMotor(byte motor, int duty) {
  if (motor >= MOTOR_COUNT) {
    return;
  }

  int constrainedDuty = constrain(duty, DUTY_MIN_VALUE, DUTY_MAX_VALUE);
  int newState = getMotorState(constrainedDuty);
  MotorPins pins = MOTORS[motor];
  if (newState != motorStates[motor]) {
    applyMotorState(pins, newState);
    motorStates[motor] = newState;
  }

  if (pins.compensateGearTrain) {
    constrainedDuty = compensateGearTrain(constrainedDuty);
  }
  analogWrite(pins.enablePin, abs(constrainedDuty));
}

int getMotorState(int duty) {
  if (duty == DUTY_STOP_VALUE) {
    return MOTOR_STATE_BRAKE;
  }
  return duty > DUTY_STOP_VALUE ? MOTOR_STATE_FORWARD : MOTOR_STATE_REVERSE;
}

void applyMotorState(MotorPins pins, int state) {
  digitalWrite(pins.enablePin, LOW);
  if (state == MOTOR_STATE_FORWARD) {
    digitalWrite(pins.controlPin1, HIGH);
    digitalWrite(pins.controlPin2, LOW);
  } else if (state == MOTOR_STATE_REVERSE) {
    digitalWrite(pins.controlPin1, LOW);
    digitalWrite(pins.controlPin2, HIGH);
  } else {
    digitalWrite(pins.controlPin1, LOW);
    digitalWrite(pins.controlPin2, LOW);
  }
}

int compensateGearTrain(int duty) {
  if (duty == DUTY_STOP_VALUE) {
    return DUTY_STOP_VALUE;
  }

  int absoluteDuty = abs(duty);
  float compensation = max(0.0, GEAR_TRAIN_COMPENSATION_BASE - (float) absoluteDuty * GEAR_TRAIN_COMPENSATION_FACTOR);
  int compensatedDuty = absoluteDuty + (int) round(compensation);
  compensatedDuty = constrain(compensatedDuty, DUTY_STOP_VALUE, DUTY_ABSOLUTE_MAX_VALUE);
  return duty > DUTY_STOP_VALUE ? compensatedDuty : -compensatedDuty;
}

int readChannel(byte channel) {
  unsigned long pulseWidth = pulseIn(channel, HIGH, CHANNEL_PULSE_TIMEOUT_US);

  #ifdef DEBUG
  Serial.print("Channel ");
  Serial.print(getChannelName(channel));
  Serial.print(": ");
  Serial.println(pulseWidth);
  #endif

  if (pulseWidth == CHANNEL_READ_TIMEOUT_VALUE) {
    return CHANNEL_READ_TIMEOUT_VALUE;
  }
  return constrain((int) pulseWidth + CHANNEL_CORRECTION, CHANNEL_MIN_VALUE, CHANNEL_MAX_VALUE);
}

const char* getChannelName(byte channel) {
  if (channel == CH1_PIN) {
    return "1";
  }
  if (channel == CH2_PIN) {
    return "2";
  }
  if (channel == CH5_PIN) {
    return "5";
  }
  return "?";
}

int readStick(byte channel, int minValue, int maxValue) {
  int value = readChannel(channel);
  if (value == CHANNEL_READ_TIMEOUT_VALUE || (value >= STICK_DEAD_ZONE_MIN && value <= STICK_DEAD_ZONE_MAX)) {
    return DUTY_STOP_VALUE;
  }
  return map(value, CHANNEL_MIN_VALUE, CHANNEL_MAX_VALUE, minValue, maxValue);
}

bool readSwitch(byte channel) {
  int value = readChannel(channel);
  if (value == CHANNEL_READ_TIMEOUT_VALUE) {
    return false;
  }
  return value > CHANNEL_CENTER_VALUE;
}
