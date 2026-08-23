#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>

// ======================================================
// CONFIGURACOES Wi-Fi
// ======================================================
const char* WIFI_SSID = "lactea-iot-aulas";
const char* WIFI_PASS = "iotempire";

// ======================================================
// CONFIGURACOES MQTT
// ======================================================
const char* MQTT_HOST = "192.168.6.1";
const uint16_t MQTT_PORT = 1883;

const char* MQTT_TOPIC_BASE = "sensores/pms7003";

// ======================================================
// DEBUG USB
// Coloque 0 para desligar os prints USB no uso final
// ======================================================
#define DEBUG_USB 1

#if DEBUG_USB
  #define DBG(x) Serial.print(x)
  #define DBGLN(x) Serial.println(x)
#else
  #define DBG(x)
  #define DBGLN(x)
#endif

// ======================================================
// PMS7003
// PMS TXD -> Wemos D7/GPIO13
// O TX do SoftwareSerial fica em D6, mas nao precisa conectar
// ======================================================
SoftwareSerial pmsSerial(D7, D6);

uint8_t buffer[32];

// ======================================================
// MQTT / Wi-Fi
// ======================================================
WiFiClient espClient;
PubSubClient mqttClient(espClient);

char boardId[20];
char mqttClientId[40];
char mqttTopic[80];

unsigned long lastMqttAttempt = 0;
const unsigned long MQTT_RETRY_INTERVAL = 5000;

unsigned long lastPublish = 0;
const unsigned long PUBLISH_INTERVAL = 1000;

// ======================================================
// FUNCOES AUXILIARES
// ======================================================
uint16_t read16(uint8_t high, uint8_t low) {
  return ((uint16_t)high << 8) | low;
}

void buildIds() {
  uint32_t chipId = ESP.getChipId();

  snprintf(boardId, sizeof(boardId), "ESP-%06X", chipId);
  snprintf(mqttClientId, sizeof(mqttClientId), "pms7003-%s", boardId);
  snprintf(mqttTopic, sizeof(mqttTopic), "%s/%s", MQTT_TOPIC_BASE, boardId);
}

void setupWiFiRadio() {
  WiFi.persistent(false);

  WiFi.mode(WIFI_OFF);
  delay(2000);

  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  // Reduz pico de corrente do radio Wi-Fi
  WiFi.setOutputPower(8.5);

  // Modo mais estavel para ESP8266
  WiFi.setPhyMode(WIFI_PHY_MODE_11G);

  WiFi.setAutoReconnect(true);

  delay(1000);
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  DBG("Conectando ao Wi-Fi: ");
  DBGLN(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) {
    delay(1000);
    yield();

    DBG("status=");
    DBGLN(WiFi.status());
  }

  if (WiFi.status() == WL_CONNECTED) {
    DBGLN("Wi-Fi conectado.");
    DBG("IP: ");
    DBGLN(WiFi.localIP().toString());
    DBG("RSSI: ");
    DBGLN(String(WiFi.RSSI()));
  } else {
    DBGLN("Falha ao conectar no Wi-Fi.");
    DBG("Status final: ");
    DBGLN(WiFi.status());
  }
}

bool connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (mqttClient.connected()) return true;

  DBG("Conectando ao MQTT... ");

  bool ok = mqttClient.connect(mqttClientId);

  if (ok) {
    DBGLN("conectado.");
  } else {
    DBG("falhou. Estado: ");
    DBGLN(mqttClient.state());
  }

  return ok;
}

bool readPMSFrame() {
  while (pmsSerial.available()) {
    if (pmsSerial.read() == 0x42) {
      unsigned long startHeader = millis();

      while (!pmsSerial.available() && millis() - startHeader < 100) {
        yield();
      }

      if (!pmsSerial.available()) return false;

      if (pmsSerial.read() == 0x4D) {
        buffer[0] = 0x42;
        buffer[1] = 0x4D;

        int index = 2;
        unsigned long start = millis();

        while (index < 32 && millis() - start < 1000) {
          if (pmsSerial.available()) {
            buffer[index++] = pmsSerial.read();
          }
          yield();
        }

        if (index < 32) {
          DBGLN("Frame incompleto.");
          return false;
        }

        uint16_t checksum = 0;

        for (int i = 0; i < 30; i++) {
          checksum += buffer[i];
        }

        uint16_t receivedChecksum = read16(buffer[30], buffer[31]);

        if (checksum != receivedChecksum) {
          DBGLN("Checksum invalido.");
          return false;
        }

        return true;
      }
    }
  }

  return false;
}

// ======================================================
// PUBLICACAO DOS DADOS
// Payload enxuto, sem CF1/ATM duplicado
// ======================================================
void publishPMSData() {
  // Usando os campos atmosfericos do PMS7003
  uint16_t pm1_atm  = read16(buffer[10], buffer[11]);
  uint16_t pm25_atm = read16(buffer[12], buffer[13]);
  uint16_t pm10_atm = read16(buffer[14], buffer[15]);

  char payload[256];

  snprintf(
    payload,
    sizeof(payload),
    "{"
      "\"timestamp_ms\":%lu,"
      "\"sensor_type\":\"pms7003\","
      "\"board_id\":\"%s\","
      "\"pm1_0\":%u,"
      "\"pm2_5\":%u,"
      "\"pm10_0\":%u,"
      "\"rssi\":%d"
    "}",
    millis(),
    boardId,
    pm1_atm,
    pm25_atm,
    pm10_atm,
    WiFi.RSSI()
  );

  bool ok = mqttClient.publish(mqttTopic, payload, false);

  DBG("PM1.0: ");
  DBG(pm1_atm);
  DBG("  PM2.5: ");
  DBG(pm25_atm);
  DBG("  PM10: ");
  DBG(pm10_atm);
  DBG("  MQTT: ");
  DBGLN(ok ? "OK" : "FALHOU");
}

// ======================================================
// SETUP
// ======================================================
void setup() {
#if DEBUG_USB
  Serial.begin(115200);
  delay(5000);

  Serial.println();
  Serial.println("BOOT OK - PMS7003 Wi-Fi MQTT");
#endif

  buildIds();

  DBG("Board ID: ");
  DBGLN(boardId);

  DBG("Topico MQTT: ");
  DBGLN(mqttTopic);

  setupWiFiRadio();

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  connectWiFi();
  connectMQTT();

  // Inicia o PMS depois do Wi-Fi/MQTT
  pmsSerial.begin(9600);
  DBGLN("PMS7003 iniciado.");
}

// ======================================================
// LOOP
// ======================================================
void loop() {
  yield();

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!mqttClient.connected()) {
    unsigned long now = millis();

    if (now - lastMqttAttempt >= MQTT_RETRY_INTERVAL) {
      lastMqttAttempt = now;
      connectMQTT();
    }
  } else {
    mqttClient.loop();
  }

  if (readPMSFrame()) {
    unsigned long now = millis();

    if (mqttClient.connected() && now - lastPublish >= PUBLISH_INTERVAL) {
      lastPublish = now;
      publishPMSData();
    }
  }
}
