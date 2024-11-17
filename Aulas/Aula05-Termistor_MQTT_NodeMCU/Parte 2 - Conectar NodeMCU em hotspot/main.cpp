#include <Arduino.h>
#include <ESP8266WiFi.h>

const char* ssid = "iPhone 14 Emilio";
const char* pass = "iotempire";

void setupWifi(){
  delay(10000);

  delay(10);
  Serial.print("\nConnecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("\nWiFi connected\nIP address: ");
  Serial.println(WiFi.localIP());
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  setupWifi();
}

void loop() {
}
