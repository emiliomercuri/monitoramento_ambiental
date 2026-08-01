# Aula 01 - Introdução ao Raspberry Pi, Linux e Arduino

Aula introdutória da disciplina de Monitoramento Ambiental - Sensores e Microcontroladores. Disciplina baseada e projetos, atividades em grupo. Quantidade de grupos: 10.

Aula a ser ministrada em sala de aula com 10 cabos de rede disponíveis e 10 pontos de energia.

## Materiais

- Arduino UNO + cabo USB
- Raspberry Pi, cartão memória e fonte
- Adaptador VGA - HDMI
- Monitor, teclado, cabo de rede

## Acender e apagar o LED do Arduino Uno usando PlatformIO

A forma mais simples é utilizar o **LED embutido do Arduino Uno** e a função `delay()`.

O identificador da placa Arduino Uno no PlatformIO é:

```text
uno
```

## 1. Criar o projeto pelo terminal

No macOS ou Linux, execute:

```bash
mkdir -p ~/platformio/arduino_led
cd ~/platformio/arduino_led

pio project init --board uno
```

A estrutura criada será semelhante a:

```text
arduino_led/
├── platformio.ini
├── include/
├── lib/
├── src/
└── test/
```

## 2. Configurar o arquivo `platformio.ini`

Crie ou substitua o arquivo `platformio.ini`:

```bash
cat > platformio.ini <<'EOF'
[env:uno]
platform = atmelavr
board = uno
framework = arduino
EOF
```

O conteúdo do arquivo será:

```ini
[env:uno]
platform = atmelavr
board = uno
framework = arduino
```

## 3. Criar o código do Arduino

Neste exemplo, o LED ficará:

- aceso durante **3 segundos**;
- apagado durante **2 segundos**.

Crie o arquivo `src/main.cpp`:

```bash
cat > src/main.cpp <<'EOF'
#include <Arduino.h>

// Tempos em milissegundos
const unsigned long TEMPO_ACESO = 3000;
const unsigned long TEMPO_APAGADO = 2000;

void setup()
{
    // Configura o LED embutido como saída
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
    // Acende o LED
    digitalWrite(LED_BUILTIN, HIGH);
    delay(TEMPO_ACESO);

    // Apaga o LED
    digitalWrite(LED_BUILTIN, LOW);
    delay(TEMPO_APAGADO);
}
EOF
```

O código criado será:

```cpp
#include <Arduino.h>

// Tempos em milissegundos
const unsigned long TEMPO_ACESO = 3000;
const unsigned long TEMPO_APAGADO = 2000;

void setup()
{
    // Configura o LED embutido como saída
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
    // Acende o LED
    digitalWrite(LED_BUILTIN, HIGH);
    delay(TEMPO_ACESO);

    // Apaga o LED
    digitalWrite(LED_BUILTIN, LOW);
    delay(TEMPO_APAGADO);
}
```

A constante `LED_BUILTIN` representa o pino conectado ao LED da própria placa. No Arduino Uno, esse LED normalmente está conectado ao pino digital 13.

## 4. Compilar o código

Dentro da pasta do projeto, execute:

```bash
pio run
```

Quando a compilação terminar corretamente, será exibida uma mensagem semelhante a:

```text
SUCCESS
```

## 5. Identificar a porta USB

Conecte o Arduino Uno ao computador e execute:

```bash
pio device list
```

No macOS, a porta pode aparecer como:

```text
/dev/cu.usbmodem1101
```

Em placas compatíveis que utilizam o conversor USB CH340, pode aparecer como:

```text
/dev/cu.wchusbserial110
```

No Linux, a porta normalmente aparece como:

```text
/dev/ttyACM0
```

ou:

```text
/dev/ttyUSB0
```

## 6. Gravar o código no Arduino Uno

Normalmente, o PlatformIO detecta automaticamente a porta USB:

```bash
pio run -t upload
```

Esse comando compila o programa e envia o firmware para o Arduino.

Caso existam várias portas USB, informe a porta manualmente:

```bash
pio run -t upload --upload-port /dev/cu.usbmodem1101
```

Substitua `/dev/cu.usbmodem1101` pela porta identificada com:

```bash
pio device list
```

## Processo completo em uma única sequência

Todo o projeto pode ser criado, compilado e enviado com os seguintes comandos:

```bash
mkdir -p ~/platformio/arduino_led
cd ~/platformio/arduino_led

pio project init --board uno

cat > platformio.ini <<'EOF'
[env:uno]
platform = atmelavr
board = uno
framework = arduino
EOF

cat > src/main.cpp <<'EOF'
#include <Arduino.h>

const unsigned long TEMPO_ACESO = 3000;
const unsigned long TEMPO_APAGADO = 2000;

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
    digitalWrite(LED_BUILTIN, HIGH);
    delay(TEMPO_ACESO);

    digitalWrite(LED_BUILTIN, LOW);
    delay(TEMPO_APAGADO);
}
EOF

pio run
pio run -t upload
```

## Alterar o tempo do LED

Os tempos são definidos em milissegundos:

```cpp
const unsigned long TEMPO_ACESO = 5000;
const unsigned long TEMPO_APAGADO = 1000;
```

Nesse exemplo:

- `5000` corresponde a 5 segundos;
- `1000` corresponde a 1 segundo.

A conversão utilizada é:

```text
1 segundo = 1000 milissegundos
```
