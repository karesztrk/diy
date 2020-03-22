#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "HX711.h"
#include <TM1637Display.h>

///////////////////////
// Scale Definitions //
///////////////////////
const int SCALE_DOUT_PIN = D1;
const int SCALE_SCK_PIN = D2;
const float SCALE_VALUE = 20155.7;
const float SCALE_DELTA = 0.1;
const float SCALE_VALID_WEIGHT = 5.0;
const int MAXIMAL_NUMBER_OF_MEASUREMENTS = 10;
const int MINIMAL_NUMBER_OF_MEASUREMENTS = 6;
//////////////////////
// Wifi Definitions //
//////////////////////
const char *WIFI_SSID     = "Csokimaz";
const char *WIFI_PASSWORD = "alpha12345";
////////////////////////
// Google Definitions //
////////////////////////
const String GOOGLE_HOST = "www.googleapis.com";
const int HTTPS_PORT = 443;
const String GOOGLE_CLIENT_ID = "573483825659-lee6eet874b7n2ph6dv63p22902p5k9j.apps.googleusercontent.com";
// {Kami <= 65, Karesz <= 100}
const String GOOGLE_REFRESH_TOKENS[] = {"1/g0qXDxjJQDSZkQHBpeh8Hq8UkKyE47H8Gy5Rnr0OQbM", "1/oOWMZp4Ai45gtgLQ_S7iWckR_tF079KEjC0Z6XtnMQI"};
const int GOOGLE_REFRESH_TOKEN_WEIGHT_MAPPER[] = {65, 100};
const String GOOGLE_CLIENT_SECRET = "2h316_5gZTzDpyF5IRePPnZZ";
///////////////////////
// Segment Display ////
///////////////////////
const int DISPLAY_CLK = D4;
const int DISPLAY_DIO = D3;
const int DISPLAY_BRIGHTNESS = 0x0f;

HX711 scale;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.google.com");
TM1637Display display(DISPLAY_CLK, DISPLAY_DIO);

bool debug = true;

const uint8_t SEG_SEND[] = {
  SEG_A | SEG_F | SEG_G | SEG_C | SEG_D,           // S
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,           // E
  SEG_C | SEG_E | SEG_G,                           // n
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G            // d
  };

const uint8_t SEG_DONE[] = {
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           // d
  SEG_G | SEG_E | SEG_C | SEG_D,                   // o
  SEG_C | SEG_E | SEG_G,                           // n
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G            // E
  };

const uint8_t SEG_FAIL[] = {
  SEG_A | SEG_F | SEG_G | SEG_E,                   // F
  SEG_A | SEG_F | SEG_G | SEG_E | SEG_B | SEG_C,   // A
  SEG_B | SEG_C,                                   // I
  SEG_F | SEG_E | SEG_D                            // L
  };

const uint8_t SEG_BOOT[] = {
  SEG_F | SEG_G | SEG_E | SEG_C | SEG_D,           // B
  SEG_G | SEG_E | SEG_C | SEG_D,                   // o
  SEG_G | SEG_E | SEG_C | SEG_D,                   // o
  SEG_F | SEG_G | SEG_E | SEG_D                    // t
  };

void setup() {
  Serial.begin(38400);
  display.setBrightness(DISPLAY_BRIGHTNESS);
  
  // Display booting...
  display.setSegments(SEG_BOOT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wifi");
  while (WiFi.status() != WL_CONNECTED) {
    delay (500);
    Serial.print(".");
  }
  Serial.println("Done");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  timeClient.begin();
  Serial.print("Syncing clock");
  while (!timeClient.update()) {
    delay (500);
    Serial.print(".");
  }
  Serial.println("Done");

  Serial.print("Initializing the scale...");
  scale.begin(SCALE_DOUT_PIN, SCALE_SCK_PIN);
  scale.set_scale(SCALE_VALUE);
  scale.tare();
  Serial.println("Done");

  display.setSegments(SEG_DONE);
  delay(1000);
  display.clear();
}

void loop() {

  float currentWeight = measureWeight();

  if (isValidMeasurement(currentWeight)) {

    currentWeight = startMeasurementAdjust(currentWeight);

    bool shareGranted = false;
    int shareResult = 1;
    if (isValidMeasurement(currentWeight)) {
      displayFinalWeight(currentWeight);
    
      if (debug) {
        Serial.print("New weight...");
        Serial.println(currentWeight, 1);
      }
      
      shareGranted = prepareSending();
  
      if (shareGranted) {
        shareResult = postWeight(currentWeight);
      }

    } 

    clearDisplay(shareGranted, shareResult);
  } 

  scale.power_down();
  delay(5000);
  scale.power_up();
}

float startMeasurementAdjust(float currentWeight) {
  int counter = MAXIMAL_NUMBER_OF_MEASUREMENTS;
  boolean finalMeasurement = false;
  while (!finalMeasurement) {
    displayWeight(currentWeight);
    float newWeight = measureWeight();
    finalMeasurement = abs(currentWeight - newWeight) < SCALE_DELTA && counter < MINIMAL_NUMBER_OF_MEASUREMENTS || counter == 1;
    currentWeight = newWeight;
    counter--;
  }
  return currentWeight;
}

float measureWeight() {
  if (debug) {
    Serial.print("Reading scale...");
  }
  scale.get_units();
  float weight = scale.get_units(10);
  if (debug) {
    Serial.println("Done");
  }
  return weight;
}

bool isValidMeasurement(float weight) {
  return weight >= SCALE_VALID_WEIGHT;
}

String refreshToken(float weight) {
  WiFiClientSecure client;
  String token = getRefreshTokenForWeight(weight);
  String mbodyRefresh = "client_secret=" + GOOGLE_CLIENT_SECRET + "&grant_type=refresh_token&refresh_token=" + token + "&client_id=" + GOOGLE_CLIENT_ID;
  Serial.println("REFRESH TOKEN\n\nConnecting to host\n");

  if (!client.connect(GOOGLE_HOST, HTTPS_PORT)) {
    Serial.println("Connection failed");
    return "";
  }

  //building header
  String payload = String("POST ") +  "/oauth2/v4/token HTTP/1.1\n" +
                   "Host: " + GOOGLE_HOST + "\n" +
                   "Content-length: " + mbodyRefresh.length() + "\n" +
                   "content-type: application/x-www-form-urlencoded\n\n" +
                   mbodyRefresh + "\r\n";

  client.println(payload);
  Serial.println(payload);

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(F("\n>>> Client Timeout !"));
      client.stop();
      return "";
    }
  }

  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
    Serial.println(F("Unexpected response: "));
    Serial.println(status);
    return "";
  }

  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) return "";

  char carriageReturn[] = "\r\n";
  if (!client.find(carriageReturn)) return "";

  // https://arduinojson.org/assistant/
  const size_t capacity = JSON_OBJECT_SIZE(4) + 310;
  DynamicJsonDocument doc(capacity);

  // Parse JSON object - read docs
  DeserializationError error = deserializeJson(doc, client);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return "";
  }

  // Extract values
  JsonObject root = doc.as<JsonObject>();
  Serial.println(F("Response:"));
  String authKey = root["access_token"].as<char*>();
  Serial.println(authKey);

  Serial.println(F("Correclty got the TOKEN\n"));

  // Disconnect
  client.stop();
  return authKey;
}

int postWeight(float weight) {
  String accessToken = refreshToken(weight);
  if (accessToken.length() < 1) {
    return -1;
  }

  WiFiClientSecure client;
  Serial.println("\nPOST WEIGHT\n\nConnecting to host\n");

  if (!client.connect(GOOGLE_HOST, HTTPS_PORT)) {
    Serial.println("Connection failed");
    return -21;
  }

  // Current time in nanos
  String timens = String(timeClient.getEpochTime()) + "000000000";
  String body = String("{") + "\"minStartTimeNs\":" + timens + ",\"maxEndTimeNs\":" + timens + ",\"dataSourceId\":\"derived:com.google.weight:573483825659:wodster:ArduinoScale:9988012:ScaleDataSource\",\"point\":[{\"dataTypeName\":\"com.google.weight\"," +
                 "\"originDataSourceId\":\"\",\"startTimeNanos\":" + timens + ",\"endTimeNanos\":" + timens + ",\"value\":[{\"fpVal\":" + String(weight) + "}]}]}";

  String payload = String("PATCH ") +  "/fitness/v1/users/me/dataSources/derived:com.google.weight:573483825659:wodster:ArduinoScale:9988012:ScaleDataSource/datasets/" + timens + "-" + timens +  " HTTP/1.1\n" +
                   "Host: " + GOOGLE_HOST + "\n" +
                   "Content-length: " + body.length() + "\n" +
                   "Content-type: application/json\n" +
                   "Authorization: Bearer " + accessToken + "\n\n" +
                   body + "\n";

  client.println(payload);
  Serial.println(payload);

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(F("\n>>> Client Timeout !"));
      client.stop();
      return -22;
    }
  }

  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
    Serial.print(F("Unexpected response: "));
    Serial.println(status);
    return -23;
  }
  
  Serial.print(F("\nPosted succesfully: "));
  Serial.println(weight);
  client.stop();
  return 1;
}

void displayFinalWeight(float weight) {
  float weightNumber = weight * 100.0;
  for(int i = 0; i < 3; i++) {
    if (i != 0) {
      display.clear();
      delay(1000);
    }
    display.showNumberDecEx(weightNumber, 0xE0, false);
    delay(1000);
  }
}

void displayWeight(float weight) {
  float weightNumber = weight * 100.0;
  display.showNumberDecEx(weightNumber, 0xE0, false);
}

void clearDisplay() {
  clearDisplay(false, 1);
}

void clearDisplay(bool displayResult, int result) {
  if (displayResult) {
    display.setSegments(result == 1 ? SEG_DONE : SEG_FAIL);  
    delay(1000);
  }
  
  display.clear();
}

bool prepareSending() {

  uint8_t PROGRESS_BAR[] = {SEG_G, 0, 0, 0};
  
  display.clear();
  display.setSegments(SEG_SEND);
  delay(1000);

  for(int i = 0; i < 4; i++) {
    display.setSegments(PROGRESS_BAR);

    float currentWeight = measureWeight();
    if (!isValidMeasurement(currentWeight)) {
      if (debug) {
        Serial.println("Canceled");
      }
      return false;
    }
    
    if (i < 3) {
      PROGRESS_BAR[i+1] = SEG_G;
    }
    
    delay(500);
  }
  
}

String getRefreshTokenForWeight(float weight) {
  for(int i = 0; i < sizeof(GOOGLE_REFRESH_TOKEN_WEIGHT_MAPPER) - 1; i++) {
    if (weight <= GOOGLE_REFRESH_TOKEN_WEIGHT_MAPPER[i]) {

      if (debug) {
        Serial.print("Selected Refresh Token for User");
        Serial.println(i+1);
      }
      
      return GOOGLE_REFRESH_TOKENS[i];
    }
  }
}
