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
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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
fsm_t on_off, menus, price, mode;

int aux = 1, tariff_mode = 1;
float price_current = 0.0, price_nextday = 0.0, price_nextweek = 0.0;

unsigned long interval, last_cycle;
unsigned long loop_micros;

// Set new state
void set_state(fsm_t& fsm, int new_state)
{
  if (fsm.state != new_state) {  // if the state chnanged tis is reset
    fsm.state = new_state;
    fsm.tes = millis();
    fsm.tis = 0;
    aux = 1;
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
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();

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
  on_off.tis = cur_time - on_off.tes;
  menus.tis = cur_time - menus.tes;
  price.tis = cur_time - price.tes;
  mode.tis = cur_time - mode.tes;

  // Calculate next state for the menus state machine (if not frozen)
  if (menus.state == 1 && S4 && !prevS4)
  {
    menus.new_state = 2;
  }
  else if (menus.state == 1 && S1 && !prevS1)
  {
    menus.new_state = 6;
  }
  else if (menus.state == 2 && S4 && !prevS4)
  {
    menus.new_state = 3;
  }
  else if(menus.state == 2 && S1 && !prevS1)
  {
    menus.new_state = 1;
  }
  else if (menus.state == 3 && S4 && !prevS4)
  {
    menus.new_state = 4;
  }
  else if (menus.state == 3 && S1 && !prevS1)
  {
    menus.new_state = 2;
  }
  else if (menus.state == 4 && S4 && !prevS4)
  {
    menus.new_state = 5;
  }
  else if(menus.state == 4 && S1 && !prevS1)
  {
    menus.new_state = 3;
  }
  else if (menus.state == 5 && S4 && !prevS4)
  {
    menus.new_state = 6;
  }
  else if(menus.state == 5 && S1 && !prevS1)
  {
    menus.new_state = 4;
  }
  else if (menus.state == 6 && S4 && !prevS4)
  {
    menus.new_state = 1;
  }
  else if (menus.state == 6 && S1 && !prevS1)
  {
    menus.new_state = 5;
  }

  // PRICE FSM
  // Calculate next state for the price state machine (use S2 and S3 to navigate between states)
  if(menus.state != 1)
  {
    price.new_state = 0; // Deactivate price fsm
    aux = 1;
  }
  else if (price.state == 0 && menus.state == 1) // Prices menu
  {
    price.new_state = 1; // Activate price fsm
    aux = 1;
  }
  else if (price.state == 1 && S3 && !prevS3)
  {
    price.new_state = 2;
    aux = 1;
  }
  else if (price.state == 1 && S2 && !prevS2)
  {
    price.new_state = 3;
    aux = 1;
  }
  else if (price.state == 2 && S3 && !prevS3)
  {
    price.new_state = 3;
    aux = 1;
  }
  else if (price.state == 2 && S2 && !prevS2)
  {
    price.new_state = 1;
    aux = 1;
  }
  else if (price.state == 3 && S3 && !prevS3)
  {
    price.new_state = 1;
    aux = 1;
  }
  else if (price.state == 3 && S2 && !prevS2)
  {
    price.new_state = 2;
    aux = 1;
  }

  // MODE FSM
  // Calculate next state for the mode state machine (use S2 and S3 to navigate between modes)
  if(menus.state != 2)
  {
    mode.new_state = 0; // Deactivate mode fsm
    aux = 1;
  }
  else if (mode.state == 0 && menus.state == 2) // Mode menu
  {
    mode.new_state = 1; // Activate mode fsm
    aux = 1;
  }
  else if (mode.state == 1 && S3 && !prevS3)
  {
    mode.new_state = 2;
    aux = 1;
  }
  else if (mode.state == 1 && S2 && !prevS2)
  {
    mode.new_state = 3;
    aux = 1;
  }
  else if (mode.state == 2 && S3 && !prevS3)
  {
    mode.new_state = 3;
    aux = 1;
  }
  else if (mode.state == 2 && S2 && !prevS2)
  {
    mode.new_state = 1;
    aux = 1;
  }
  else if (mode.state == 3 && S3 && !prevS3)
  {
    mode.new_state = 1;
    aux = 1;
  }
  else if (mode.state == 3 && S2 && !prevS2)
  {
    mode.new_state = 2;
    aux = 1;
  }

  // ON_OFF FSM
  // Calculate next state for the on_off state machine
  if (on_off.state == 0 && (S1 && !prevS1 || S2 && !prevS2 || S3 && !prevS3 || S4 && !prevS4))
  {
    on_off.new_state = 1;
    menus.new_state = 1;  // Unfreezes menus sm
  }
  else if (on_off.state == 1 && S1 && S4) // Turn off if S1 and S4 are pressed simultaneously
  {
    on_off.new_state = 0;
    menus.new_state = 0; // Freezes menus sm
  }

  // Update the states
  set_state(on_off, on_off.new_state);
  set_state(menus, menus.new_state);
  set_state(price, price.new_state);
  set_state(mode, mode.new_state);

  // Update tariff mode
  if (mode.state == 1) tariff_mode = 1;
  else if (mode.state == 2) tariff_mode = 2;
  else if (mode.state == 3) tariff_mode = 3;

  // Actions set by the current state of the price state machine
  if (price.state == 1 && aux) // Current price menu
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setFont(NULL);
    display.setCursor(0, 5);
    display.println("Current price:");
    display.setCursor(0, 15);
    display.println(price_current);
    display.setCursor(0, 30);
    display.println("Use S2/S3 to navigate between future price previsions");
    display.display();
    aux = 0;
  }
  else if (price.state == 2) // Tomorrows price menu
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setFont(NULL);
    display.setCursor(0, 5);
    display.println("Tomorrow's price:");
    display.setCursor(0, 15);
    display.println(price_nextday);
    display.setCursor(0, 30);
    display.println("Use S2/S3 to navigate between future price previsions");
    display.display();
    aux = 0;
  }
  else if (price.state == 3) // Next weeks price menu
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setFont(NULL);
    display.setCursor(0, 5);
    display.println("Next week's price:");
    display.setCursor(0, 15);
    display.println(price_nextweek);
    display.setCursor(0, 30);
    display.println("Use S2/S3 to navigate between future price previsions");
    display.display();
    aux = 0;
  }

  // Actions set by the current state of the mode state machine
  if (mode.state !=0 && aux)
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setFont(NULL);
    display.setCursor(0, 5);
    display.println("Current tariff mode:");
    display.setCursor(0, 15);
    display.println(tariff_mode);
    display.setCursor(0, 30);
    display.println("Use S2/S3 to change  tariff mode");
    display.display();
    aux = 0;
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

  Serial.print(" on_off.state: ");
  Serial.print(on_off.state);

  Serial.print(" menus.state: ");
  Serial.print(menus.state);

  Serial.print(" loop: ");
  Serial.println(micros() - loop_micros);
}
