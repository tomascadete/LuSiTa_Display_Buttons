#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

void setColor(Adafruit_NeoPixel strip, uint32_t color);

uint32_t Wheel(Adafruit_NeoPixel strip, byte WheelPos);

void theaterChaseRainbow(Adafruit_NeoPixel strip, uint8_t wait, int cycles);

void ledInit(Adafruit_NeoPixel strip);
