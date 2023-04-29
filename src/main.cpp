#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "Adafruit_I2CDevice.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
// Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define PIN_S1 12
#define PIN_S2 13
#define PIN_S3 14
#define PIN_S4 15

typedef struct {
  int state, new_state;

  // tes - time entering state
  // tis - time in state
  unsigned long tes, tis;
} fsm_t;

// Input variables
uint8_t S1, prevS1;
uint8_t S2, prevS2;
uint8_t S3, prevS3;
uint8_t S4, prevS4;

// Our finite state machine
fsm_t fsm1;

unsigned long interval, last_cycle;
unsigned long loop_micros;

// Set new state
void set_state(fsm_t& fsm, int new_state)
{
  if (fsm.state != new_state) {  // if the state chnanged tis is reset
    fsm.state = new_state;
    fsm.tes = millis();
    fsm.tis = 0;
  }
}

void setup()
{
  // put your setup code here, to run once:
  // Initialize serial and wait for port to open:
  Serial.begin(9600);
  pinMode(PIN_S1, INPUT_PULLUP);
  pinMode(PIN_S2, INPUT_PULLUP);
  pinMode(PIN_S3, INPUT_PULLUP);
  pinMode(PIN_S4, INPUT_PULLUP);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  // if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  // { 
  //   Serial.println(F("SSD1306 allocation failed"));
  //   for(;;); // Don't proceed, loop forever
  // }
  
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  // display.display();

  // Clear the buffer
  // display.clearDisplay();
}

void loop()
{
  // To measure the time between loop() calls
  //unsigned long last_loop_micros = loop_micros; 

  // Do this only every "interval" miliseconds 
  // It helps to clear the switches bounce effect
  unsigned long now = millis();
  if (now - last_cycle > interval) {
    loop_micros = micros();
    last_cycle = now;
  }

  // Read the inputs
  prevS1 = S1;
  prevS2 = S2;
  prevS3 = S3;
  prevS4 = S4;
  S1 = !digitalRead(PIN_S1);
  S2 = !digitalRead(PIN_S2);
  S3 = !digitalRead(PIN_S3);
  S4 = !digitalRead(PIN_S4);

  // FSM processing
  // Update tis for all state machines
  unsigned long cur_time = millis();   // Just one call to millis()
  fsm1.tis = cur_time - fsm1.tes;

  // Calculate next state for the first state machine
  if (fsm1.state == 0 && S4 && !prevS4)
  {
    fsm1.new_state = 1;
  }
  else if (fsm1.state == 1 && S4 && !prevS4)
  {
    fsm1.new_state = 2;
  }
  else if(fsm1.state == 1 && S1 && !prevS1)
  {
    fsm1.new_state = 0;
  }
  else if (fsm1.state == 2 && S4 && !prevS4)
  {
    fsm1.new_state = 3;
  }
  else if(fsm1.state == 2 && S1 && !prevS1)
  {
    fsm1.new_state = 1;
  }
  else if (fsm1.state == 3 && S4 && !prevS4)
  {
    fsm1.new_state = 4;
  }
  else if(fsm1.state == 3 && S1 && !prevS1)
  {
    fsm1.new_state = 2;
  }
  else if (fsm1.state == 4 && S4 && !prevS4)
  {
    fsm1.new_state = 5;
  }
  else if(fsm1.state == 4 && S1 && !prevS1)
  {
    fsm1.new_state = 3;
  }
  else if (fsm1.state == 5 && S4 && !prevS4)
  {
    fsm1.new_state = 6;
  }
  else if(fsm1.state == 5 && S1 && !prevS1)
  {
    fsm1.new_state = 4;
  }
  else if (fsm1.state == 6 && S4 && !prevS4)
  {
    fsm1.new_state = 7;
  }
  else if(fsm1.state == 6 && S1 && !prevS1)
  {
    fsm1.new_state = 5;
  }
  else if (fsm1.state == 7 && S4 && !prevS4)
  {
    fsm1.new_state = 0;
  }
  else if(fsm1.state == 7 && S1 && !prevS1)
  {
    fsm1.new_state = 6;
  }


  // Update the states
  set_state(fsm1, fsm1.new_state);

  // Actions set by the current state of the first state machine
  if (fsm1.state == 0)
  {
    // DO SOMETHING
  }
  else if (fsm1.state == 1)
  {
    // DO SOMETHING
  }
  else if (fsm1.state == 2)
  {
    // DO SOMETHING
  }
  else if (fsm1.state == 3)
  {
    // DO SOMETHING
  }
  else if (fsm1.state == 4)
  {
    // DO SOMETHING
  }
  else if (fsm1.state == 5)
  {
    // DO SOMETHING
  }
  else if (fsm1.state == 6)
  {
    // DO SOMETHING
  }
  else if (fsm1.state == 7)
  {
    // DO SOMETHING
  }


  // Debug using the serial port
  Serial.print("S1: ");
  Serial.print(S1);

  Serial.print(" S2: ");
  Serial.print(S2);

  Serial.print(" S3: ");
  Serial.print(S3);

  Serial.print(" S4: ");
  Serial.print(S4);

  Serial.print(" fsm1.state: ");
  Serial.print(fsm1.state);

  Serial.print(" loop: ");
  Serial.println(micros() - loop_micros);
}
