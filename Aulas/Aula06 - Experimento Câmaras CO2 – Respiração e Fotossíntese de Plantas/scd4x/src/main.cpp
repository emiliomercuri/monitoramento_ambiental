#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DFRobot_SCD4X.h>

// ======================================================
// CONFIGURACOES Wi-Fi
// ======================================================
const char* WIFI_SSID = "plant";
const char* WIFI_PASS = "iotempire";

// ======================================================
// CONFIGURACOES MQTT
// ======================================================
const char* MQTT_HOST = "192.168.4.1";
const uint16_t MQTT_PORT = 1883;

const char* MQTT_TOPIC_BASE = "sensores/scd41";

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
// SCD41 (I2C)
// SDA -> Wemos D2/GPIO4
// SCL -> Wemos D1/GPIO5
// ======================================================
DFRobot_SCD4X SCD4X(&Wire, /*i2cAddr = */SCD4X_I2C_ADDR);

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

unsigned long lastWiFiAttempt = 0;
const unsigned long WIFI_RETRY_INTERVAL = 15000;

unsigned long lastPublish = 0;
// O SCD41 atualiza a cada ~5 s no modo periodico
const unsigned long PUBLISH_INTERVAL = 5000;

// ======================================================
// FUNCOES AUXILIARES
// ======================================================
void buildIds() {
  uint32_t chipId = ESP.getChipId();

  snprintf(boardId, sizeof(boardId), "ESP-%06X", chipId);
  snprintf(mqttClientId, sizeof(mqttClientId), "scd41-%s", boardId);
  snprintf(mqttTopic, sizeof(mqttTopic), "%s/%s", MQTT_TOPIC_BASE, boardId);
}

void setupWiFiRadio() {
  WiFi.persistent(false);

  WiFi.mode(WIFI_OFF);
  delay(2000);

  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  // Potencia do radio (max ~20.5 dBm). Se a fonte/USB for fraca e o ESP
  // resetar ao conectar, reduza para 17 ou menos.
  WiFi.setOutputPower(17.0);

  // Modo mais estavel para ESP8266
  WiFi.setPhyMode(WIFI_PHY_MODE_11G);

  WiFi.setAutoReconnect(true);

  delay(1000);
}

void debugScanNetworks() {
#if DEBUG_USB
  DBGLN("Escaneando redes Wi-Fi...");

  int n = WiFi.scanNetworks();

  if (n <= 0) {
    DBGLN("Nenhuma rede encontrada!");
  } else {
    for (int i = 0; i < n; i++) {
      DBG("  ");
      DBG(WiFi.SSID(i));
      DBG("  RSSI: ");
      DBG(WiFi.RSSI(i));
      DBG(" dBm  canal: ");
      DBG(WiFi.channel(i));
      DBG(WiFi.encryptionType(i) == ENC_TYPE_NONE ? "  (aberta)" : "");
      DBGLN("");
    }
  }

  WiFi.scanDelete();
#endif
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
// CONFIGURACAO DO SCD41
// ======================================================
void setupSCD41() {
  // Init do sensor
  while (!SCD4X.begin()) {
    DBGLN("Falha na comunicacao com o SCD41, verifique a conexao");
    delay(3000);
  }
  DBGLN("SCD41 encontrado.");

  // O sensor precisa estar fora do modo de medicao para configurar
  SCD4X.enablePeriodMeasure(SCD4X_STOP_PERIODIC_MEASURE);
  delay(500);

  // Offset de temperatura (ajuste conforme seu ambiente)
  SCD4X.setTempComp(4.0);

  float temp = SCD4X.getTempComp();
  DBG("Compensacao de temperatura atual: ");
  DBG(temp);
  DBGLN(" C");

  // Altitude do local (Curitiba ~935 m, ajuste conforme necessario)
  SCD4X.setSensorAltitude(540);

  uint16_t altitude = SCD4X.getSensorAltitude();
  DBG("Altitude configurada: ");
  DBG(altitude);
  DBGLN(" m");

  // Inicia medicao periodica (atualiza a cada ~5 s)
  SCD4X.enablePeriodMeasure(SCD4X_START_PERIODIC_MEASURE);

  DBGLN("SCD41 iniciado em modo de medicao periodica.");
}

// ======================================================
// IMPRESSAO DOS DADOS (sempre, com ou sem Wi-Fi/MQTT)
// ======================================================
void printSCD41Data(const DFRobot_SCD4X::sSensorMeasurement_t& data) {
  DBG("CO2: ");
  DBG(data.CO2ppm);
  DBG(" ppm  Temp: ");
  DBG(data.temp);
  DBG(" C  Umidade: ");
  DBG(data.humidity);
  DBGLN(" %RH");
}

// ======================================================
// PUBLICACAO DOS DADOS
// ======================================================
void publishSCD41Data(const DFRobot_SCD4X::sSensorMeasurement_t& data) {
  char payload[256];

  snprintf(
    payload,
    sizeof(payload),
    "{"
      "\"timestamp_ms\":%lu,"
      "\"sensor_type\":\"scd41\","
      "\"board_id\":\"%s\","
      "\"co2_ppm\":%u,"
      "\"temperature_c\":%.2f,"
      "\"humidity_rh\":%.2f,"
      "\"rssi\":%d"
    "}",
    millis(),
    boardId,
    data.CO2ppm,
    data.temp,
    data.humidity,
    WiFi.RSSI()
  );

  bool ok = mqttClient.publish(mqttTopic, payload, false);

  DBG("MQTT publish: ");
  DBG(ok ? "OK" : "FALHOU");
  DBG("  topico: ");
  DBGLN(mqttTopic);
}

// ======================================================
// SETUP
// ======================================================
void setup() {
#if DEBUG_USB
  Serial.begin(115200);
  delay(5000);

  Serial.println();
  Serial.println("BOOT OK - SCD41 Wi-Fi MQTT");
#endif

  buildIds();

  DBG("Board ID: ");
  DBGLN(boardId);

  DBG("Topico MQTT: ");
  DBGLN(mqttTopic);

  setupWiFiRadio();

  debugScanNetworks();

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  connectWiFi();
  connectMQTT();

  // Inicia o SCD41 depois do Wi-Fi/MQTT
  setupSCD41();
}

// ======================================================
// LOOP
// ======================================================
void loop() {
  yield();

  unsigned long now = millis();

  // Reconexao Wi-Fi sem travar o loop por 30 s a cada tentativa
  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWiFiAttempt >= WIFI_RETRY_INTERVAL) {
      lastWiFiAttempt = now;
      DBGLN("Wi-Fi desconectado, tentando novamente...");
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

  // Leitura e impressao acontecem SEMPRE, com ou sem rede
  if (SCD4X.getDataReadyStatus()) {
    DFRobot_SCD4X::sSensorMeasurement_t data;
    SCD4X.readMeasurement(&data);

    printSCD41Data(data);

    if (mqttClient.connected() && now - lastPublish >= PUBLISH_INTERVAL) {
      lastPublish = now;
      publishSCD41Data(data);
    }
  }

  delay(100);
}
