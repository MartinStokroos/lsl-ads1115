/*
   Dummy sampler sketch

   This sketch generates a 1 Hz sine wave on channel 1 and
   and a 6Hz sine wave on channel 2.
   The samples are sent in 16-bit samples at a 100Hz rate.

   By: M. Stokroos

   Date: February 24th, 2026


   Output package byte format: < START | COUNT | CH1_LSB | CH1_MSB | CH2_LSB | CH2_MSB | CRC >


   License: MIT License

   Copyright (c) M.Stokroos 2026
   

   Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,
   modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:
   The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
   COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
   ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "NativeDDS.h"

#define T_LOOP_US 200    // main loop duration in us.
#define PKG_SIZE 7

const int rdyPin = 2; // Interrupt pin (must support interrupt)
const int ledPin = 13; // LED pin
const int debugPin = 3;

unsigned long nextLoop;
unsigned int div10ms = 0;
unsigned int div500ms = 0;
unsigned char pkg_cnt = 0;

double fToneOne = 1.0; // output frequency in Hz
double fToneTwo = 6.0;
int output;

// Transmit package bytes: < START | COUNT | CH1_L | CH1_H | CH2_L | CH2_H | CRC >
byte txBuff[PKG_SIZE] = {0xAA, 0, 0, 0, 0, 0, 0};

DDS_10bit_1Ch toneOne; // create instance of DDS_10bit_1Ch
DDS_10bit_1Ch toneTwo;


void setup() {
  Serial.begin(115200);
  while (!Serial);

  pinMode(ledPin, OUTPUT);
  pinMode(rdyPin, INPUT);
  pinMode(debugPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  toneOne.begin(fToneOne, 0, T_LOOP_US * 1.0e-6); // Initilize the DDS (frequency, startphase, update period time).
  toneTwo.begin(fToneTwo, 0, T_LOOP_US * 1.0e-6);

  nextLoop = micros() + T_LOOP_US; // Set the loop timer variable.
}


void loop() {

  digitalWrite(debugPin, HIGH); // toggle pin for timing validation.

  toneOne.update(); // update wave synthesizers
  toneTwo.update();

  if (++div10ms >= 50) // rate divider for sampler 100 Hz
  {
    div10ms = 0;
    output = toneOne.out1;
    txBuff[2] = output & 0xFF; // CH1 LSB
    txBuff[3] = (output >> 8) & 0xFF; // CH1 MSB

    output = toneTwo.out1;
    txBuff[4] = output & 0xFF; // CH2 LSB
    txBuff[5] = (output >> 8) & 0xFF; // CH2 MSB

    txBuff[1] = pkg_cnt; // package counter
    pkg_cnt++;
    txBuff[6] = (txBuff[1] + txBuff[2] + txBuff[3] + txBuff[4] + txBuff[5]) & 0xFF; // checksum
    Serial.write(txBuff, PKG_SIZE); // transmit package.
  }

  if (++div500ms >= 2500) // rate divider for blinking LED
  {
    div500ms = 0;
    digitalWrite(ledPin, !digitalRead(ledPin));
  }

  digitalWrite(debugPin, LOW); // toggle pin for timing validation.

  // Maintain ~1ms loop time
  while (nextLoop > micros()) 
  {
    ;
  }
  nextLoop += T_LOOP_US;  // Set next loop time
}
