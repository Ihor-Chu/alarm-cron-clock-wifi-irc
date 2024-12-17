// Using the IRremote library: https://github.com/Arduino-IRremote/Arduino-IRremote
#include <IRremote.hpp>
#include "PinDefinitionsAndMore.h" // Define macros for input and output pin etc.
//#define IR_SEND_PIN 3

void setup() {
  Serial.begin(115200);
  pinMode(5, INPUT_PULLUP); // SW1 connected to pin 2
  pinMode(6, INPUT_PULLUP); // SW2 connected to pin 3
  pinMode(7, INPUT_PULLUP); // SW3 connected to pin 4
  // The IR LED is connected to pin 3 (PWM ~) on the Arduino
  IrSender.begin(IR_SEND_PIN);
}

void loop() {
//     Protocol=NEC Address=0x1 Command=0x12 Raw-Data=0xED12FE01 32 bits LSB first

// Send with: IrSender.sendNEC(0x1, 0x12, <numberOfRepeats>);
  
  if (digitalRead(5) == LOW) { // When SW1 is pressed
    IrSender.sendNEC(0x1, 0x12, 32);  // Replace with your own unique code
    Serial.println("Code sent!");
    delay(30);
  } ;


  delay(100);
}