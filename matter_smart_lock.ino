/*
    Nano Matter Servo Lock
    by C. Kalapati

    A Matter door lock for the Arduino Nano Matter that rotates a servo via 
    PWM every time the lock is triggered to be unlocked or locked. 3 buttons 
    are used as input for a passcode for locking and unlocking.

    Derived from default Matter Door Lock code by Tamas Jozsi (Silicon Labs)
*/

#include <Matter.h>
#include <MatterDoorLock.h>
#include <vector>

#ifndef BTN_BUILTIN
#define BTN_BUILTIN PA0
#endif

/*-- Variables (to be modified to your use case) --*/

std::vector<int> passcode = {1, 1, 3, 2}; // Passcode to unlock door
const long inputWaitTime = 5000; // Time before clearing the buffer after last input (milliseconds)

const int servoPin = 9;
const int button1Pin = 6;
const int button2Pin = 7;
const int button3Pin = 8;
const int ledRedPin = 5;
const int ledGreenPin = 4;
const int ledBluePin = 3;

/*-------------------------------------------------*/

const int minPulseWidth = 500; // Min pulse width in microseconds
const int maxPulseWidth = 2500; // Max pulse width in microseconds
const int servoMinAngle = 0;
const int servoMaxAngle = 180;

volatile bool button_pressed = false;
volatile bool button1_pressed = false;
volatile bool button2_pressed = false;
volatile bool button3_pressed = false;

unsigned long previousMicros = 0; // Store time in microseconds
unsigned long pulseDuration = 0;
unsigned long period = 20000;    // Total PWM period in microseconds (20 ms)
int servoState = LOW;

unsigned long previousMillis = 0; // Store time in milliseconds
int previousBufferSize = 0;

MatterDoorLock matter_door_lock;
std::vector<int> buffer;

void set_servo_angle(int);
void handle_button_press();
void handle_passcode_input(int);
void handle_button_press3();
void handle_button_press2();
void handle_button_press1();

void setup()
{
  Serial.begin(115200);
  Matter.begin();
  matter_door_lock.begin();

  pinMode(servoPin, OUTPUT);
  pinMode(ledRedPin, OUTPUT);
  pinMode(ledBluePin, OUTPUT);
  pinMode(ledGreenPin, OUTPUT);

  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);
  pinMode(button3Pin, INPUT_PULLUP);

  pinMode(BTN_BUILTIN, INPUT_PULLUP);
  attachInterrupt(BTN_BUILTIN, &handle_button_press, FALLING);
  attachInterrupt(button1Pin, &handle_button_press1, FALLING);
  attachInterrupt(button2Pin, &handle_button_press2, FALLING);
  attachInterrupt(button3Pin, &handle_button_press3, FALLING);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LED_BUILTIN_INACTIVE);

  Serial.println("Matter door lock");

  // Pairing information for the door lock with Matter devices
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("Matter device is not commissioned");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\n", Matter.getOnboardingQRCodeUrl().c_str());
  }

  Serial.printf("Manual pairing code: %s\n", Matter.getManualPairingCode().c_str());
  Serial.printf("QR code URL: %s\n", Matter.getOnboardingQRCodeUrl().c_str());

  // Connect to the Matter and Thread network before starting
  while (!Matter.isDeviceCommissioned()) {
    delay(200);
  }

  Serial.println("Waiting for Thread network...");
  while (!Matter.isDeviceThreadConnected()) {
    delay(200);
  }
  Serial.println("Connected to Thread network");

  Serial.println("Waiting for Matter device discovery...");
  while (!matter_door_lock.is_online()) {

    // Cycle through a loading animation with the Blue LED
    for (int i = 0; i < 100; i++) {
      analogWrite(ledBluePin, i);
      delay(5);
    }
    for (int i = 100; i >= 0; i--) {
      analogWrite(ledBluePin, i);
      delay(5);
    }
    delay(100);
  }
  Serial.println("Matter device is now online");
}

void loop()
{

  bool locked = matter_door_lock.is_locked();

  if (buffer.size() == 4) {
    if (buffer == passcode) {
      matter_door_lock.toggle();
    } else {

      // Lock the door if it is unlocked after incorrect input
      if (!locked) {
        matter_door_lock.toggle();
      }
    }
    buffer.clear();
  } else if (buffer.size() != 0) {

    unsigned long currentMillis = millis();

    // Check to see if the inputWaitTime time has elapsed
    if (currentMillis - previousMillis > inputWaitTime) {
      buffer.clear();
    }
  }

  static bool locked_last = false;

  if (locked != locked_last) {
    locked_last = locked;
    if (locked) {
      Serial.println("Locked");
      analogWrite(ledRedPin, 100);
      analogWrite(ledGreenPin, 0);
      digitalWrite(ledBluePin, LOW);
      set_servo_angle(0);
    } else {
      Serial.println("Unlocked");
      analogWrite(ledRedPin, 0);
      analogWrite(ledGreenPin, 50);
      digitalWrite(ledBluePin, LOW);
      set_servo_angle(80);
    }
  }


  // lock door if built in button is pressed
  if (button_pressed) {
    button_pressed = false;
    matter_door_lock.toggle();
  }
}

void handle_button_press()
{

  static uint32_t btn_last_press = 0;
  if (millis() < btn_last_press + 200) {
    return;
  }
  btn_last_press = millis();
  button_pressed = true;
  
}

void handle_button_press1()
{
  Serial.println("Button 1 Pressed");
  static uint32_t btn_last_press = 0;
  if (millis() < btn_last_press + 200) {
    return;
  }
  btn_last_press = millis();
  handle_passcode_input(1);
}

void handle_button_press2()
{
  Serial.println("Button 2 Pressed");
  static uint32_t btn_last_press = 0;
  if (millis() < btn_last_press + 200) {
    return;
  }
  btn_last_press = millis();
  handle_passcode_input(2);
}

void handle_button_press3()
{
  Serial.println("Button 3 Pressed");
  static uint32_t btn_last_press = 0;
  if (millis() < btn_last_press + 200) {
    return;
  }
  btn_last_press = millis();
  handle_passcode_input(3);
}

void set_servo_angle(int angle) {

  if (angle < servoMinAngle) {
    angle = servoMinAngle;
  }

  int pulseWidth = map(angle, servoMinAngle, 180, minPulseWidth, maxPulseWidth);

  for (int i = 0; i < 20; i++) {
    digitalWrite(servoPin, HIGH);
    delayMicroseconds(pulseWidth);
    digitalWrite(servoPin, LOW);
    delayMicroseconds(20000 - pulseWidth);
  }
}

void handle_passcode_input(int number) {

  previousMillis = millis();
  buffer.push_back(number);

}