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

const char* MQTT_TOPIC_BASE = "sensores/sds011";

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
// SDS011
// SDS011 TXD -> Wemos D7/GPIO13
// O TX do SoftwareSerial fica em D6, mas nao precisa conectar
// (o SDS011 em modo ativo transmite sozinho, sem precisar de comandos)
// ======================================================
SoftwareSerial sdsSerial(D7, D6);

uint8_t buffer[10];

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
void buildIds() {
  uint32_t chipId = ESP.getChipId();

  snprintf(boardId, sizeof(boardId), "ESP-%06X", chipId);
  snprintf(mqttClientId, sizeof(mqttClientId), "sds011-%s", boardId);
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

// ======================================================
// LEITURA DO FRAME DO SDS011
// Frame de 10 bytes: AA C0 [PM2.5 lo] [PM2.5 hi] [PM10 lo] [PM10 hi] [ID1] [ID2] [checksum] AB
// checksum = soma dos bytes 2..7 (mod 256)
// ======================================================
bool readSDS011Frame() {
  while (sdsSerial.available()) {
    if (sdsSerial.read() == 0xAA) {
      unsigned long startHeader = millis();

      while (!sdsSerial.available() && millis() - startHeader < 100) {
        yield();
      }

      if (!sdsSerial.available()) return false;

      if (sdsSerial.read() == 0xC0) {
        buffer[0] = 0xAA;
        buffer[1] = 0xC0;

        int index = 2;
        unsigned long start = millis();

        while (index < 10 && millis() - start < 1000) {
          if (sdsSerial.available()) {
            buffer[index++] = sdsSerial.read();
          }
          yield();
        }

        if (index < 10) {
          DBGLN("Frame incompleto.");
          return false;
        }

        uint8_t checksum = 0;

        for (int i = 2; i < 8; i++) {
          checksum += buffer[i];
        }

        if (checksum != buffer[8] || buffer[9] != 0xAB) {
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
// ======================================================
void publishSDS011Data() {
  uint16_t pm25 = (buffer[3] << 8) | buffer[2];
  uint16_t pm10 = (buffer[5] << 8) | buffer[4];

  char payload[256];

  snprintf(
    payload,
    sizeof(payload),
    "{"
      "\"timestamp_ms\":%lu,"
      "\"sensor_type\":\"sds011\","
      "\"board_id\":\"%s\","
      "\"pm2_5\":%.1f,"
      "\"pm10_0\":%.1f,"
      "\"rssi\":%d"
    "}",
    millis(),
    boardId,
    pm25 / 10.0,
    pm10 / 10.0,
    WiFi.RSSI()
  );

  bool ok = mqttClient.publish(mqttTopic, payload, false);

  DBG("PM2.5: ");
  DBG(pm25 / 10.0);
  DBG("  PM10: ");
  DBG(pm10 / 10.0);
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
  Serial.println("BOOT OK - SDS011 Wi-Fi MQTT");
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

  // Inicia o sensor depois do Wi-Fi/MQTT
  sdsSerial.begin(9600);
  DBGLN("SDS011 iniciado.");
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

  if (readSDS011Frame()) {
    unsigned long now = millis();

    if (mqttClient.connected() && now - lastPublish >= PUBLISH_INTERVAL) {
      lastPublish = now;
      publishSDS011Data();
    }
  }
}
