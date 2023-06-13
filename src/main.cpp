#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "Adafruit_I2CDevice.h"
#include <Adafruit_NeoPixel.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define LEDS_PIN 1        // Data pin for the LED strip
#define LED_COUNT 115    // Number of LEDs in the strip

// Declare our NeoPixel strip object:
Adafruit_NeoPixel leds(LED_COUNT, LEDS_PIN, NEO_GRB + NEO_KHZ800);

// I2C for communication with the display
TwoWire CustomI2C0(16, 17); // SDA, SCL

// I2C for communication with Raspberry Pi Pico
TwoWire CustomI2C1(18, 19); // SDA, SCL

// Initialize the SSD1306 display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &CustomI2C0, -1);

#define PIN_BUTTON_LEFT 12
#define PIN_BUTTON_HOME 13
#define PIN_BUTTON_OK 14
#define PIN_BUTTON_RIGHT 15

#define N_COLORS 6

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
uint8_t SWITCH, prevSWITCH;

// Our finite state machine
fsm_t on_off, led_control, menus, brightness, cur_price, previsions, set_colour, set_treshold;

bool wifi_connected = true, current_price_mode = true, previsions_mode = false, set_colour_mode = false;
char ssid[32] = "eduroam";
char password[32] = "password";
int aux = 1, auxLED = 1, battery_level = 99, reminder=0, led_brightness = 60, day = 1, month = 1, year = 2023, hour = 17, minute = 00, j;
int months_31days[7] = {1,3,5,7,8,10,12};
int months_30days[11] = {1,3,4,5,6,7,8,9,10,11,12};
int months_all[12] = {1,2,3,4,5,6,7,8,9,10,11,12};
float price_current = 1.6, price_prevision = 1.4;
char colors[N_COLORS][10] = {"red", "green", "blue", "yellow", "orange", "white"};
int colour_index = 0;

// Treshold_1 is when the light changes from green to greenish yellow
// Treshold_2 is when the light changes from greenish yellow to yellow
// Treshold_3 is when the light changes from yellow to orange
// Treshold_4 is when the light changes from orange to red
float treshold_1 = 0.40, treshold_2 = 0.75, treshold_3 = 1.10, treshold_4 = 1.50;

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
  Serial.begin(9600);

  pinMode(PIN_BUTTON_LEFT, INPUT_PULLUP);
  pinMode(PIN_BUTTON_HOME, INPUT_PULLUP);
  pinMode(PIN_BUTTON_OK, INPUT_PULLUP);
  pinMode(PIN_BUTTON_RIGHT, INPUT_PULLUP);

  leds.begin();  // Call this to start up the LED strip.

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) // 3D
  { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  CustomI2C1.begin(); // Raspberry Pi Pico I2C

  leds.setBrightness(led_brightness);
  leds.show();
}

void loop()
{
  // To measure the time between loop() calls
  //unsigned long last_loop_micros = loop_micros; 

  // Do this only every "interval" miliseconds 
  // It helps to clear the switches bounce effect
  unsigned long now = millis();
  if (now - last_cycle > interval)
  {
    loop_micros = micros();
    last_cycle = now;

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
    led_control.tis = cur_time - led_control.tes;
    menus.tis = cur_time - menus.tes;
    brightness.tis = cur_time - brightness.tes;
    cur_price.tis = cur_time - cur_price.tes;
    previsions.tis = cur_time - previsions.tes;
    set_colour.tis = cur_time - set_colour.tes;
    set_treshold.tis = cur_time - set_treshold.tes;

    // Calculate next state for the set_treshold state machine
    if (set_treshold.state == 1 && RIGHT && !prevRIGHT) // treshold 1 selection
    {
      if(treshold_1 < treshold_2 - 0.05)
      {
        treshold_1 += 0.05;
      }
      aux = 1;
    }
    else if (set_treshold.state == 1 && LEFT && !prevLEFT)
    {
      if (treshold_1 > 0.05)
      {
        treshold_1 -= 0.05;
      }
      aux = 1;
    }
    else if (set_treshold.state == 1 && OK && !prevOK)
    {
      set_treshold.new_state = 2; // treshold 2 selection
    }
    else if (set_treshold.state == 2 && RIGHT && !prevRIGHT) // treshold 2 selection
    {
      if (treshold_2 < treshold_3 - 0.05)
      {
        treshold_2 += 0.05;
      }
      aux = 1;
    }
    else if (set_treshold.state == 2 && LEFT && !prevLEFT)
    {
      if (treshold_2 > treshold_1 + 0.05)
      {
        treshold_2 -= 0.05;
      }
      aux = 1;
    }
    else if (set_treshold.state == 2 && OK && !prevOK)
    {
      set_treshold.new_state = 3; // treshold 3 selection
    }
    else if (set_treshold.state == 3 && RIGHT && !prevRIGHT) // treshold 3 selection
    {
      if (treshold_3 < treshold_4 - 0.05)
      {
        treshold_3 += 0.05;
      }
      aux = 1;
    }
    else if (set_treshold.state == 3 && LEFT && !prevLEFT)
    {
      if (treshold_3 > treshold_2 + 0.05)
      {
        treshold_3 -= 0.05;
      }
      aux = 1;
    }
    else if (set_treshold.state == 3 && OK && !prevOK)
    {
      set_treshold.new_state = 4; // treshold 4 selection
    }
    else if (set_treshold.state == 4 && RIGHT && !prevRIGHT) // treshold 4 selection
    {
      treshold_4 += 0.05;
      aux = 1;
    }
    else if (set_treshold.state == 4 && LEFT && !prevLEFT)
    {
      if (treshold_4 > treshold_3 + 0.05)
      {
        treshold_4 -= 0.05;
      }
      aux = 1;
    }
    else if (set_treshold.state == 4 && OK && !prevOK)
    {
      set_treshold.new_state = 5; // Confirmation screen
      auxLED = 1;
    }
    else if (set_treshold.state == 5 && set_treshold.tis > 3000)
    {
      set_treshold.new_state = 0; // Back to main menu
      menus.new_state = reminder;
      aux = 1;
    }
    


    // Calculate next state for the set_colour state machine (red, green, blue, yellow, orange, white)
    if (set_colour.state == 1 && RIGHT && !prevRIGHT)
    {
      if(colour_index < N_COLORS-1)
      {
        colour_index++;
      }
      aux = 1;
    }
    else if (set_colour.state == 1 && LEFT && !prevLEFT)
    {
      if(colour_index > 0)
      {
        colour_index--;
      }
      aux = 1;
    }
    else if (set_colour.state == 1 && OK && !prevOK)
    {
      set_colour_mode = true;
      previsions_mode = false;
      current_price_mode = false;
      set_colour.new_state = 2; // Confirmation screen
      aux = 1;
      auxLED = 1;
    }
    else if (set_colour.state == 2 && set_colour.tis > 3000)
    {
      set_colour.new_state = 0; // Back to main menu
      menus.new_state = reminder;
      aux = 1;
    }

    // Calculate next state for the previsions state machine (dia, mês, ano, hora, minuto)
    if (previsions.state == 1 && RIGHT && !prevRIGHT)
    {
      if(day < 31)
      {
        day++;
      }
      aux = 1;
    }
    else if (previsions.state == 1 && LEFT && !prevLEFT)
    {
      if(day > 1)
      {
        day--;
      }
      aux = 1;
    }
    else if (previsions.state == 1 && OK && !prevOK)
    {
      if (day == 31)
      {
        month = months_31days[0];
      }
      else if (day == 30)
      {
        month = months_30days[0];
      }
      else
      {
        month = months_all[0];
      }
      j=0;
      previsions.new_state = 2; // Month
      aux = 1;
    }
    else if (previsions.state == 2 && RIGHT && !prevRIGHT)
    {
      if (day == 31 && j < 6-1)
      {
        j++;
        month = months_31days[j];
      }
      else if (day == 30 && j < 11-1)
      {
        j++;
        month = months_30days[j];
      }
      else if (day < 30 && j < 12-1)
      {
        j++;
        month = months_all[j];
      }
      aux = 1;
    }
    else if (previsions.state == 2 && LEFT && !prevLEFT)
    {
      if (day == 31 && j > 0)
      {
        j--;
        month = months_31days[j];
      }
      else if (day == 30 && j > 0)
      {
        j--;
        month = months_30days[j];
      }
      else if (day < 30 && j > 0)
      {
        j--;
        month = months_all[j];
      }
      aux = 1;
    }
    else if (previsions.state == 2 && OK && !prevOK)
    {
      previsions.new_state = 3; // Year
    }
    else if (previsions.state == 3 && RIGHT && !prevRIGHT)
    {
      year++;
      aux = 1;
    }
    else if (previsions.state == 3 && LEFT && !prevLEFT)
    {
      year--;
      aux = 1;
    }
    else if (previsions.state == 3 && OK && !prevOK)
    {
      previsions.new_state = 4; // Hour
    }
    else if (previsions.state == 4 && RIGHT && !prevRIGHT)
    {
      if(hour < 23)
      {
        hour++;
      }
      aux = 1;
    }
    else if (previsions.state == 4 && LEFT && !prevLEFT)
    {
      if(hour > 0)
      {
        hour--;
      }
      aux = 1;
    }
    else if (previsions.state == 4 && OK && !prevOK)
    {
      previsions.new_state = 5; // Minute
    }
    else if (previsions.state == 5 && RIGHT && !prevRIGHT)
    {
      if(minute < 59)
      {
        minute++;
      }
      aux = 1;
    }
    else if (previsions.state == 5 && LEFT && !prevLEFT)
    {
      if(minute > 0)
      {
        minute--;
      }
      aux = 1;
    }
    else if (previsions.state == 5 && OK && !prevOK)
    {
      // TODO: Call the function to get the prevision for the specified date and time
      // price_prevision = get_price_prevision(day, month, year, hour, minute);
      previsions.new_state = 6; // Show prevision for selected day
    }
    else if (previsions.state == 6 && OK && !prevOK)
    {
      previsions_mode = true; // Activate prevision mode
      current_price_mode = false;
      set_colour_mode = false;
      previsions.new_state = 7; // Confirmation of prevision mode activation
      auxLED = 1;
    }
    else if (previsions.state == 7 && previsions.tis >= 3000)
    {
      previsions.new_state = 0;
      menus.new_state = reminder;
      aux = 1;
    }


    // Calculate next state for the cur_price state machine
    if (cur_price.state == 1 && cur_price.tis >= 3000)
    {
      cur_price.new_state = 0;
      menus.new_state = reminder;
      aux = 1;
      auxLED = 1;
    }


    // Calculate next state for the brightness state machine
    if(brightness.state == 1 && RIGHT && !prevRIGHT)
    {
      if(led_brightness < 240)
      {
        led_brightness += 20;
        // leds.setBrightness(led_brightness);
      }
      aux = 1;
    }
    else if(brightness.state == 1 && LEFT && !prevLEFT)
    {
      if(led_brightness >= 20)
      {
        led_brightness -= 20;
        // leds.setBrightness(led_brightness);
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
    else if (menus.state == 1 && LEFT && !prevLEFT)
    {
      menus.new_state = 7; // Allows to set the colour change tresholds for the LEDs if OK is pressed
    }
    else if (menus.state == 2 && RIGHT && !prevRIGHT)
    {
      menus.new_state = 3; // Shows current price and enables current price mode on the LEDs if OK is pressed
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
    else if (menus.state == 3 && OK && !prevOK)
    {
      current_price_mode = true;
      previsions_mode = false;
      set_colour_mode = false;
      cur_price.new_state = 1; // Activates cur_price state machine
      reminder = menus.state; // Saves the current state to return to it later
      menus.new_state = 0; // Disables menus state machine (because another one was entered)
    }
    else if (menus.state == 3 && LEFT && !prevLEFT)
    {
      menus.new_state = 2; // Shows brightness and enables brightness adjustment if OK is pressed
    }
    else if (menus.state == 3 && RIGHT && !prevRIGHT)
    {
      menus.new_state = 4; // Allows to configure a prevision for a given day at a given time. To enter configuration panel, OK must be pressed
    }
    else if (menus.state == 4 && OK && !prevOK)
    {
      previsions.new_state = 1; // Activates previsions state machine
      reminder = menus.state; // Saves the current state to return to it later
      menus.new_state = 0; // Disables menus state machine (because another one was entered)
    }
    else if (menus.state == 4 && LEFT && !prevLEFT)
    {
      menus.new_state = 3; // Shows current price and enables current price mode on the LEDs if OK is pressed
    }
    else if (menus.state == 4 && RIGHT && !prevRIGHT)
    {
      menus.new_state = 5; // Allows to set a specific colour for the LEDs if OK is pressed
    }
    else if (menus.state == 5 && OK && !prevOK)
    {
      set_colour.new_state = 1; // Activates set_colour state machine
      reminder = menus.state; // Saves the current state to return to it later
      menus.new_state = 0; // Disables menus state machine (because another one was entered)
    }
    else if (menus.state == 5 && LEFT && !prevLEFT)
    {
      menus.new_state = 4; // Allows to configure a prevision for a given day at a given time. To enter configuration panel, OK must be pressed
    }
    else if (menus.state == 5 && RIGHT && !prevRIGHT)
    {
      menus.new_state = 6; // Displays the Wi-Fi state
    }
    else if (menus.state == 6 && LEFT && !prevLEFT)
    {
      menus.new_state = 5; // Allows to set a specific colour for the LEDs if OK is pressed
    }
    else if (menus.state == 6 && RIGHT && !prevRIGHT)
    {
      menus.new_state = 7; // Allows to set the colour change tresholds for the LEDs if OK is pressed
    }
    else if (menus.state == 7 && OK && !prevOK)
    {
      set_treshold.new_state = 1; // Activates set_tresholds state machine
      reminder = menus.state; // Saves the current state to return to it later
      menus.new_state = 0; // Disables menus state machine (because another one was entered)
    }
    else if (menus.state == 7 && RIGHT && !prevRIGHT)
    {
      menus.new_state = 1; // Home screen displays the battery level
    }
    else if (menus.state == 7 && LEFT && !prevLEFT)
    {
      menus.new_state = 6; // Displays the Wi-Fi state
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

    // LED_CONTROL FSM
    // Calculate next state for the led_control state machine
    if(current_price_mode && led_control.state!=1)
    {
      led_control.new_state = 1; // Current price mode
      auxLED = 1;
    }
    else if(previsions_mode && led_control.state!=2)
    {
      led_control.new_state = 2; // Previsions mode
      auxLED = 1;
    }
    else if(set_colour_mode && led_control.state!=3)
    {
      led_control.new_state = 3; // Set colour mode
      auxLED = 1;
    }
    

    // Update the states
    set_state(on_off, on_off.new_state);
    set_state(led_control, led_control.new_state);
    set_state(menus, menus.new_state);
    set_state(brightness, brightness.new_state);
    set_state(cur_price, cur_price.new_state);
    set_state(previsions, previsions.new_state);
    set_state(set_colour, set_colour.new_state);
    set_state(set_treshold, set_treshold.new_state);

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
    else if (menus.state == 3 && aux) // Current price screen
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 1);
      display.print("Current price: ");
      display.println(price_current);
      display.setCursor(0, 13);
      display.println("Press OK to enable   current price mode");
      display.display();
      aux = 0;
    }
    else if (menus.state == 4 && aux) // Previsions screen
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 1);
      display.println("Press OK to configurea specific date and  time to observe      previsions");
      display.display();
      aux = 0;
    }
    else if (menus.state == 5 && aux) // Set colour main screen
    {
      if(set_colour_mode)
      {
        display.clearDisplay();
        display.setTextColor(WHITE);
        display.setTextSize(1);
        display.setFont(NULL);
        display.setCursor(0, 1);
        display.println("Set colour mode is   activated!");
        display.setCursor(0, 20);
        display.print("Colour: ");
        display.println(colors[colour_index]);
        display.display();
        aux = 0;
      }
      else
      {
        display.clearDisplay();
        display.setTextColor(WHITE);
        display.setTextSize(1);
        display.setFont(NULL);
        display.setCursor(0, 4);
        display.println("Press OK to select a colour for the LED's to display");
        display.display();
        aux = 0;
      }
    }
    else if (menus.state == 6 && aux) // Display Wi-Fi state
    {
      if(wifi_connected)
      {
        display.clearDisplay();
        display.setTextColor(WHITE);
        display.setTextSize(1);
        display.setFont(NULL);
        display.setCursor(0, 9);
        display.println("Connected to Wi-Fi!");
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
        display.setCursor(0, 4);
        display.println("No Wi-Fi connection");
        display.println("Please configure in  the browser");
        display.display();
        aux =0;
      }
    }
    else if (menus.state == 7 && aux) // Allows colour treshold configuration if OK is pressed
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 4);
      display.println("Press OK to configurethe colour change    price tresholds");
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

    // Actions set by the current state of the cur_price state machine
    if (cur_price.state == 1 && aux) // Confirmation screen
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 1);
      display.println("Success!!!");
      display.setCursor(0, 13);
      display.println("Colours now representthe current price");
      display.display();
      aux = 0;
    }

    // Actions set by the current state of the previsions state machine
    if (previsions.state == 1 && aux) // Select the day (scroll with LEFT and RIGHT)
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 1);
      display.println("Choose the day");
      display.println("Use L/R and then OK");
      display.setCursor(0, 20);
      display.println(day);
      display.display();
      aux = 0;
    }
    else if (previsions.state == 2 && aux) // Select the month (scroll with LEFT and RIGHT)
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 1);
      display.println("Choose the month");
      display.println("Use L/R and then OK");
      display.setCursor(0, 20);
      display.println(month);
      display.display();
      aux = 0;
    }
    else if (previsions.state == 3 && aux) // Select the year (scroll with LEFT and RIGHT)
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 1);
      display.println("Choose the year");
      display.println("Use L/R and then OK");
      display.setCursor(0, 20);
      display.println(year);
      display.display();
      aux = 0;
    }
    else if (previsions.state == 4 && aux) // Select the hour (scroll with LEFT and RIGHT)
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 1);
      display.println("Choose the hour");
      display.println("Use L/R and then OK");
      display.setCursor(0, 20);
      display.println(hour);
      display.display();
      aux = 0;
    }
    else if (previsions.state == 5 && aux) // Select the minute (scroll with LEFT and RIGHT)
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 1);
      display.println("Choose the minute");
      display.println("Use L/R and then OK");
      display.setCursor(0, 20);
      display.println(minute);
      display.display();
      aux = 0;
    }
    else if (previsions.state == 6 && aux) // Display the prevision for the specified date and time
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 0);
      display.print("Prevision at");
      display.print(" ");
      display.print(hour);
      display.print(":");
      display.println(minute);
      display.print(day);
      display.print("/");
      display.print(month);
      display.print("/");
      display.print(year);
      display.print(": ");
      display.println(price_prevision);
      display.println("Press OK to track    this price");
      display.display();
      aux = 0;
    }
    else if (previsions.state == 7 && aux)
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 0);
      display.println("Success!!!");
      display.println("Colours will now     represent the trackedprice");
      display.display();
      aux = 0;
    }

    // Actions set by the current state of the set_colour state machine
    if (set_colour.state == 1 && aux) // Display the selected colour
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 1);
      display.println("Select a color using L/R and OK");
      display.setCursor(0, 21);
      display.print("Set colour: ");
      display.println(colors[colour_index]);
      display.display();
      aux = 0;
    }
    else if (set_colour.state == 2 && aux) // Confirmation screen
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 9);
      display.println("Colour now set to");
      display.println(colors[colour_index]);
      display.display();
      aux = 0;
    }
    
    // Actions set by the current state of the set_treshold state machine
    if (set_treshold.state == 1 && aux) // Display the currently selected treshold_1 (OK to confirm)
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 4);
      display.println("Green -> Gree/Yellow");
      display.println("Use L/R and OK");
      display.print("Treshold 1: ");
      display.print(treshold_1);
      display.display();
      aux = 0;
    }
    else if (set_treshold.state == 2 && aux) // Display the currently selected treshold_2 (OK to confirm)
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 4);
      display.println("Green/Yellow->Yellow");
      display.println("Use L/R and OK");
      display.print("Treshold 2: ");
      display.print(treshold_2);
      display.display();
      aux = 0;
    }
    else if (set_treshold.state == 3 && aux) // Display the currently selected treshold_3 (OK to confirm)
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 4);
      display.println("Yellow -> Orange");
      display.println("Use L/R and OK");
      display.print("Treshold 3: ");
      display.print(treshold_3);
      display.display();
      aux = 0;
    }
    else if (set_treshold.state == 4 && aux) // Display the currently selected treshold_4 (OK to confirm)
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 4);
      display.println("Orange -> Red");
      display.println("Use L/R and OK");
      display.print("Treshold 4: ");
      display.print(treshold_4);
      display.display();
      aux = 0;
    }
    else if (set_treshold.state == 5 && aux) // Confirmation screen
    {
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setFont(NULL);
      display.setCursor(0, 2);
      display.println("Set tresholds:");
      display.setCursor(0, 14);
      display.print(treshold_1);
      display.print("/");
      display.print(treshold_2);
      display.print("/");
      display.print(treshold_3);
      display.print("/");
      display.print(treshold_4);
      display.display();
      aux = 0;
    }

    // Actions set by the current state of the led_control state machine
    if (led_control.state == 1 && auxLED)
    {
      if(price_current < treshold_1) // Green
      {
        for(int i = 0; i < LED_COUNT; i++)
        {
          leds.setPixelColor(i, leds.Color(0, 255, 0)); 
        }
        auxLED = 0;
      }
      else if(price_current >= treshold_1 && price_current < treshold_2) // Greenish Yellow
      {
        for(int i = 0; i < LED_COUNT; i++) 
        {
          leds.setPixelColor(i, leds.Color(154, 205, 50)); 
        }
        auxLED = 0;
      }
      else if(price_current >= treshold_2 && price_current < treshold_3) // Yellow
      {
        for(int i = 0; i < LED_COUNT; i++)
        {
          leds.setPixelColor(i, leds.Color(255, 255, 0));
        }
        auxLED = 0;
      }
      else if(price_current >= treshold_3 && price_current < treshold_4) // Orange
      {
        for(int i = 0; i < LED_COUNT; i++)
        {
          leds.setPixelColor(i, leds.Color(255, 165, 0));
        }
        auxLED = 0;
      }
      else if(price_current >= treshold_4) // RED
      {
        for(int i = 0; i < LED_COUNT; i++)
        {
          leds.setPixelColor(i, leds.Color(255, 0, 0));
        }
        auxLED = 0;
      }
    }
    else if (led_control.state == 2 && auxLED)
    {
      if(price_prevision < treshold_1) // Green
      {
        for(int i = 0; i < LED_COUNT; i++)
        {
          leds.setPixelColor(i, leds.Color(0, 255, 0)); 
        }
        auxLED = 0;
      }
      else if(price_prevision >= treshold_1 && price_prevision < treshold_2)  // Greenish Yellow
      {
        for(int i = 0; i < LED_COUNT; i++) 
        {
          leds.setPixelColor(i, leds.Color(154, 205, 50)); 
        }
        auxLED = 0;
      }
      else if(price_prevision >= treshold_2 && price_prevision < treshold_3) // Yellow
      {
        for(int i = 0; i < LED_COUNT; i++)
        {
          leds.setPixelColor(i, leds.Color(255, 255, 0));
        }
        auxLED = 0;
      }
      else if(price_prevision >= treshold_3 && price_prevision < treshold_4) // Orange
      {
        for(int i = 0; i < LED_COUNT; i++)
        {
          leds.setPixelColor(i, leds.Color(255, 165, 0));
        }
        auxLED = 0;
      }
      else if(price_prevision >= treshold_4) // Red
      {
        for(int i = 0; i < LED_COUNT; i++)
        {
          leds.setPixelColor(i, leds.Color(255, 0, 0));
        }
        auxLED = 0;
      }
    }
    else if (led_control.state == 3 && auxLED)
    {
      if (colour_index == 0) // RED
      {
        for(int i = 0; i < LED_COUNT; i++)
        {
          leds.setPixelColor(i, leds.Color(255, 0, 0));
        }
        auxLED = 0;
      }
      else if (colour_index == 1) // GREEN
      {
        for(int i = 0; i < LED_COUNT; i++)
        {
          leds.setPixelColor(i, leds.Color(0, 255, 0));
        }
        auxLED = 0;
      }
      else if (colour_index == 2) // BLUE
      {
        for(int i = 0; i < LED_COUNT; i++)
        {
          leds.setPixelColor(i, leds.Color(0, 0, 255));
        }
        auxLED = 0;
      }
      else if (colour_index == 3) // YELLOW
      {
        for(int i = 0; i < LED_COUNT; i++)
        {
          leds.setPixelColor(i, leds.Color(255, 255, 0));
        }
        auxLED = 0;
      }
      else if (colour_index == 4) // ORANGE
      {
        for(int i = 0; i < LED_COUNT; i++)
        {
          leds.setPixelColor(i, leds.Color(255, 165, 0));
        }
        auxLED = 0;
      }
      else if (colour_index == 5) // WHITE
      {
        for(int i = 0; i < LED_COUNT; i++)
        {
          leds.setPixelColor(i, leds.Color(255, 255, 255));
        }
        auxLED = 0;
      }
    }
    leds.show();

    // Debug using the serial port
    Serial.print("LEFT: ");
    Serial.print(LEFT);

    Serial.print(" RIGHT: ");
    Serial.print(RIGHT);

    Serial.print(" HOME: ");
    Serial.print(HOME);

    Serial.print(" OK: ");
    Serial.print(OK);

    Serial.print(" led_control.state: ");
    Serial.print(led_control.state);

    Serial.print(" on_off.state: ");
    Serial.print(on_off.state);

    Serial.print(" menus.state: ");
    Serial.print(menus.state);

    Serial.print(" brightness.state: ");
    Serial.print(brightness.state);

    Serial.print(" cur_price.state: ");
    Serial.print(cur_price.state);

    Serial.print(" previsions.state: ");
    Serial.print(previsions.state);

    Serial.print(" set_colour.state: ");
    Serial.print(set_colour.state);

    Serial.print(" set_treshold.state: ");
    Serial.print(set_treshold.state);

    Serial.print(" loop: ");
    Serial.println(micros() - loop_micros);

  }
}
