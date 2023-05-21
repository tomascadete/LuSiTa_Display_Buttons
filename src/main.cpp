#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "Adafruit_I2CDevice.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

TwoWire CustomI2C0(16, 17); // SDA, SCL

// Initialize the SSD1306 display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &CustomI2C0, -1);

#define PIN_BUTTON_LEFT 12
#define PIN_BUTTON_HOME 13
#define PIN_BUTTON_OK 14
#define PIN_BUTTON_RIGHT 15

typedef struct {
  int state, new_state;

  // tes - time entering state
  // tis - time in state
  unsigned long tes, tis;
} fsm_t;

// Input variables
uint8_t LEFT, prevLEFT;
uint8_t HOME, prevHOME;
uint8_t OK, prevOK;
uint8_t RIGHT, prevRIGHT;

// Our finite state machine
fsm_t on_off, menus, brightness;;

bool wifi_connected = true;
char ssid[32] = "eduroam";
int aux = 1, saved_tariff = 1, battery_level = 99, reminder=0, led_brightness = 50;
float price_current = 0.0, price_nextday = 0.0, price_nextweek = 0.0, limit_low = 0.0, limit_high = 0.0;

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
  pinMode(PIN_BUTTON_LEFT, INPUT_PULLUP);
  pinMode(PIN_BUTTON_HOME, INPUT_PULLUP);
  pinMode(PIN_BUTTON_OK, INPUT_PULLUP);
  pinMode(PIN_BUTTON_RIGHT, INPUT_PULLUP);


  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) // 3D
  { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

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
  prevLEFT = LEFT;
  prevHOME = HOME;
  prevOK = OK;
  prevRIGHT = RIGHT;
  LEFT = !digitalRead(PIN_BUTTON_LEFT);
  HOME = !digitalRead(PIN_BUTTON_HOME);
  OK = !digitalRead(PIN_BUTTON_OK);
  RIGHT = !digitalRead(PIN_BUTTON_RIGHT);

  // FSM processing
  // Update tis for all state machines
  unsigned long cur_time = millis();   // Just one call to millis()
  on_off.tis = cur_time - on_off.tes;
  menus.tis = cur_time - menus.tes;
  brightness.tis = cur_time - brightness.tes;


  // Calculate next state for the brightness state machine
  if(brightness.state == 1 && RIGHT && !prevLEFT)
  {
    if(led_brightness < 100)
    {
      led_brightness += 10;
    }
    aux = 1;
  }
  else if(brightness.state == 1 && LEFT && !prevLEFT)
  {
    if(led_brightness > 10)
    {
      led_brightness -= 10;
    }
    aux = 1;
  }
  else if(brightness.state == 1 && OK && !prevOK)
  {
    brightness.new_state = 0;
    menus.new_state = reminder;
    aux = 1;
  }

  // Calculate next state for the menus state machine
  if (menus.state == 1 && RIGHT && !prevRIGHT)
  {
    menus.new_state = 2; // Shows brightness and enables brightness adjustment if OK is pressed
  }
  else if (menus.state == 2 && RIGHT && !prevRIGHT)
  {
    menus.new_state = 3;
  }
  else if (menus.state == 2 && LEFT && !prevLEFT)
  {
    menus.new_state = 1; // Home screen displays the battery level
  }
  else if (menus.state == 2 && OK && !prevOK)
  {
    brightness.new_state = 1; // Activates brightness state machine
    reminder = menus.state; // Saves the current state to return to it later
    menus.new_state = 0; // Disables menus state machine (because another one was entered)
  }

  // ON_OFF FSM
  // Calculate next state for the on_off state machine
  if (on_off.state == 0 && HOME && !prevHOME)
  {
    on_off.new_state = 1;
    menus.new_state = 1;  // Starts the menus state machine
  }
  else if (on_off.state == 1 && LEFT && RIGHT) // Turn off if LEFT and RIGHT are pressed simultaneously
  {
    on_off.new_state = 0;
    menus.new_state = 0; // Stops the menus state machine
    aux = 1;
  }

  // Update the states
  set_state(on_off, on_off.new_state);
  set_state(menus, menus.new_state);
  set_state(brightness, brightness.new_state);

  // Update limit_low and limit_high (TODO: get the limit values from the cloud)
  limit_low = 5;
  limit_high = 10;

  // Update battery level (TODO: implement a way to get the battery level)
  battery_level = 100;

  // Update wifi status (TODO: implement a way to get the wifi status)
  wifi_connected = true;

  if (on_off.state == 0)
  {
    display.clearDisplay();
    display.display();
    aux = 0;
  }

  // Actions set by the current state of the menus state machine
  if (menus.state == 1 && aux) // Home screen (battery level)
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setFont(NULL);
    display.setCursor(0, 12);
    display.print("Battery level: ");
    display.println(battery_level);
    display.display();
    aux = 0;
  }
  else if (menus.state == 2 && aux) // Brightness screen
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setFont(NULL);
    display.setCursor(0, 2);
    display.print("LED brightness:");
    display.print(led_brightness);
    display.println(" %");
    display.setCursor(0, 18);
    display.println("Press OK to adjust");
    display.display();
    aux = 0;
  }

  // Actions set by the current state of the brightness state machine
  if (brightness.state == 1 && aux)
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setFont(NULL);
    display.setCursor(0, 1);
    display.println("Adjust brightness L/Rbuttons and press OK");
    display.setCursor(0, 20);
    display.print("Brightness: ");
    display.print(led_brightness);
    display.println(" %");
    display.display();
    aux = 0;
  }



  // Debug using the serial port
  Serial.print("LEFT: ");
  Serial.print(LEFT);

  Serial.print(" RIGHT: ");
  Serial.print(RIGHT);

  Serial.print(" HOME: ");
  Serial.print(HOME);

  Serial.print(" OK: ");
  Serial.print(OK);

  Serial.print(" on_off.state: ");
  Serial.print(on_off.state);

  Serial.print(" menus.state: ");
  Serial.print(menus.state);

  Serial.print(" brightness.state: ");
  Serial.print(brightness.state);

  Serial.print(" loop: ");
  Serial.println(micros() - loop_micros);
}
