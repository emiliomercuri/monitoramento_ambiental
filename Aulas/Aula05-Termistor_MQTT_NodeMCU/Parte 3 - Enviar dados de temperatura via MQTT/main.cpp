#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>


const double VCC = 3.3;             // NodeMCU on board 3.3v vcc
const double R2 = 10000;            // 10k ohm series resistor
const double adc_resolution = 1023; // 10-bit adc

const double A = 0.001129148;   // thermistor equation parameters
const double B = 0.000234125;
const double C = 0.0000000876741; 


/****** WiFi Connection Details *******/
// const char* ssid = "nome do wi-fi";
// const char* password = "senha do wi-fi";

const char* ssid = "iPhone 14 Emilio";
const char* password = "iotempire";

/******* MQTT Broker Connection Details *******/
const char* mqtt_server = "94ff99181c9b48e09727f2df74e0d137.s1.eu.hivemq.cloud";
const char* mqtt_username = "hivemq.webclient.1731812025042";
const char* mqtt_password = "@6Q.,38kcdvbDR2J!zCH";
const int mqtt_port =8883;


/**** Secure WiFi Connectivity Initialisation *****/
WiFiClientSecure espClient;

/**** MQTT Client Initialisation Using WiFi Connection *****/
PubSubClient client(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];


/************* Connect to WiFi ***********/
void setup_wifi() {
  delay(10);
  Serial.print("\nConnecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("\nWiFi connected\nIP address: ");
  Serial.println(WiFi.localIP());
}


/************* Connect to MQTT Broker ***********/
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP8266Client-";   // Create a random client ID
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");

      client.subscribe("led_state");   // subscribe the topics here

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");   // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}



/**** Method for Publishing MQTT Messages **********/
void publishMessage(const char* topic, String payload , boolean retained){
  if (client.publish(topic, payload.c_str(), true))
      Serial.println("Message publised ["+String(topic)+"]: "+payload);
}

void setup() {
  Serial.begin(9600);  /* Define baud rate for serial communication */

  while (!Serial) delay(1);
  setup_wifi();

  espClient.setInsecure();

  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  double Vout, Rth, temperature, adc_value; 

  if (!client.connected()) reconnect(); // check if client is connected
  client.loop();

  adc_value = analogRead(A0);
  Vout = (adc_value * VCC) / adc_resolution;
  Rth = (VCC * R2 / Vout) - R2;

/*  Steinhart-Hart Thermistor Equation:
 *  Temperature in Kelvin = 1 / (A + B[ln(R)] + C[ln(R)]^3)
 *  where A = 0.001129148, B = 0.000234125 and C = 8.76741*10^-8  */
  temperature = (1 / (A + (B * log(Rth)) + (C * pow((log(Rth)),3))));   // Temperature in kelvin

  temperature = temperature - 273.15;  // Temperature in degree celsius
  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.println(" degree celsius");
  delay(500);

  JsonDocument doc;

  doc["deviceId"] = "NodeMCU";
  doc["siteId"] = "My Demo Lab";
  doc["temperature"] = temperature;

  char mqtt_message[128];
  serializeJson(doc, mqtt_message);

  publishMessage("esp8266_data", mqtt_message, true);

  delay(1000);

}
 

