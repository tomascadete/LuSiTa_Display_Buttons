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
fsm_t on_off, menus, price, tariff, limits, battery, wifi;

bool wifi_connected = true;
char ssid[32] = "eduroam";
int aux = 1, saved_tariff = 1, battery_level = 99;
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

  tariff.new_state = 1;
  set_state(tariff, tariff.new_state);
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
  price.tis = cur_time - price.tes;
  tariff.tis = cur_time - tariff.tes;
  limits.tis = cur_time - limits.tes;
  battery.tis = cur_time - battery.tes;
  wifi.tis = cur_time - wifi.tes;

  // Calculate next state for the menus state machine (use LEFT and RIGHT to navigate between states)
  if (menus.state == 1 && RIGHT && !prevRIGHT)
  {
    menus.new_state = 3;
  }
  else if (menus.state == 1 && LEFT && !prevLEFT)
  {
    menus.new_state = 6;
  }
  else if (menus.state == 3 && RIGHT && !prevRIGHT)
  {
    menus.new_state = 4;
  }
  else if (menus.state == 3 && LEFT && !prevLEFT)
  {
    menus.new_state = 1;
  }
  else if (menus.state == 4 && RIGHT && !prevRIGHT)
  {
    menus.new_state = 5;
  }
  else if(menus.state == 4 && LEFT && !prevLEFT)
  {
    menus.new_state = 3;
  }
  else if (menus.state == 5 && RIGHT && !prevRIGHT)
  {
    menus.new_state = 6;
  }
  else if(menus.state == 5 && LEFT && !prevLEFT)
  {
    menus.new_state = 4;
  }
  else if (menus.state == 6 && RIGHT && !prevRIGHT)
  {
    menus.new_state = 1;
  }
  else if (menus.state == 6 && LEFT && !prevLEFT)
  {
    menus.new_state = 5;
  }

  // PRICE FSM
  // Calculate next state for the price state machine (use HOME and OK to navigate between price displays)
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
  else if (price.state == 1 && OK && !prevOK)
  {
    price.new_state = 2;
    aux = 1;
  }
  else if (price.state == 1 && HOME && !prevHOME)
  {
    price.new_state = 3;
    aux = 1;
  }
  else if (price.state == 2 && OK && !prevOK)
  {
    price.new_state = 3;
    aux = 1;
  }
  else if (price.state == 2 && HOME && !prevHOME)
  {
    price.new_state = 1;
    aux = 1;
  }
  else if (price.state == 3 && OK && !prevOK)
  {
    price.new_state = 1;
    aux = 1;
  }
  else if (price.state == 3 && HOME && !prevHOME)
  {
    price.new_state = 2;
    aux = 1;
  }

  // TARIFF FSM
  // Calculate next state for the tariff state machine (use HOME and OK to navigate between tariffs when in the tariffs menu)
  if(menus.state == 3)
  {
    if (tariff.state == 1 && OK && !prevOK)
    {
      tariff.new_state = 2;
      aux = 1;
    }
    else if (tariff.state == 1 && HOME && !prevHOME)
    {
      tariff.new_state = 3;
      aux = 1;
    }
    else if (tariff.state == 2 && OK && !prevOK)
    {
      tariff.new_state = 3;
      aux = 1;
    }
    else if (tariff.state == 2 && HOME && !prevHOME)
    {
      tariff.new_state = 1;
      aux = 1;
    }
    else if (tariff.state == 3 && OK && !prevOK)
    {
      tariff.new_state = 1;
      aux = 1;
    }
    else if (tariff.state == 3 && HOME && !prevHOME)
    {
      tariff.new_state = 2;
      aux = 1;
    }
  }

  // LIMITS FSM
  // Calculate the next state for the limits state machine (no navigation, only display of set limits)
  if(menus.state != 4)
  {
    limits.new_state = 0; // Deactivate limits fsm
    aux = 1;
  }
  else if (limits.state == 0 && menus.state == 4) // Limits menu
  {
    limits.new_state = 1; // Activate limits fsm
    aux = 1;
  }

  // BATTERY FSM
  // Calculate the next state for the battery state machine (no navigation, only display of battery status)
  if(menus.state != 5)
  {
    battery.new_state = 0; // Deactivate battery fsm
    aux = 1;
  }
  else if (battery.state == 0 && menus.state == 5) // Battery menu
  {
    battery.new_state = 1; // Activate battery fsm
    aux = 1;
  }

  // WIFI FSM
  // Calculate the next state for the wifi state machine (no navigation, only display of wifi status and SSID)
  // If wifi is not connected, display "Not connected"
  // IF wifi is connected, display "Connected to: " + SSID
  if(menus.state != 6)
  {
    wifi.new_state = 0; // Deactivate wifi fsm
    aux = 1;
  }
  else if (wifi.state == 0 && menus.state == 6) // Wifi menu
  {
    wifi.new_state = 1; // Activate wifi fsm
    aux = 1;
  }

  // ON_OFF FSM
  // Calculate next state for the on_off state machine
  if (on_off.state == 0 && ((LEFT && !prevLEFT) || (HOME && !prevHOME) || (OK && !prevOK) || (RIGHT && !prevRIGHT)))
  {
    on_off.new_state = 1;
    menus.new_state = 1;  // Unfreezes menus sm
  }
  else if (on_off.state == 1 && LEFT && RIGHT) // Turn off if LEFT and RIGHT are pressed simultaneously
  {
    on_off.new_state = 0;
    menus.new_state = 0; // Freezes menus sm
    aux = 1;
  }

  // Update the states
  set_state(on_off, on_off.new_state);
  set_state(menus, menus.new_state);
  set_state(price, price.new_state);
  set_state(tariff, tariff.new_state);
  set_state(limits, limits.new_state);
  set_state(battery, battery.new_state);
  set_state(wifi, wifi.new_state);

  // Update price_current (TODO: get the prices from the cloud)
  price_current = 0.5;
  price_nextday = 0.6;
  price_nextweek = 0.7;

  // Update saved_tariff
  if (tariff.state == 1) saved_tariff = 1;
  else if (tariff.state == 2) saved_tariff = 2;
  else if (tariff.state == 3) saved_tariff = 3;

  // Update limit_low and limit_high (TODO: get the limit values from the cloud)
  limit_low = 5;
  limit_high = 10;

  // Update battery level (TODO: implement a way to get the battery level)
  battery_level = 99;

  // Update wifi status (TODO: implement a way to get the wifi status)
  wifi_connected = true;

  if (on_off.state == 0)
  {
    display.clearDisplay();
    display.display();
    aux = 0;
  }

  // Actions set by the current state of the price state machine
  if (price.state == 1 && aux) // Current price menu
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setFont(NULL);
    display.setCursor(0, 0);
    display.println("Current price:");
    display.println(price_current);
    display.setCursor(0, 20);
    display.println("Use HOME/OK to navigate");
    display.display();
    aux = 0;
  }
  else if (price.state == 2) // Tomorrows price menu
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setFont(NULL);
    display.setCursor(0, 0);
    display.println("Price in 24h:");
    display.println(price_nextday);
    display.setCursor(0, 20);
    display.println("Use HOME/OK to navigate");
    display.display();
    aux = 0;
  }
  else if (price.state == 3) // Next weeks price menu
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setFont(NULL);
    display.setCursor(0, 0);
    display.println("Next week's price:");
    display.println(price_nextweek);
    display.setCursor(0, 20);
    display.println("Use HOME/OK to navigate");
    display.display();
    aux = 0;
  }


  // Actions set by the current state of the tariff state machine
  if (menus.state == 3 && aux)
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setFont(NULL);
    display.setCursor(0, 0);
    display.println("Current tariff:");
    display.println(saved_tariff);
    display.setCursor(0, 20);
    display.println("Use HOME/OK to change");
    display.display();
    aux = 0;
  }

  // Actions set by the current state of the limits state machine
  if (limits.state == 1 && aux)
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setFont(NULL);
    display.setCursor(0, 0);
    display.println("Current limits");
    display.setCursor(0, 11);
    display.print("Low: ");
    display.println(limit_low);
    display.print("High: ");
    display.println(limit_high);
    display.display();
    aux = 0;
  }

  // Actions set by the current state of the battery state machine
  if (battery.state == 1 && aux)
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setFont(NULL);
    display.setCursor(0, 0);
    display.println("Current battery level");
    display.setCursor(0, 15);
    display.print(battery_level);
    display.print(" %");
    display.display();
    aux = 0;
  }

  // Actions set by the current state of the wifi state machine
  if (wifi.state == 1 && aux)
  {
    if (wifi_connected)
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 0);
      display.println("Connected to Wi-Fi!");
      display.setCursor(0, 15);
      display.print("SSID: ");
      display.println(ssid);
      display.display();
      aux = 0;
    }
    else
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 0);
      display.println("No Wi-Fi connection.");
      display.println("Please configure the Wi-Fi using the APP  or your browser");
      display.display();
      aux = 0;
    }
  }


  // Debug using the serial port
  Serial.print("LEFT: ");
  Serial.print(LEFT);

  Serial.print(" HOME: ");
  Serial.print(HOME);

  Serial.print(" OK: ");
  Serial.print(OK);

  Serial.print(" RIGHT: ");
  Serial.print(RIGHT);

  Serial.print(" on_off.state: ");
  Serial.print(on_off.state);

  Serial.print(" menus.state: ");
  Serial.print(menus.state);

  Serial.print(" price.state: ");
  Serial.print(price.state);

  Serial.print(" tariff.state: ");
  Serial.print(tariff.state);

  Serial.print(" limits.state: ");
  Serial.print(limits.state);

  Serial.print(" battery.state: ");
  Serial.print(battery.state);

  Serial.print(" wifi.state: ");
  Serial.print(wifi.state);

  Serial.print(" loop: ");
  Serial.println(micros() - loop_micros);
}
