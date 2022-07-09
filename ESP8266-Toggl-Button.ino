// Includes
/// For Wifi
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
/// Wifi Response Parsing
#include <ArduinoJson.h>
#include <Arduino_JSON.h>
#include "base64.h"
/// Neopixel Handling
#include <Adafruit_NeoPixel.h>
/// Includes variables specified in readme
#include "secrets-do-not-commit.h"

// Consts
const int ButtonPin     = D5;
const int NeopixelPin   = 5; // Actually D2 but doesn't seem to work if i use D2, gpio5 on esp8266
const int NeopixelCount = 16;
const int NeopixelBrightness = 10;

// Function Decls - Should be in order of impl below setup & loop.
void ICACHE_RAM_ATTR ButtonInterruptHandler();
void colorWipe(uint32_t color, int wait);
void setAuth(String const& Token);
void connectToWifi();
String httpGETRequest();
const String StartTimeEntry(String const& Description, String const& Tags,String const& CreatedWith);
const String StopTimeEntry(String const& ID);
const bool isTimerActive();
const String getTimerData(String Input);

// Neopixel Color Codes
/// Green - Timer Running
/// Red - Timer stopped
/// Blue - No network connection
/// Pink - Running Command

// Flags
bool isButtonPressed  = false;
bool togglTimerState  = false; // False == off, True == Running
bool hasChanged       = false;

// Global Vars
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NeopixelCount, NeopixelPin, NEO_GRB + NEO_KHZ800);
String          TogglAuthorizationKey{};

//String TogglAuthorizationKey{};
String TogglTimerID;


void setup() {
  Serial.begin(115200);

  // Button and interrupt inits
  pinMode(ButtonPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(ButtonPin), ButtonInterruptHandler, RISING);

  // Neopixel Inits
  pixels.begin(); // This initializes the NeoPixel library.
  pixels.show();            // Turn OFF all pixels ASAP
  pixels.setBrightness(NeopixelBrightness); // Set BRIGHTNESS to about 1/5 (max = 255)
  colorWipe(pixels.Color(0,   0,   255)     , 50); // Blue

  // Toggl Setup
  setAuth(Token); // Should only need to do this once.

  // Wifi Setup
  connectToWifi();

  // Reset the timer state using a toggl api request to make
  // sure that we are aligned now we have reconnected.
  togglTimerState = isTimerActive();

} //ENDSETUP  

void loop() {

  // Only react to the button press if we are conencted to the network
  // otherwise we should be in the trying to reconnect loop
  if ((WiFi.status() == WL_CONNECTED)) {
    // Act on the button press
    if (isButtonPressed){
      Serial.print("Stamp(ms): ");
      Serial.println(millis());
      isButtonPressed = false;
      togglTimerState = !togglTimerState;
      hasChanged = true;
    }
  
    // Change the toggl timer state if needed
    if (togglTimerState && hasChanged){
      colorWipe(pixels.Color(255,  51,   255)     , 50); // Pink
      TogglTimerID = (StartTimeEntry("Warhammer Work", "unreviewed", "ESP-Button"));
      delay(1000); // Ensure we don't overload the API
      togglTimerState = isTimerActive();
      hasChanged = false;
    }
    else if (!togglTimerState && hasChanged)
    {
      colorWipe(pixels.Color(255,  51,   255)     , 50); // Pink
      StopTimeEntry(TogglTimerID);
      delay(1000); // Ensure we don't overload the API
      togglTimerState = isTimerActive();
      hasChanged = false;
    }
  }

  if ((WiFi.status() != WL_CONNECTED)) {
    colorWipe(pixels.Color(0,   0,   255)     , 50); // Blue
    connectToWifi();
    // Reset the timer state using a toggl api request to make
    // sure that we are aligned now we have reconnected.
    togglTimerState = isTimerActive();
  }
  
}//ENDLOOP

void ButtonInterruptHandler(){
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 2s, assume it's a bounce and ignore
  // This also stops the api calls being too frequent.
  // We also check to see if we've dealt with the previous button press
  if ( (interrupt_time - last_interrupt_time > 1000) && !isButtonPressed ) 
  {
    isButtonPressed = true;
  }
  last_interrupt_time = interrupt_time;
}

// Fill strip pixels one after another with a color. Strip is NOT cleared
// first; anything there will be covered pixel by pixel. Pass in color
// (as a single 'packed' 32-bit value, which you can get by calling
// strip.Color(red, green, blue) as shown in the loop() function above),
// and a delay time (in milliseconds) between pixels.
void colorWipe(uint32_t color, int wait) {
  for(int i=0; i<pixels.numPixels(); i++) { // For each pixel in strip...
    pixels.setPixelColor(i, color);         //  Set pixel's color (in RAM)
    pixels.show();                          //  Update strip to match
    delay(wait);                           //  Pause for a moment
  }
}

// Do some encoding to the Token and create the Authorization Header
// for the Toggl REST request.
void setAuth(String const& Token) {

  String TokenHolder{Token + ":api_token"};
  uint8_t Size{TokenHolder.length()+1};
  
  char Encoded[Size];
  char TESTBUFF[Size];
  
  TokenHolder.toCharArray(TESTBUFF,Size);
  
  int encoded_length = b64_encode(Encoded, TESTBUFF, Size-1);
  String Out{Encoded};
  
  TogglAuthorizationKey = ("Basic " + Out);
}

void connectToWifi() {

  WiFi.begin(Ssid, Password);
  Serial.println("Connecting to Wifi.");
  
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting...");
  }
  
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}


String httpGETRequest() {
  
  HTTPClient http;
  WiFiClientSecure clienthttps;
  
  // Think this line basically says "ignore certs" similar to -k in curl
  clienthttps.setInsecure();
  
  // Your Domain name with URL path or IP address with path
  http.begin(clienthttps, "https://api.track.toggl.com/api/v8/time_entries/current");
  http.addHeader("Authorization", TogglAuthorizationKey);
  
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();
  
  return payload;
}

// Borrowed from the toggl library which wouldn't compile so I'm doing it the hard way
const String StartTimeEntry(String const& Description, String const& Tags, String const& CreatedWith){

  Serial.println("Sending StartTimeEntry request");
  String payload;
  
  HTTPClient https;
  WiFiClientSecure clienthttps;
  
  // Think this line basically says "ignore certs" similar to -k in curl
  clienthttps.setInsecure();
    
  https.begin(clienthttps, "https://api.track.toggl.com/api/v8/time_entries/start");
  https.addHeader("Authorization", TogglAuthorizationKey, true);
  https.addHeader("Content-Type", " application/json");
  
  DynamicJsonDocument doc(JSON_ARRAY_SIZE(1) +JSON_OBJECT_SIZE(5 + 1));
  
  doc["time_entry"]["description"] = Description;
  doc["time_entry"]["tags"] = Tags;
  doc["time_entry"]["pid"] = TogglProjectId;
  doc["time_entry"]["created_with"] = CreatedWith;
  
  serializeJson(doc, payload);
  
  int httpResponseCode = https.POST(payload);
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = https.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  
  doc.clear();
  
  deserializeJson(doc, https.getString());
  
  String TimeID = doc["data"]["id"];
  Serial.println(TimeID);
  doc.clear();
  doc.garbageCollect();
  https.end();
  
  return TimeID;
}

const String StopTimeEntry(String const& ID){
  Serial.println("Sending StopTimeEntry request");

  String Output{};

  HTTPClient https;
  WiFiClientSecure clienthttps;
  // Think this line basically says "ignore certs" similar to -k in curl
  clienthttps.setInsecure();
  
  // Your Domain name with URL path or IP address with path
  https.begin(clienthttps, "https://api.track.toggl.com/api/v8/time_entries/" + ID +"/stop");
  https.addHeader("Authorization", TogglAuthorizationKey, true);
  https.addHeader("Content-Type", " application/json");
  
  String TMP{String(https.PUT(" "))};
  
//  doc.clear();
//  deserializeJson(doc, https.getString());
//  Serial.println(doc);
  
  https.end();
  
  Output = TMP;
  
  return Output;
}

const bool isTimerActive(){

  Serial.println("asking if timer is active");
  bool output;  
  String wid = getTimerData("wid"); //Just using a filter for less data.
  Serial.print("Status Data: ");
  Serial.println(wid);
  if(wid != "null"){
    output = true;
    Serial.println("Status == running");
    colorWipe(pixels.Color(0,   255,   0)     , 50); // Green
  }  
  else{
    output = false;
    Serial.println("Status == not running");
    colorWipe(pixels.Color(255,   0,   0)     , 50); // Red
  }
  
  return output;
}

const String getTimerData(String Input){

    String payload{};
    String Output{};
    int16_t HTTP_Code{};

    HTTPClient https;
    WiFiClientSecure clienthttps;
    // Think this line basically says "ignore certs" similar to -k in curl
    clienthttps.setInsecure();
    https.begin(clienthttps, "https://api.track.toggl.com/api/v8/time_entries/current");
    https.addHeader("Authorization", TogglAuthorizationKey);
    
    HTTP_Code = https.GET();

    if (HTTP_Code>0) {
      Serial.print("HTTP Response code: ");
      Serial.println(HTTP_Code);
    }
    else {
      Serial.print("Error code: ");
      Serial.println(HTTP_Code);
    }


    if (HTTP_Code >= 200 && HTTP_Code <= 226){
        StaticJsonDocument<46> filter;
        filter["data"][Input] = true;
        DynamicJsonDocument doc(JSON_OBJECT_SIZE(4));        
        deserializeJson(doc, https.getString(), DeserializationOption::Filter(filter));
        const String TMP_Str = doc["data"][Input];
        Output = TMP_Str;
        doc.garbageCollect();
        filter.garbageCollect();
    }
    else{ // To return the error instead of the data, no idea why the built in espHttpClient "errorToString" only returns blank space when a known error occurs...
         Output = ("Error: " + String(HTTP_Code));
    }

    https.end();
    return Output;  
}
