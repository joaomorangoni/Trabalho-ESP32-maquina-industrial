#define LED_VERMELHO 23
#define LED_VERDE    22

#define BTN_START    21
#define BTN_FAIL     19
#define BTN_RESET    18

#define POT_LIMITE   34
#define POT_TEMP     35

#define DHTPIN       12

enum EstadoMaquina {
  PARADO,
  SUCESSO,
  FALHA
};

EstadoMaquina estadoAtual = PARADO;

float temperaturaReal = 0;
float umidade = 0;

float temperaturaSimulada = 0;
float limiteTemperatura = 0;

bool overrideLimite = false;
bool overrideTemp = false;

float limiteManual = 0;
float tempManual = 0;

unsigned long ultimoEnvio = 0;
unsigned long ultimoDHT = 0;
unsigned long ultimoDebounce = 0;

const unsigned long intervaloSerial = 1000;

bool esperaNivel(int nivel, unsigned long timeout) {
  unsigned long inicio = micros();
  while (digitalRead(DHTPIN) == nivel) {
    if (micros() - inicio > timeout) return false;
  }
  return true;
}

bool lerDHT22(float *temp, float *hum) {
  uint8_t data[5] = {0, 0, 0, 0, 0};

  pinMode(DHTPIN, OUTPUT);
  digitalWrite(DHTPIN, LOW);
  delay(2);
  digitalWrite(DHTPIN, HIGH);
  delayMicroseconds(30);
  pinMode(DHTPIN, INPUT_PULLUP);

  if (!esperaNivel(HIGH, 100)) return false;
  if (!esperaNivel(LOW, 100)) return false;
  if (!esperaNivel(HIGH, 100)) return false;

  for (int i = 0; i < 40; i++) {
    if (!esperaNivel(LOW, 70)) return false;

    unsigned long inicio = micros();

    if (!esperaNivel(HIGH, 100)) return false;

    unsigned long duracao = micros() - inicio;

    data[i / 8] <<= 1;

    if (duracao > 40) data[i / 8] |= 1;
  }

  uint8_t checksum = data[0] + data[1] + data[2] + data[3];
  if (checksum != data[4]) return false;

  *hum = ((data[0] << 8) | data[1]) * 0.1;

  int16_t rawTemp = ((data[2] & 0x7F) << 8) | data[3];
  *temp = rawTemp * 0.1;

  if (data[2] & 0x80) *temp *= -1;

  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("");
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);

  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BTN_FAIL, INPUT_PULLUP);
  pinMode(BTN_RESET, INPUT_PULLUP);

  pinMode(DHTPIN, INPUT_PULLUP);
}

void lerSensores() {
  if (millis() - ultimoDHT > 2000) {
    ultimoDHT = millis();

    noInterrupts();
    bool ok = lerDHT22(&temperaturaReal, &umidade);
    interrupts();

    if (!ok) {
      temperaturaReal = -1;
      umidade = -1;
    }
  }

  if (!overrideLimite) {
    int leituraLimite = analogRead(POT_LIMITE);
    limiteTemperatura = leituraLimite * 100.0 / 4095.0;
  } else {
    limiteTemperatura = limiteManual;
  }

  if (!overrideTemp) {
    int leituraTemp = analogRead(POT_TEMP);
    temperaturaSimulada = leituraTemp * 100.0 / 4095.0;
  } else {
    temperaturaSimulada = tempManual;
  }
}

void lerBotoes() {
  if (millis() - ultimoDebounce < 200) return;

  if (digitalRead(BTN_START) == LOW) {
    if (estadoAtual == PARADO) estadoAtual = SUCESSO;
    ultimoDebounce = millis();
  }

  if (digitalRead(BTN_FAIL) == LOW) {
    estadoAtual = FALHA;
    ultimoDebounce = millis();
  }

  if (digitalRead(BTN_RESET) == LOW) {
    estadoAtual = PARADO;
    ultimoDebounce = millis();
  }
}

void logicaSistema() {
  if (estadoAtual == SUCESSO) {
    if (temperaturaSimulada > limiteTemperatura) {
      estadoAtual = FALHA;
    }
  }
}

void controlarLEDs() {
  switch (estadoAtual) {
    case PARADO:
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_VERMELHO, LOW);
      break;

    case SUCESSO:
      digitalWrite(LED_VERDE, HIGH);
      digitalWrite(LED_VERMELHO, LOW);
      break;

    case FALHA:
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_VERMELHO, HIGH);
      break;
  }
}

String obterEstadoTexto() {
  switch (estadoAtual) {
    case PARADO: return "PARADO";
    case SUCESSO: return "SUCESSO";
    case FALHA: return "FALHA";
  }
  return "DESCONHECIDO";
}

void imprimirStatusCompleto() {
  Serial.println("===== STATUS =====");
  Serial.print("Estado: ");
  Serial.println(obterEstadoTexto());
  Serial.print("Temperatura: ");
  Serial.println(temperaturaReal);
  Serial.print("Umidade: ");
  Serial.println(umidade);
  Serial.print("Temp Simulada: ");
  Serial.println(temperaturaSimulada);
  Serial.print("Limite: ");
  Serial.println(limiteTemperatura);
  Serial.println("==================");
}

void lerComandosSerial() {
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();
    comando.toUpperCase();

    if (comando == "START") estadoAtual = SUCESSO;
    else if (comando == "RESET") estadoAtual = PARADO;
    else if (comando == "FAIL") estadoAtual = FALHA;
    else if (comando == "STATUS") imprimirStatusCompleto();
    else if (comando.startsWith("SETLIMITE=")) {
      limiteManual = comando.substring(11).toFloat();
      overrideLimite = true;
    }
    else if (comando.startsWith("SETTEMP=")) {
      tempManual = comando.substring(8).toFloat();
      overrideTemp = true;
    }
    else if (comando == "AUTO") {
      overrideLimite = false;
      overrideTemp = false;
    }
  }
}

void enviarStatusSerial() {
  if (millis() - ultimoEnvio >= intervaloSerial) {
    ultimoEnvio = millis();

    Serial.print("M1,");
    Serial.print(obterEstadoTexto());
    Serial.print(",");
    Serial.print(temperaturaReal);
    Serial.print(",");
    Serial.print(temperaturaSimulada);
    Serial.print(",");
    Serial.println(limiteTemperatura);
  }
}

void loop() {
  lerSensores();
  lerBotoes();
  lerComandosSerial();
  logicaSistema();
  controlarLEDs();
  enviarStatusSerial();
}