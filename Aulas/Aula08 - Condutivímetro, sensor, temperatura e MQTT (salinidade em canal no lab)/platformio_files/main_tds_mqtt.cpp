#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>

// ======================================================
// WI-FI / MQTT (mesma infraestrutura do projeto SCD41)
// ======================================================
const char* WIFI_SSID = "star";
const char* WIFI_PASS = "iotempire";
const char* MQTT_HOST = "192.168.4.1";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_TOPIC_BASE = "sensores/tds";

#define DEBUG_USB 1
#if DEBUG_USB
  #define DBG(x) Serial.print(x)
  #define DBGLN(x) Serial.println(x)
#else
  #define DBG(x) do {} while (0)
  #define DBGLN(x) do {} while (0)
#endif

// ======================================================
// SENSORES (pinos e calibracao do codigo original)
// ======================================================
const uint8_t pinTemp = D4; // DS18B20 (OneWire)
const uint8_t pinTDS = A0;  // Sinal analogico de condutividade
const float vRef = 3.2f;   // Tensao maxima usada no codigo original (verificar A0)
const float K = 1.0f;      // Fator de calibracao da celula

const unsigned long MEASURE_INTERVAL = 1000UL;
const unsigned long WIFI_RETRY_INTERVAL = 15000UL;
const unsigned long MQTT_RETRY_INTERVAL = 5000UL;

OneWire oneWire(pinTemp);
DallasTemperature sensorTemp(&oneWire);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

char boardId[20];
char mqttClientId[40];
char mqttTopic[80];

unsigned long lastMeasure = 0;
unsigned long lastWiFiAttempt = 0;
unsigned long lastMqttAttempt = 0;

void buildIds() {
  uint32_t chipId = ESP.getChipId();
  snprintf(boardId, sizeof(boardId), "ESP-%06X", chipId);
  snprintf(mqttClientId, sizeof(mqttClientId), "tds-%s", boardId);
  snprintf(mqttTopic, sizeof(mqttTopic), "%s/%s", MQTT_TOPIC_BASE, boardId);
}

void setupWiFiRadio() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  delay(2000);
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setOutputPower(17.0);
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
  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000UL) {
    delay(500);
    yield();
  }
  if (WiFi.status() == WL_CONNECTED) {
    DBG("Wi-Fi conectado. IP: ");
    DBGLN(WiFi.localIP().toString());
  } else {
    DBGLN("Wi-Fi indisponivel; as medicoes continuam.");
  }
}

bool connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (mqttClient.connected()) return true;

  bool ok = mqttClient.connect(mqttClientId);
  if (ok) {
    DBGLN("MQTT conectado.");
  } else {
    DBG("Falha MQTT, estado: ");
    DBGLN(mqttClient.state());
  }
  return ok;
}

void publishData(float temperatura, float tensao, float condutividade) {
  if (!mqttClient.connected()) return;

  // String evita depender do suporte a %f no snprintf da plataforma.
  String payload;
  payload.reserve(220);
  payload += F("{\"timestamp_ms\":");
  payload += String(millis());
  payload += F(",\"sensor_type\":\"tds\",\"board_id\":\"");
  payload += boardId;
  payload += F("\",\"temperature_c\":");
  payload += String(temperatura, 2);
  payload += F(",\"voltage_v\":");
  payload += String(tensao, 3);
  payload += F(",\"conductivity_us_cm\":");
  payload += String(condutividade, 2);
  payload += F(",\"rssi\":");
  payload += String(WiFi.RSSI());
  payload += '}';

  bool ok = mqttClient.publish(mqttTopic, payload.c_str(), false);
  DBG("MQTT publish: ");
  DBG(ok ? "OK" : "FALHOU");
  DBG(" topico: ");
  DBGLN(mqttTopic);
}

void measureAndPublish() {
  sensorTemp.requestTemperatures();
  float temperatura = sensorTemp.getTempCByIndex(0);

  if (temperatura == DEVICE_DISCONNECTED_C || !isfinite(temperatura)) {
    DBGLN("Falha na leitura do DS18B20; medicao ignorada.");
    return;
  }

  float soma = 0.0f;
  for (int i = 0; i < 20; ++i) {
    soma += analogRead(pinTDS);
    delay(10);
    if (mqttClient.connected()) mqttClient.loop();
  }

  // Equacoes mantidas do codigo enviado (compensacao a 25 C).
  const float leitura = soma / 20.0f;
  const float tensao = leitura * vRef / 1023.0f;
  const float tensao25 = tensao / (1.0f + 0.02f * (temperatura - 25.0f));
  const float condutividade = K *
      (133.42f * powf(tensao25, 3.0f) -
       255.86f * powf(tensao25, 2.0f) +
       857.39f * tensao25);

  DBG("Temperatura: "); DBG(temperatura); DBG(" C | Tensao: ");
  DBG(tensao); DBG(" V | Condutividade: "); DBG(condutividade);
  DBGLN(" uS/cm");

  publishData(temperatura, tensao, condutividade);
}

void setup() {
#if DEBUG_USB
  Serial.begin(115200);
  delay(500);
  Serial.println("\nBOOT OK - TDS/DS18B20 Wi-Fi MQTT");
#endif

  pinMode(pinTDS, INPUT);
  sensorTemp.begin();
  buildIds();
  DBG("Board ID: "); DBGLN(boardId);
  DBG("Topico MQTT: "); DBGLN(mqttTopic);

  setupWiFiRadio();
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setBufferSize(256);
  connectWiFi();
  connectMQTT();

  // Primeira medicao ao entrar no loop.
  lastMeasure = millis() - MEASURE_INTERVAL;
}

void loop() {
  yield();
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWiFiAttempt >= WIFI_RETRY_INTERVAL) {
      lastWiFiAttempt = now;
      DBGLN("Tentando reconectar Wi-Fi...");
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
  } else if (!mqttClient.connected()) {
    if (now - lastMqttAttempt >= MQTT_RETRY_INTERVAL) {
      lastMqttAttempt = now;
      connectMQTT();
    }
  } else {
    mqttClient.loop();
  }

  if (now - lastMeasure >= MEASURE_INTERVAL) {
    lastMeasure = now;
    measureAndPublish();
  }
  delay(10);
}
