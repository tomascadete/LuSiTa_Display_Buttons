#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

void setColor(Adafruit_NeoPixel strip, uint32_t color){
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);  // Define a cor do LED
    strip.show();  // Mostra as alterações nos LEDs
    // delay(5);
  }
}

uint32_t Wheel(Adafruit_NeoPixel strip, byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void theaterChaseRainbow(Adafruit_NeoPixel strip, uint8_t wait, int cycles)
{
    for (int j=0; j < cycles; j++) {     // cycle all 256 colors in the wheel
      for (int q=0; q < 3; q++) {
        for (uint16_t i=0; i < strip.numPixels(); i=i+3)
        {
          strip.setPixelColor(i+q, Wheel(strip, ((i+j) % 255)));    //turn every third pixel on
        }
        strip.show();
        delay(wait);

        for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
          strip.setPixelColor(i+q, 0);        //turn every third pixel off
        }
      }
    }
}

void ledInit(Adafruit_NeoPixel strip)
{
    theaterChaseRainbow(strip, 50, 10);
}
