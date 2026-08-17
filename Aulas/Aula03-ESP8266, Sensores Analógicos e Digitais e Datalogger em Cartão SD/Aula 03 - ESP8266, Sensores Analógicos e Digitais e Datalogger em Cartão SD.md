# Aula 03 - ESP8266, Sensores Analógicos e Digitais e Datalogger em Cartão SD

Terceira aula da disciplina de Monitoramento Ambiental - Sensores e Microcontroladores. Continuação das Aulas 01 e 02, mantendo o trabalho em grupos (10 grupos) e o fluxo de desenvolvimento com **PlatformIO** a partir do Raspberry Pi acessado via SSH.

Até aqui todo o trabalho foi feito no **Arduino Uno**. Nesta aula trocamos de microcontrolador e passamos a montar um sistema de monitoramento com o microcontrolador ESO8266:

- migração para o **ESP8266** (NodeMCU e Wemos D1 mini) e suas diferenças em relação ao Arduino;
- leitura de **sensores analógicos** (`analogRead`) e **digitais**;
- **datalogger**: gravação das leituras em um **cartão SD** via **SPI**;
- controle do LED;

## Materiais

- NodeMCU V3 (ESP8266) + cabo micro-USB
- Wemos D1 mini (ESP8266), para o exemplo 3
- Raspberry Pi
- Protoboard e jumpers
- 1 sensor analógico (umidade de solo, LDR, MQ-x, etc.)
- 1 sensor **DHT11** ou **DHT22/AM2302**, para o exemplo 3
- 1 LED vermelho + resistor de 220 Ω
- Módulo leitor de cartão **microSD** (SPI) + cartão microSD formatado em **FAT32**

---

## 1. Acesso ao Raspberry Pi e preparação da aula

Como nas aulas anteriores, acesse o Raspberry Pi via SSH:

```bash
ssh nome@ip
```

Senha:

```text
iot***
```

Crie a pasta desta aula (todos os exercícios ficarão dentro dela):

```bash
mkdir aula03
cd aula03
```

---

### Mapa de pinos do NodeMCU / D1 mini

No código usamos os nomes `D0`…`D8` (já definidos pelo *core* do ESP8266), não os números de GPIO.

| Nome | GPIO | Observações |
|---|---|---|
| `D0` | 16 | **Não tem PWM** e não tem interrupção; usado para acordar do *deep sleep* |
| `D1` | 5 | SCL (I2C) |
| `D2` | 4 | SDA (I2C) |
| `D3` | 0 | *Pull-up* no boot — não pode estar em LOW ao ligar |
| `D4` | 2 | LED interno; *pull-up* no boot |
| `D5` | 14 | **SCK** (SPI) |
| `D6` | 12 | **MISO** (SPI) |
| `D7` | 13 | **MOSI** (SPI) |
| `D8` | 15 | **CS/SS** (SPI) — *pull-down* no boot |
| `A0` | ADC | Entrada analógica, **0 a 3,3 V**, leitura de 0 a 1023 |

---

## 3. Exemplo 1 — Sensor analógico controlando um LED (NodeMCU)

**Objetivo:** ler um sensor analógico no NodeMCU, mostrar o valor no Monitor Serial e acender um LED quando a leitura passar de um limiar "crítico".

### Circuito

- **Sensor analógico:** sinal (`S`) em `A0`, `+` em `3V3`/`5V`(VV) e `-` em `GND`
- **LED:** perna maior (ânodo) em `D0`, em série com um resistor de **220 Ω**; perna menor (cátodo) no `GND`

> O resistor limita a corrente. Lembre que no ESP8266 o limite por pino é de apenas **12 mA**, o resistor de 220 Ω já mantém a corrente bem abaixo disso.

### Criando o projeto

```bash
mkdir ex1
cd ex1

pio init -b nodemcuv2
```

### Identificadores de placas no PlatformIO

| Placa | Identificador (`board`) |
|-------|-------------------------|
| Arduino Uno | `uno` |
| Arduino Nano (ATmega328P) | `nanoatmega328` |
| Arduino Pro Mini | `pro8MHzatmega328` ou `pro16MHzatmega328` |
| Arduino Mega 2560 | `megaatmega2560` |
| Wemos D1 Mini (ESP8266) | `d1_mini` |
| NodeMCU 1.0 (ESP-12E) | `nodemcuv2` |
| NodeMCU 0.9 (ESP-12) | `nodemcu` |
| NodeMCU-32S (ESP32) | `esp32vn-iot-uno` |
| Heltec WiFi LoRa 32 V3/V4 | `heltec_wifi_lora_32_V3` |
| Raspberry Pi Pico | `pi_pico` |

> **Heltec V4:** utilize o identificador da V3 e aumente a memória para 16 MB no `platformio.ini`.

### `platformio.ini`

Agora, com o NodeMCU, o arquivo platformio.ini passa a ser:

```ini
[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2
framework = arduino
monitor_speed = 115200
```

Note que já definimos o `monitor_speed = 115200`, evitando ter que especificar `--baud 115200` toda vez que abrir o monitor

Para **ver** o conteúdo do arquivo:

```bash
cat platformio.ini
```

Para **editar**:

```bash
micro platformio.ini
```

### Código

Entre na pasta `src` e crie/abra o arquivo `main.cpp`:

```bash
cd src
micro main.cpp
```

```cpp
#include <Arduino.h>

const unsigned long tempo = 3000;
const int pinRed = D0;
const int pinSen = A0;

void setup()
{
    Serial.begin(9600);

    pinMode(pinRed, OUTPUT);
    pinMode(pinSen, INPUT);
}

void loop()
{
    int sensor = analogRead(pinSen);

    if (sensor > 400)
    {
        digitalWrite(pinRed, HIGH);   // condição crítica: acende
    }
    else
    {
        digitalWrite(pinRed, LOW);    // condição normal: apaga
    }

    Serial.println(sensor);

    delay(tempo);
}
```


```bash
pio run -t upload
pio device monitor
```

---

## 4. Exemplo 2 — Datalogger: salvando os dados em cartão SD

Agora acrescentamos um módulo de cartão microSD, que se comunica por **SPI**.

Como o `ex2` reaproveita todo o `ex1`, copie a pasta inteira (na pasta da aula):

```bash
cd ..
cp -r ex1 ex2
cd ex2
```

### Circuito do módulo SD (NodeMCU)

| Módulo SD | NodeMCU | Sinal |
|---|---|---|
| `CS` / `SS` | `D8` | seleção do dispositivo |
| `MOSI` | `D7` | dados: mestre → escravo |
| `MISO` | `D6` | dados: escravo → mestre |
| `SCK` / `CLK` | `D5` | *clock* |
| `VCC` | `3V3` (ou `5V`, ver abaixo) | alimentação |
| `GND` | `GND` | referência |

> **Alimentação:** o cartão SD funciona em 5 V (pino deve ser conectado ao VV do NodeMCU). Alguns módulos podem operar em `3V3`. Alimentar um módulo de 5 V com 3,3 V resulta em falha de inicialização.


### Código

```bash
micro src/main.cpp
```

```cpp
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

const unsigned long tempo = 3000;
const int pinRed = D0;
const int pinSen = A0;

const int pinSD = D8;   // CS do cartão SD

void setup()
{
    Serial.begin(115200);

    pinMode(pinRed, OUTPUT);
    pinMode(pinSen, INPUT);

    Serial.println("Inicializacao do cartao SD...");

    if (!SD.begin(pinSD))
    {
        Serial.println("Inicializacao falhou !!");
        return;
    }

    File dados = SD.open("dados.txt", FILE_WRITE);

    if (dados)
    {
        dados.println("Dados de umidade em (unidades binarias)");
        dados.close();
    }
}

void loop()
{
    int sensor = analogRead(pinSen);

    if (sensor > 400)
    {
        digitalWrite(pinRed, HIGH);
    }
    else
    {
        digitalWrite(pinRed, LOW);
    }

    Serial.println(sensor);

    File dados = SD.open("dados.txt", FILE_WRITE);

    if (dados)
    {
        dados.println(sensor);
        dados.close();
    }
    else
    {
        Serial.println("problemas com o cartao!");
    }

    delay(tempo);
}
```


```bash
pio run -t upload
pio device monitor
```

### Modos de abertura do arquivo

O segundo argumento de `SD.open(nome, modo)` define como o arquivo é aberto. No ESP8266 podem ser usadas as constantes ou as strings equivalentes:

| Modo | String | O que faz |
|---|---|---|
| `FILE_READ` | `"r"` | somente leitura; falha se o arquivo não existir |
| `FILE_WRITE` | `"a+"` | leitura e escrita; cria se não existir e grava **sempre no final** (*append*), preservando o conteúdo anterior |
| — | `"w"` | somente escrita; cria o arquivo e **apaga** o conteúdo anterior |
| — | `"a"` | somente escrita no final (*append*); cria se não existir |
| — | `"r+"` | leitura e escrita a partir do início; **não** cria o arquivo |
| — | `"w+"` | leitura e escrita; cria e **apaga** o conteúdo anterior |

Se o segundo argumento for omitido, o padrão é `FILE_READ`.

Para um datalogger queremos **`FILE_WRITE`**: cada nova leitura é acrescentada ao fim do arquivo.

> **Atenção:** no **ESP32** a mesma constante `FILE_WRITE` **apaga** o arquivo, lá o *append* é feito com `FILE_APPEND`.


---

## 5. Exemplo 3 — Wemos D1 mini e sensor DHT (temperatura e umidade)

**Objetivo:** o mesmo sistema (sensor + LED + datalogger), agora no **Wemos D1 mini** com um **sensor DHT** medindo temperatura e umidade do ar.

O D1 mini usa o **mesmo chip ESP8266** do NodeMCU, então a fiação do cartão SD **não muda em nada**. Muda apenas o pino do LED:


```bash
cd ..
cp -r ex2 ex3
cd ex3
```

### `platformio.ini`

```bash
micro platformio.ini
```

```ini
[env:d1_mini]
platform = espressif8266
board = d1_mini
framework = arduino
monitor_speed = 115200

lib_deps =
    adafruit/DHT sensor library
    adafruit/Adafruit Unified Sensor
```

Até agora só usamos bibliotecas que já vêm com o *core* (`SPI`, `SD`). O DHT precisa de uma biblioteca externa, e é o `lib_deps` que diz isso ao PlatformIO, ele **baixa e instala sozinho** na primeira compilação, dentro da pasta do projeto. Não é preciso rodar nenhum comando de instalação.

### Circuito

- **LED:** ânodo em `D2` com resistor de 220 Ω; cátodo no `GND`
- **Sensor analógico:** sinal em `A0`, `+` em `3V3`/`5V`, `-` em `GND`
- **Sensor DHT:** `DATA`/`OUT`/`S` em `D1`, `+`/`VCC` em `3V3`, `-`/`GND` em `GND`
- **Cartão SD:** `D8`/`D7`/`D6`/`D5` — igual ao Exemplo 2


O DHT usa um protocolo próprio de 1 fio, em que o microcontrolador dá um pulso de início e o sensor responde com 40 bits contendo temperatura e umidade **já convertidas em unidades físicas** (°C e %). Não precisa converter, o valor já vem pronto. Quem cuida dessa conversa de bits é a biblioteca.


### Código

```bash
micro src/main.cpp
```

```cpp
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <DHT.h>

const unsigned long tempo = 3000;
const int pinRed = D0;   // LED 
const int pinSen = A0;   // sensor analogico
const int pinDHT = D1;   // sensor DHT (temperatura e umidade)

const int pinSD = D8;

#define TIPO_DHT DHT11

DHT dht(pinDHT, TIPO_DHT);

int contSegundos = 0;

void setup()
{
    Serial.begin(115200);
    delay(2000);

    pinMode(pinRed, OUTPUT);
    pinMode(pinSen, INPUT);

    dht.begin();

    Serial.println("Inicializacao do cartao SD...");

    if (!SD.begin(pinSD))
    {
        Serial.println("Inicializacao falhou !!");
        return;
    }

    File dados = SD.open("dados.txt", FILE_WRITE);

    if (dados)
    {
        dados.println("tempo(s)\tanalogico\ttemp(C)\tumid(%)");
        dados.close();
    }
}

void loop()
{
    int sensor = analogRead(pinSen);            // 0 a 1023

    float temperatura = dht.readTemperature();  // em graus Celsius
    float umidade     = dht.readHumidity();     // em porcentagem

    if (isnan(temperatura) || isnan(umidade))
    {
        Serial.println("Falha na leitura do DHT!");
        contSegundos = contSegundos + tempo / 1000;
        delay(tempo);
        return;
    }


    if (temperatura > 30.0)
    {
        digitalWrite(pinRed, HIGH);
    }

    Serial.print(contSegundos);
    Serial.print("\t");
    Serial.print(sensor);
    Serial.print("\t");
    Serial.print(temperatura);
    Serial.print("\t");
    Serial.println(umidade);

    File dados = SD.open("dados.txt", FILE_WRITE);

    if (dados)
    {
        dados.print(contSegundos);
        dados.print("\t");
        dados.print(sensor);
        dados.print("\t");
        dados.print(temperatura);
        dados.print("\t");
        dados.println(umidade);
        dados.close();
    }
    else
    {
        Serial.println("problemas com o cartao!");
    }

    contSegundos = contSegundos + tempo / 1000;

    delay(tempo);
}
```

### DHT11 ou DHT22?

Os dois módulos são idênticos na ligação e no código

| | DHT11 | DHT22 / AM2302 |
|---|---|---|
| Cor do sensor | azul | branco |
| Temperatura | 0 a 50 °C (±2 °C) | -40 a 80 °C (±0,5 °C) |
| Umidade | 20 a 90 % (±5 %) | 0 a 100 % (±2 %) |
| Resolução | 1 °C / 1 % | 0,1 °C / 0,1 % |
| Intervalo mínimo entre leituras | 1 s | **2 s** |


```bash
pio run -t upload
pio device monitor
```

## Exercício desafio — Acionando uma lâmpada de 110 V com relé

Desafio: repetir o sistema do Exemplo 3, mas **substituir o LED por uma lâmpada de 110 V**, controlada por um **módulo relé**. 

### Segurança em primeiro lugar

> **A montagem da parte de 110 V é feita apenas com a bancada desenergizada e sob supervisão do professor.**
>
> - Nunca conecte ou desconecte fios com o cabo na tomada.
> - Todo o lado de alta tensão do relé deve ficar **isolado** (fita isolante ou terminais cobertos) — nada de fio descascado na protoboard.
> - **A parte de 110 V nunca passa pela protoboard nem pelos jumpers**; use cabo apropriado direto nos bornes do relé.
> - O relé deve chavear o **fio fase**, não o neutro.
> - Confira toda a montagem com o professor **antes** de energizar.

### O relé

Um relé é um interruptor acionado eletricamente: um pino do ESP energiza uma bobina (lado de baixa tensão) que fecha um contato mecânico (lado de alta tensão). Os dois lados ficam **eletricamente isolados**, e é isso que torna seguro um pino de 3,3 V comandar uma lâmpada de 110 V.

**Lado de controle (baixa tensão):**

| Módulo relé | ESP8266 |
|---|---|
| `VCC` | `5V` (pino `VIN`/`5V` do D1 mini) |
| `GND` | `GND` |
| `IN` | `D0` (o mesmo pino que controlava o LED) |

**Lado de potência (alta tensão)** — o relé tem três bornes:

- `COM` (comum): recebe o **fase** da tomada
- `NO` (*normally open*): vai para a lâmpada — fechado **somente** quando o relé é acionado
- `NC` (*normally closed*): fechado quando o relé está em repouso

Use `COM` + `NO`, para que a lâmpada fique **apagada** se a placa desligar ou travar. O **neutro** vai direto da tomada para a lâmpada, sem passar pelo relé.

### O que muda no código

1. O relé é uma saída **digital** — não há meio-termo. 
2. Muitos módulos relé são **ativos em LOW**: `digitalWrite(pinRele, LOW)` **liga** a lâmpada. Teste antes com o relé desconectado da rede elétrica, dá para ouvir o clique e ver o LED do módulo.
3. **Não deixe o relé chaveando a cada leitura.** Se o sensor ficar oscilando em torno do limiar, o relé bate sem parar e queima. 

### Dicas

- Comece **sem a lâmpada**: monte só o lado de controle e confirme pelo clique e pelo LED do módulo que o acionamento está correto.
- O relé precisa de mais corrente do que um pino do ESP fornece (limite de 12 mA). Módulos com **optoacoplador** já resolvem isso; se o seu relé não acionar de forma confiável, esse é o motivo.
- Verifique a **corrente máxima** impressa no relé (tipicamente 10 A a 250 V) e confira se a carga cabe nela.
- Registre no cartão SD também o **estado do relé** (0 ou 1), não só a leitura do sensor.