#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "HX711.h"

///////////////////////
// Scale Definitions //
///////////////////////
const int SCALE_DOUT_PIN = D1;
const int SCALE_SCK_PIN = D2;
const float scaleValue = 20155.7;
//////////////////////
// Wifi Definitions //
//////////////////////
const char *ssid     = "MineroIT";
const char *password = "Minero2018";
///////////////////////
// IFTTT Definitions //
///////////////////////
const char* IFTTT_host = "maker.ifttt.com";
const int httpPort = 80;
const char* IFTTT_key= "cqXTr5xq0NOWrgDTWi-cGlPimM_aZhn-BrAn1pk9pp_";
const char* IFTTT_notification_event = "new_weight";
////////////////////////
// Google Definitions //
////////////////////////
const String hostGoogle = "www.googleapis.com";
const int securePort = 443;
const String clientId = "573483825659-lee6eet874b7n2ph6dv63p22902p5k9j.apps.googleusercontent.com";
const String refreshToken = "1/oOWMZp4Ai45gtgLQ_S7iWckR_tF079KEjC0Z6XtnMQI";
const String clientSecret = "2h316_5gZTzDpyF5IRePPnZZ";

HX711 scale;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

bool measuring = false;
bool debug = true;

void setup() {
  Serial.begin(38400);

  WiFi.begin(ssid, password);

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
  scale.set_scale(scaleValue);
  scale.tare();
  Serial.println("Done");
}

void loop() {

  float currentWeight = measureWeight();

  if (isBodyWeight(currentWeight)) {

    if (!measuring) {
      switchMeasuring(true);

      if (debug) {
        Serial.print("New weight...");
        Serial.println(currentWeight, 1);
        String weight = String(currentWeight);
        sendNotification(weight);
        postWeight(weight);
      }
    }
 
  } else {
    switchMeasuring(false);
  }

  scale.power_down();
  delay(5000);
  scale.power_up();
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

bool isBodyWeight(float weight) {
  return weight > 1.0;
}

void switchMeasuring(bool state) {
  measuring = state;
}

String refreshToken() {
  WiFiClientSecure client;
  String mbodyRefresh = "client_secret=" + clientSecret + "&grant_type=refresh_token&refresh_token=" + refreshToken + "&client_id=" + clientId;
  Serial.println("REFRESH TOKEN\n\nConnecting to host\n");

  if (!client.connect(hostGoogle, securePort)) {
    Serial.println("Connection failed");
    return "";
  }

  //building header
  String payload = String("POST ") +  "/oauth2/v4/token HTTP/1.1\n" +
                   "Host: " + hostGoogle + "\n" +
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
  const size_t capacity = JSON_OBJECT_SIZE(4) + 270;
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

int postWeight(String weight) {
  String accessToken = refreshToken();
  if (accessToken.length() < 1) {
    return -1;
  }

  WiFiClientSecure client;
  Serial.println("\nPOST WEIGHT\n\nConnecting to host\n");

  if (!client.connect(hostGoogle, securePort)) {
    Serial.println("Connection failed");
    return -21;
  }

  // Current time in nanos
  String timens = String(timeClient.getEpochTime()) + "000000000";
  //building body; WARNING: "dataSourceId" could be different if you created one different from mine
  String body = String("{") + "\"minStartTimeNs\":" + timens + ",\"maxEndTimeNs\":" + timens + ",\"dataSourceId\":\"derived:com.google.weight:573483825659:wodster:ArduinoScale:9988012:ScaleDataSource\",\"point\":[{\"dataTypeName\":\"com.google.weight\"," +
                 "\"originDataSourceId\":\"\",\"startTimeNanos\":" + timens + ",\"endTimeNanos\":" + timens + ",\"value\":[{\"fpVal\":" + weight + "}]}]}";

  String payload = String("PATCH ") +  "/fitness/v1/users/me/dataSources/derived:com.google.weight:573483825659:wodster:ArduinoScale:9988012:ScaleDataSource/datasets/" + timens + "-" + timens +  " HTTP/1.1\n" +
                   "Host: " + hostGoogle + "\n" +
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

int sendNotification(String weight) {
  WiFiClient client;
  
  // Make sure we can connect
  if (!client.connect(IFTTT_host, httpPort)) {
    return -1;
  }

  String payload = String("PATCH ") +  "/trigger/" + String(IFTTT_notification_event) + "/with/key/" + String(IFTTT_key) + "?value1=" + weight + " HTTP/1.1\n" +
                   "Host: " + IFTTT_host + "\n";

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
  
  Serial.print(F("\n Notification sent succesfully."));
  client.stop();
  return 1;
}

