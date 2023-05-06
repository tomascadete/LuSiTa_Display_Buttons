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
fsm_t on_off, menus, price, mode, tariff, limits, battery, wifi;

bool wifi_connected = true;
char ssid[32] = "SSID";
int aux = 1, saved_mode = 1, saved_tariff = 1, battery_level = 99;
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
  pinMode(PIN_S1, INPUT_PULLUP);
  pinMode(PIN_S2, INPUT_PULLUP);
  pinMode(PIN_S3, INPUT_PULLUP);
  pinMode(PIN_S4, INPUT_PULLUP);


  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) // 3D
  { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
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

  delay(5000);
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
  tariff.tis = cur_time - tariff.tes;
  limits.tis = cur_time - limits.tes;
  battery.tis = cur_time - battery.tes;
  wifi.tis = cur_time - wifi.tes;

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

  // TARIFF FSM
  // Calculate next state for the tariff state machine (use S2 and S3 to navigate between tariffs)
  if(menus.state != 3)
  {
    tariff.new_state = 0; // Deactivate tariff fsm
    aux = 1;
  }
  else if (tariff.state == 0 && menus.state == 3) // Tariff menu
  {
    tariff.new_state = 1; // Activate tariff fsm
    aux = 1;
  }
  else if (tariff.state == 1 && S3 && !prevS3)
  {
    tariff.new_state = 2;
    aux = 1;
  }
  else if (tariff.state == 1 && S2 && !prevS2)
  {
    tariff.new_state = 3;
    aux = 1;
  }
  else if (tariff.state == 2 && S3 && !prevS3)
  {
    tariff.new_state = 3;
    aux = 1;
  }
  else if (tariff.state == 2 && S2 && !prevS2)
  {
    tariff.new_state = 1;
    aux = 1;
  }
  else if (tariff.state == 3 && S3 && !prevS3)
  {
    tariff.new_state = 1;
    aux = 1;
  }
  else if (tariff.state == 3 && S2 && !prevS2)
  {
    tariff.new_state = 2;
    aux = 1;
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
  if (on_off.state == 0 && ((S1 && !prevS1) || (S2 && !prevS2) || (S3 && !prevS3) || (S4 && !prevS4)))
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
  set_state(tariff, tariff.new_state);
  set_state(limits, limits.new_state);
  set_state(battery, battery.new_state);
  set_state(wifi, wifi.new_state);

  // Update saved_mode
  if (mode.state == 1) saved_mode = 1;
  else if (mode.state == 2) saved_mode = 2;
  else if (mode.state == 3) saved_mode = 3;

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
    display.println("Current mode:");
    display.setCursor(0, 15);
    display.println(saved_mode);
    display.setCursor(0, 30);
    display.println("Use S2/S3 to change  mode");
    display.display();
    aux = 0;
  }

  // Actions set by the current state of the tariff state machine
  if (tariff.state !=0 && aux)
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setFont(NULL);
    display.setCursor(0, 5);
    display.println("Current tariff:");
    display.setCursor(0, 15);
    display.println(saved_tariff);
    display.setCursor(0, 30);
    display.println("Use S2/S3 to change  tariff");
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
    display.setCursor(0, 5);
    display.println("Current defined limit");
    display.setCursor(0, 20);
    display.print("Low limit:");
    display.println(limit_low);
    display.setCursor(0, 35);
    display.print("High limit:");
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
    display.setCursor(0, 5);
    display.println("Current battery level");
    display.setCursor(0, 30);
    display.println(battery_level);
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
      display.setCursor(0, 5);
      display.println("Connected to Wi-FI!");
      display.setCursor(0, 20);
      display.println("SSID:");
      display.setCursor(0, 40);
      display.println("ssid");
      display.display();
      aux = 0;
    }
    else
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 5);
      display.println("No Wi-Fi connection");
      display.setCursor(0, 25);
      display.println("Please configure the Wi-Fi using the APP  or your browser");
      display.display();
      aux = 0;
    }
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

  Serial.print(" price.state: ");
  Serial.print(price.state);

  Serial.print(" mode.state: ");
  Serial.print(mode.state);

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
