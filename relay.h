#ifndef __LED_H
#define __LED_H

#include "Arduino.h"

#define LED_PIN 8

#define LED(x) digitalWrite(LED_PIN, x)
#define LED_TOGGLE() digitalWrite(LED_PIN, !digitalRead(LED_PIN))


void led_init(void); 
// void delay_ms(uint16 ums);

#endif
