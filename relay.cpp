#include "LED.h"


void led_init(void) 
{
    pinMode(LED_PIN, OUTPUT); 
    digitalWrite(LED_PIN, HIGH);
}
// void delay_ms(uint16 ums)
// {

// }