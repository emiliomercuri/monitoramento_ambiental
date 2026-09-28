#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SPI.h>
#include <SD.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RTClib.h>

const unsigned long tempo = 1000;
const int pinTemp = D4;   // DS18B20
const int pinTDS  = A0;   // TDS
const int pinSD   = D0;   // CS do cartao SD

const float vRef = 3.2;   // tensao maxima do A0 do Wemos (V)
const float K    = 1.0;   // constante da celula de condutividade

OneWire oneWire(pinTemp);
DallasTemperature sensorTemp(&oneWire);
RTC_DS3231 rtc;

void setup()
{
    Serial.begin(115200);

    WiFi.mode(WIFI_OFF);   // desligar WiFi para economizar bateria

    pinMode(pinTDS, INPUT);

    sensorTemp.begin();

    if (!rtc.begin())
    {
        Serial.println("RTC nao encontrado!");
    }

    // quando verdadeiro, acerta com a data e hora da compilacao
    if (rtc.lostPower())
    {
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    Serial.println("Inicializacao do cartao SD...");

    if (!SD.begin(pinSD))
    {
        Serial.println("Inicializacao falhou !!");
        return;
    }

    File dados = SD.open("dados.txt", FILE_WRITE);

    if (dados)
    {
        dados.println("data_hora\ttemp(C)\ttensao(V)\tcond(uS/cm)");
        dados.close();
    }
}

void loop()
{
    // data e hora
    DateTime agora = rtc.now();
    char dataHora[] = "DD/MM/YYYY hh:mm:ss";
    agora.toString(dataHora);   // troca o modelo pela data e hora atuais

    // temperatura da agua
    sensorTemp.requestTemperatures();
    float temperatura = sensorTemp.getTempCByIndex(0);

    // condutividade
    float soma = 0;
    for (int i = 0; i < 20; i++)
    {
        soma = soma + analogRead(pinTDS);
        delay(10);
    }
    float leitura = soma / 20.0;
    float tensao = leitura * vRef / 1023.0;
    float tensao25 = tensao / (1.0 + 0.02 * (temperatura - 25.0));
    float condutividade = K * (133.42 * pow(tensao25, 3) - 255.86 * pow(tensao25, 2) + 857.39 * tensao25);

    // mostra no monitor serial
    Serial.print(dataHora);
    Serial.print("\t");
    Serial.print(temperatura);
    Serial.print("\t");
    Serial.print(tensao, 3);
    Serial.print("\t");
    Serial.println(condutividade);

    // grava no cartao SD
    File dados = SD.open("dados.txt", FILE_WRITE);

    if (dados)
    {
        dados.print(dataHora);
        dados.print("\t");
        dados.print(temperatura);
        dados.print("\t");
        dados.print(tensao, 3);
        dados.print("\t");
        dados.println(condutividade);
        dados.close();
    }
    else
    {
        Serial.println("problemas com o cartao!");
    }

    delay(tempo);
}
