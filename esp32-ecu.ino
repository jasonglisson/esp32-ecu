// Import required libraries
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include "WiFiClient.h"
#include "WiFiAP.h"
#include "wavTrigger.h"
#include "AsyncTCP.h"
#include "neotimer.h"
#include "ezButton.h"
#include <ESP32Servo.h>

HardwareSerial MySerial(2);

// Servo servo;
int servoPin = 32;
Servo servo;

/*** Variables ***/
// Buttons
ezButton GREEN_BUTTON_PIN (14);
ezButton YELLOW_BUTTON_PIN (27);
ezButton RED_BUTTON_PIN (26);

#define RED_LED_PIN 18
#define YELLOW_LED_PIN 19
#define GREEN_LED_PIN 25

// Trap
ezButton TRAP_TRIGGER (4);
#define TRAP_LIGHT 22

// Main Lever
ezButton MAIN_LEVER (5);

// Cutoff Process
ezButton CUTOFF_LEVER (33);
#define VENT_SMOKER 13
#define VENT_FAN_LIGHT 12

// Top LEDS
#define TOP_GREEN_LED 15
#define TOP_RED_LED 2

// Needle Gauge Meter
#define NEEDLE_GAUGE 23
unsigned long NEEDLEpreviousMillis = 0;
unsigned long NEEDLEcurrentMillis;

// Generic trap flag
int trapFlag = 0x00;
int trapInsert = 0;
int lasttrapState = HIGH; // the previous state from the input pin
int trapState;     // the current reading from the input pin

//String ssid;
String pass;

// Replace with your network credentials

String ssid = "Ecto Containment Unit";
//const char* password = "Ghostbusters84!";
const char* password = NULL;

// Search for parameter in HTTP POST request
const char* SSID_INPUT = "ssid";
const char* PASS_INPUT = "pass";
const char* VENT_INPUT = "ventnum";
const char* ONOFF_INPUT = "ventonoff";

String onoff;
String ventnum;
int ventThreshNum;
String ventThreshSTG;

// File paths to save input values permanently
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* ventPath = "/vent.txt";
const char* onoffPath = "/onoff.txt";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

wavTrigger wTrig; // Our WAV Trigger object

// Variables will change:
int fanState = LOW;
int ledState = LOW;
int needleState = LOW;
int topRedLEDState = LOW;

int remoteVar = 0;
int randNumber;

// will store last time LED was updated
unsigned long previousMillis = 0;
unsigned long REDTOPpreviousMillis = 0;

// constants won't change:
const long interval = 10500;
unsigned long currentMillis;
unsigned long REDTOPcurrentMillis;
int count = 0;
bool shutdownstate = 0;

#define RX2 16
#define TX2 17

/*** System Status ***/
static enum {
  READY,
  TRAP_INSERT,
  RED_BTN_PREP,
  RED_BTN_PUSH,
  YELLOW_BTN_PREP,
  YELLOW_BTN_PUSH,
  LEVER_PREP,
  LEVER_PUSH,
  TRAP_CLEAN,
  CUT_OFF_LEVER,
  ECU_OFF,
  SLEEP
} state = READY;

String processor(const String& var) {

  if (var == "SSID") {
    return ssid;
  } else if (var == "PASS") {
    return pass;
  }

  return String();
}

String processor2(const String& var) {

  //  if (var == "VENTNUM"){
  //    return ventnum;
  //  }

  return String();
}

void setup() {

  Serial.begin(115200);
  servo.attach(servoPin);
  MySerial.begin(9600, SERIAL_8N1, RX2, TX2);

  Serial.println("Configuring access point...");

  // You can remove the password parameter if you want the AP to be open.
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  //Serial.println(myIP);
  delay(1000);

  Serial.println("Server started");

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html");
  });

  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });

  server.on("/heading.png", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/heading.png", "image/png");
  });

  server.on("/byline.png", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/byline.png", "image/png");
  });

  server.on("/ecu.png", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/ecu.png", "image/png");
  });

  server.on("/cinder-block.jpg", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/cinder-block.jpg", "image/jpeg");
  });

  server.on("/slime.jpg", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/slime.jpg", "image/jpeg");
  });

  server.on("/ecto-1.png", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/ecto-1.png", "image/png");
  });

  // Route to set GPIO to LOW
  server.on("/ready", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html");
    remoteReady();
  });

  // Route to set GPIO to LOW
  server.on("/shut-down", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html");
    remoteShutDown();
  });

  // Route to set GPIO to HIGH
  server.on("/vent", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html");
    remoteVent();
  });

  // Route to set GPIO to LOW
  server.on("/sleep", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html");
    remoteSleep();
  });

  // Route to set GPIO to LOW
  server.on("/ecto", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html");
    remoteEcto();
  });

  // Route to set GPIO to LOW
  server.on("/alarm", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html");
    remoteAlarm();
  });

  // Route to set GPIO to LOW
  server.on("/slimer", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html");
    remoteSlimer();
  });

  server.on("/wifi-settings", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/wifimanager.html", "text/html", false, processor);
  });

  //  server.on("/wifi-settings", HTTP_POST, [](AsyncWebServerRequest *request) {
  //      int params = request->params();
  //      for(int i=0;i<params;i++){
  //        AsyncWebParameter* p = request->getParam(i);
  //        if(p->isPost()){
  //          // HTTP POST ssid value
  //          if (p->name() == SSID_INPUT) {
  //            ssid = p->value().c_str();
  //            Serial.print("SSID set to: ");
  //            Serial.println(ssid);
  //            // Write file to save value
  //            writeFile(SPIFFS, ssidPath, ssid.c_str());
  //          }
  //          // HTTP POST pass value
  //          if (p->name() == PASS_INPUT) {
  //            pass = p->value().c_str();
  //            Serial.print("Password set to: ");
  //            Serial.println(pass);
  //            // Write file to save value
  //            writeFile(SPIFFS, passPath, pass.c_str());
  //          }
  //        }
  //      }
  //    //request->send(200, "text/plain", "Done. ESP will restart.");
  //    request->send(200, "text/html", "<br><br><br><center><h1 style='font-size:4.7rem;'>SSID and Password Set!</h1><br><h1>SSID: <span style='color:blue;'>" + ssid + "</span><br>Password: <span style='color:blue;'>" + pass + "</span></h1></center><br><center><h1>You may now begin using these settings. You will see the above SSID in your list of available Wifi networks.</h1></center><br><center><h1 style='color:red;'><b>DON'T FORGET YOUR PASSWORD!<br>WRITE IT DOWN IF YOU HAVE TO!</b></h1></center><br><br><center>When you have written your username and password down, you may close this window</center>");
  //    delay(3000);
  //    ESP.restart();
  //  });

  // Route to set GPIO to LOW
  server.on("/audio-settings", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/audio-settings.html");
  });

  server.on("/traps-cleaned", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/traps-cleaned.html", "text/html", false, processor2);
  });

  // Trap Trigger and State
  TRAP_TRIGGER.setDebounceTime(50);

  // Main buttons and state
  RED_BUTTON_PIN.setDebounceTime(50);
  YELLOW_BUTTON_PIN.setDebounceTime(50);
  GREEN_BUTTON_PIN.setDebounceTime(50);

  // Main Lever
  MAIN_LEVER.setDebounceTime(50);

  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(NEEDLE_GAUGE, OUTPUT);
  pinMode(VENT_FAN_LIGHT, OUTPUT);
  pinMode(TOP_GREEN_LED, OUTPUT);
  pinMode(TOP_RED_LED, OUTPUT);

  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(YELLOW_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, HIGH);

  digitalWrite(TOP_GREEN_LED, HIGH);
  digitalWrite(TOP_RED_LED, HIGH);

  digitalWrite(VENT_FAN_LIGHT, HIGH);

  // Cutoff Sequence
  CUTOFF_LEVER.setDebounceTime(50);

  // WAV Trigger startup at 57600
  wTrig.start();
  delay(10);

  //   Send a stop-all command and reset the sample-rate offset, in case we have
  //    reset while the WAV Trigger was already playing.
  wTrig.stopAllTracks();
  wTrig.samplerateOffset(0);            // Reset our sample rate offset
  wTrig.masterGain(0);                  // Reset the master gain to 0dB

  wTrig.trackLoad(1);
  wTrig.trackPlayPoly(1);               // Start Track 1
  wTrig.trackLoad(2);
  wTrig.update();                       // Wait 2 secs
  delay(8000);
  wTrig.trackPlayPoly(2);               // Start Containment Unit constant hum
  wTrig.trackFade(1, -40, 2000, 0);       // Fade Track 1 down to -40dB over 2 secs
  wTrig.trackFade(2, 0, 2000, 0);       // Fade Track 2 up to 0dB over 2 secs
  wTrig.update();                       // Wait 2 secs
  wTrig.trackLoop(2, 1);
  Serial.println("ECU Ready");

  // Start server
  server.begin();
}

void loop() {
  // Always update buttons first!
  RED_BUTTON_PIN.loop();
  YELLOW_BUTTON_PIN.loop();
  GREEN_BUTTON_PIN.loop();
  TRAP_TRIGGER.loop();
  MAIN_LEVER.loop();

  // Read trap sensor once
//  trapState = digitalRead(TRAP_TRIGGER);

//  // === Handle trap insertion transition ===
//  if (state == READY && trapState == HIGH && lasttrapState == LOW) {
//    Serial.println("TRAP INSERTED: transitioning to TRAP_INSERT");
//    state = TRAP_INSERT;
//  }
//  lasttrapState = trapState;

  detectShutdown();

  // === State machine ===
  switch (state) {

    case READY:
      Serial.println("State: READY");

      // If red button released, force trap insert
      if (TRAP_TRIGGER.isPressed()) {
        Serial.println("TRAP_INSERT");
        digitalWrite(GREEN_LED_PIN, LOW);
        wTrig.trackLoad(4);
        wTrig.update();
        state = TRAP_INSERT;
      }
      // LEDs for ready
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(YELLOW_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, HIGH);
     
      break;

    case TRAP_INSERT:
      // Instead of delay, wait for trapState to be stable, or use millis()
      if (TRAP_TRIGGER.isPressed()) {
        Serial.println("Playing trap insert sound");
        Serial.println("State: TRAP_INSERT");
        digitalWrite(TOP_GREEN_LED, LOW);
        digitalWrite(GREEN_LED_PIN, LOW);
        digitalWrite(TOP_RED_LED, HIGH);
        digitalWrite(RED_LED_PIN, HIGH);
        wTrig.trackPlayPoly(4);
        wTrig.update();
        // transition to next state immediately (or use a timer)
        state = RED_BTN_PREP;
      }
      break;

    case RED_BTN_PREP:
      Serial.println("State: RED_BTN_PREP");
      if (RED_BUTTON_PIN.isPressed()) {
        Serial.println("Red button pressed → RED_BTN_PUSH");
        wTrig.trackLoad(7);
        state = RED_BTN_PUSH;
      }
      break;
      
    case RED_BTN_PUSH:
      Serial.println("State: RED_BTN_PUSH");
      if (RED_BUTTON_PIN.isReleased()) {
        Serial.println("Red button released → YELLOW_BTN_PREP");
        wTrig.trackFade(2, -2, 1000, 0);
        wTrig.trackGain(7, 2);
        wTrig.trackPlayPoly(7);
        wTrig.trackLoad(8);
        wTrig.update();
        state = YELLOW_BTN_PREP;
      }
      break;
      
    case YELLOW_BTN_PREP:
      Serial.println("State: YELLOW_BTN_PREP");
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(YELLOW_LED_PIN, HIGH);
      if (YELLOW_BUTTON_PIN.isPressed()) {
        Serial.println("YELLOW_BTN_PREP");
        wTrig.trackFade(2, -2, 1000, 0);
        wTrig.trackGain(7, 2);
        wTrig.trackPlayPoly(7);
        wTrig.trackLoad(8);
        wTrig.update();
        state = YELLOW_BTN_PREP;
      }
      break;
      
    // Once the yellow button is pressed,
    case YELLOW_BTN_PUSH:
      if (YELLOW_BUTTON_PIN.isReleased()) {
        Serial.println("YELLOW BUTTON PUSHED");
        wTrig.trackFade(2, -2, 500, 0);
        wTrig.trackFade(7, -2, 500, 0);
        wTrig.update();
        wTrig.trackGain(8, 5);
        wTrig.trackPlayPoly(8);
        wTrig.update();
        digitalWrite(RED_LED_PIN, LOW);
        digitalWrite(YELLOW_LED_PIN, HIGH);
        state = LEVER_PREP;
      }
      break;
      
    // The lever is ready to be pulled down
    case LEVER_PREP:
      if (digitalRead(YELLOW_BUTTON_PIN) == HIGH) {
        Serial.println("LEVER PREP");
        state = LEVER_PUSH;
        wTrig.trackLoad(3);
      }
      detectShutdown();
      break;
      
    // The lever has been pulled down
    case LEVER_PUSH:
      if (MAIN_LEVER.isPressed()) {
        Serial.println("LEVER PUSH");
        wTrig.trackStop(8);
        wTrig.trackStop(7);
        wTrig.update();
        state = TRAP_CLEAN;
      }
      detectShutdown();
      break;
      
    // The trap is clean and the system has reset
    case TRAP_CLEAN:
      // The Main lever must be pulled back up to reset the system
      if (MAIN_LEVER.isPressed()) {
        digitalWrite(YELLOW_LED_PIN, LOW);
        digitalWrite(RED_LED_PIN, LOW);
        digitalWrite(TOP_GREEN_LED, HIGH);
        Serial.println("TRAP CLEAN");
        digitalWrite(GREEN_LED_PIN, HIGH);
        wTrig.trackFade(2, 0, 1000, 0);
        wTrig.trackGain(3, 10);
        wTrig.trackPlayPoly(3);
        wTrig.trackFade(4, 0, 2000, 0);
        wTrig.update();
        trapFlag = 0x01;
        delay(10000);
        state = READY;
      }
      detectShutdown();
      break;
      
    // This is the start of the shutdown process. You can
    // get to this step from anywhere in the process.
    case CUT_OFF_LEVER:
      if (shutdownstate == 0) {

        digitalWrite(GREEN_LED_PIN, LOW);
        digitalWrite(RED_LED_PIN, HIGH);
        digitalWrite(TOP_GREEN_LED, LOW);

        unsigned long NEEDLEcurrentMillis = millis();

        if (NEEDLEcurrentMillis - NEEDLEpreviousMillis >= 100) {
          // save the last time you blinked the LED
          NEEDLEpreviousMillis = NEEDLEcurrentMillis;

          // if the LED is off turn it on and vice-versa:
          if (needleState == LOW) {
            needleState = HIGH;
          } else {
            needleState = LOW;
          }
          digitalWrite(NEEDLE_GAUGE, needleState);
        }

        REDTOPcurrentMillis = millis();

        if (REDTOPcurrentMillis - REDTOPpreviousMillis >= 515) {
          REDTOPpreviousMillis = REDTOPcurrentMillis;
          if (topRedLEDState == LOW) {
            topRedLEDState = HIGH;
          } else {
            topRedLEDState = LOW;
          }
          digitalWrite(TOP_RED_LED, topRedLEDState);
        }

        if (currentMillis - previousMillis >= interval) {
          count++;
          previousMillis = currentMillis;
        }
      }
      switch (count) {
        case 0:
          // Run the vent PUMP
          currentMillis = millis();
          break;
        case 1:
          currentMillis = millis();
          break;
        case 2:
          // Turn the PUMP off
          // Open the Vent
          //          if (digitalRead(servoPin) == LOW) {
          //            servo.write(1500);
          //          }
          // Turn the fan on
          Serial.println("SHUT DOWN 2");
          digitalWrite(VENT_FAN_LIGHT, HIGH);
          currentMillis = millis();
          break;
        case 3:
          Serial.println("SHUT DOWN 3");
          currentMillis = millis();

          // Turn the fan off but leave vent open
          digitalWrite(VENT_FAN_LIGHT, LOW);
          wTrig.trackFade(11, -40, 1000, 0);
          break;
        case 4:
          Serial.println("SHUT DOWN 4");
          currentMillis = millis();
          shutdownstate = 1;
          break;
      }
      if (shutdownstate == 1) {
        wTrig.trackLoad(1);
        wTrig.trackLoad(2);
        wTrig.trackLoad(9);
        wTrig.trackLoad(10);
        wTrig.trackGain(1, 0);
        wTrig.trackGain(2, -40);
        wTrig.trackGain(9, -30);
        wTrig.trackGain(10, -30);
        state = ECU_OFF;
      }
      break;
      
    case ECU_OFF:
      Serial.println("ECU IS OFF");
    
      digitalWrite(TOP_RED_LED, LOW);
      digitalWrite(NEEDLE_GAUGE, LOW);

      currentMillis = millis();

      // Flash red button and play click
      if (currentMillis - previousMillis >= 600) {
        previousMillis = currentMillis;
        if (ledState == LOW) {
          ledState = HIGH;
          wTrig.trackPlayPoly(9);
        } else {
          ledState = LOW;
          wTrig.trackPlayPoly(10);
        }
        digitalWrite(RED_LED_PIN, ledState);
      }
  
      // If maincut off lever is back up, system will reset.
      if(digitalRead(CUTOFF_LEVER) == LOW) {
        wTrig.trackPlayPoly(1);
        wTrig.update();
        wTrig.trackPlayPoly(2);
        wTrig.trackFade(2, 0, 5000, 0);       // Fade Track 2 up to 0dB over 7 secs
        wTrig.update();                       // Wait 2 secs
        wTrig.trackLoop(2, 1);
        shutdownstate = 0;
        count = 0;
        trapFlag = 0x00;
        state = READY;
      }
      break;
    case SLEEP:
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(YELLOW_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
      digitalWrite(TOP_GREEN_LED, LOW);
      digitalWrite(TOP_RED_LED, LOW);
      digitalWrite(VENT_FAN_LIGHT, LOW);
      wTrig.stopAllTracks();
      break;
  }
}

void detectShutdown() {
    if (CUTOFF_LEVER.isPressed()) {
      // Lever is pulled down
      shutDown();
      state = CUT_OFF_LEVER;
      Serial.println("SHUTDOWN START");
    }
}

void shutDown() {

  // Shutdown sequence sounds
  wTrig.stopAllTracks();
  wTrig.update();
  wTrig.trackLoad(5);
  wTrig.trackLoad(11);
  wTrig.trackGain(5, 10);
  wTrig.trackGain(11, 10);
  wTrig.trackPlayPoly(5);
  wTrig.trackPlayPoly(11);
}

void remoteReady() {
  remoteVar = 0;
}

void remoteShutDown() {
  remoteVar = 1;
}

void remoteVent() {
  remoteVar = 2;
}

void remoteSleep() {
  remoteVar = 3;
}

void remoteEcto() {
  remoteVar = 4;
}

void remoteSlimer() {
  remoteVar = 5;
}

void remoteAlarm() {
  remoteVar = 6;
}

void ventProcess() {
  wTrig.trackLoad(18);
  wTrig.trackGain(18, 10);
  wTrig.trackPlayPoly(18);
  //  servo.write(1500);
  //insert fan turn on, pump, and smoker
}
