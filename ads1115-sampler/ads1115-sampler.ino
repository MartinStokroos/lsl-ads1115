/*
   ADS1115 sampler sketch

   This sketch samples 2 differential, 16-bit ADC input channels at 100Hz.

   By: M. Stokroos

   Date: February 24th, 2026


   Output package byte format: < START | COUNT | CH1_LSB | CH1_MSB | CH2_LSB | CH2_MSB | CRC >


   License: MIT License

   Copyright (c) M.Stokroos 2021

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

#include <Wire.h>
#include "Adafruit_ADS1X15.h"

#define T_LOOP_US 5000    // main loop duration in us.
#define PKG_SIZE 7

const int rdyPin = 2; // Interrupt pin (must support interrupt)
const int ledPin = 13; // LED pin
const int debugPin = 3;
volatile bool conversionReady = false;
unsigned long nextLoop;
unsigned int tick = 0;
int result;
unsigned char pkg_cnt = 0;

// Transmit package bytes: < START | COUNT | CH1_L | CH1_H | CH2_L | CH2_H | CRC >
byte txBuff[PKG_SIZE] = {0xAA, 0, 0, 0, 0, 0, 0};

enum ReadState {
  READ_CH1,
  READ_CH2
};

ReadState currentState = READ_CH1;

Adafruit_ADS1115 ads;


// Interrupt Service Routine
void convRdy() {
  conversionReady = true;
}


void setup() {
  Serial.begin(115200);
  while (!Serial);

  Wire.setClock(400000);

  pinMode(ledPin, OUTPUT);
  pinMode(rdyPin, INPUT);
  pinMode(debugPin, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(rdyPin), convRdy, FALLING);
  digitalWrite(ledPin, LOW);

  if (!ads.begin()) {
    digitalWrite(ledPin, HIGH); // turn on LED if fails.
    while (1);
  }

  // Set the internal ADC data rate.
  ads.setDataRate(RATE_ADS1115_250SPS);

  // Configure the ADC input stage gain setting.
  // GAIN_TWOTHIRDS = +/-6.144 V,
  // GAIN_ONE = +/-4.096V,
  // GAIN_TWO = +/-2.048V,
  // GAIN_FOUR = +/-1.024V,
  // GAIN_EIGHT = +/-0.512V,
  // GAIN_SIXTEEN = +/-0.256V
  ads.setGain(GAIN_TWO);  // +/- 2.048V, largest full input range for 5V supply.

  // Configure ALERT/RDY pin as conversion ready
  ads.enableRDYMode();

  // Start first conversion
  ads.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_0_1, false);

  nextLoop = micros() + T_LOOP_US; // Set the loop timer variable.
}


void loop() {

  if (conversionReady) {
    conversionReady = false;
    digitalWrite(debugPin, HIGH); // toggle pin for timing validation.

    result = ads.getLastConversionResults();

    if (currentState == READ_CH1) {
      // channel1 result
      txBuff[2] = result & 0xFF; // CH1 LSB
      txBuff[3] = (result >> 8) & 0xFF; // CH1 MSB

      // Start second channel
      currentState = READ_CH2;
      ads.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_2_3, false);

      if (++tick % 100 == 0) // Rate divider for blinking LED
      {
          digitalWrite(ledPin, !digitalRead(ledPin)); // Blink the LED.
      }

    } else {
      // channel2 result
      txBuff[4] = result & 0xFF; // CH2 LSB
      txBuff[5] = (result >> 8) & 0xFF; // CH2 MSB

      // Restart first channel
      currentState = READ_CH1;
      ads.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_0_1, false);

      txBuff[1] = pkg_cnt; // package counter
      pkg_cnt++;
      txBuff[6] = (txBuff[1] + txBuff[2] + txBuff[3] + txBuff[4] + txBuff[5]) & 0xFF; // checksum
      Serial.write(txBuff, PKG_SIZE); // transmit package. rate ~100 Hz
    }
    digitalWrite(debugPin, LOW); // toggle pin for timing validation.
  }

  // Maintain ~200 Hz pacing.
  while (nextLoop > micros()) {;}
  nextLoop += T_LOOP_US;  // Set next loop time
}
