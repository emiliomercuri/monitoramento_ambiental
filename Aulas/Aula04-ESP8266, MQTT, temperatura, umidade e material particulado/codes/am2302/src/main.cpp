#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

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

const char* MQTT_TOPIC_BASE = "sensores/am2302";

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
// AM2302 (DHT22)
// AM2302 DATA -> Wemos D4/GPIO2 (com resistor de pull-up 4k7~10k para 3V3)
// ======================================================
#define DHTPIN D4
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

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
// O DHT22/AM2302 tem taxa de amostragem maxima de ~0.5Hz (1 leitura a cada 2s)
const unsigned long PUBLISH_INTERVAL = 2000;

// ======================================================
// FUNCOES AUXILIARES
// ======================================================
void buildIds() {
  uint32_t chipId = ESP.getChipId();

  snprintf(boardId, sizeof(boardId), "ESP-%06X", chipId);
  snprintf(mqttClientId, sizeof(mqttClientId), "am2302-%s", boardId);
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
// PUBLICACAO DOS DADOS
// ======================================================
void publishAM2302Data() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    DBGLN("Falha na leitura do AM2302.");
    return;
  }

  char payload[256];

  snprintf(
    payload,
    sizeof(payload),
    "{"
      "\"timestamp_ms\":%lu,"
      "\"sensor_type\":\"am2302\","
      "\"board_id\":\"%s\","
      "\"temperature_c\":%.1f,"
      "\"humidity_pct\":%.1f,"
      "\"rssi\":%d"
    "}",
    millis(),
    boardId,
    temperature,
    humidity,
    WiFi.RSSI()
  );

  bool ok = mqttClient.publish(mqttTopic, payload, false);

  DBG("Temp: ");
  DBG(temperature);
  DBG(" C  Umidade: ");
  DBG(humidity);
  DBG(" %  MQTT: ");
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
  Serial.println("BOOT OK - AM2302 Wi-Fi MQTT");
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
  dht.begin();
  DBGLN("AM2302 iniciado.");
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

  unsigned long now = millis();

  if (mqttClient.connected() && now - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = now;
    publishAM2302Data();
  }
}
