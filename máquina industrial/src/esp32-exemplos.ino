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
const unsigned long intervaloSerial = 1000;


void setup() {

  Serial.begin(115200);

  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);

  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BTN_FAIL, INPUT_PULLUP);
  pinMode(BTN_RESET, INPUT_PULLUP);

  pinMode(DHTPIN, INPUT_PULLUP);

  Serial.println("Sistema iniciado");
}


void loop() {

  lerSensores();

  lerBotoes();

  lerComandosSerial();

  logicaSistema();

  controlarLEDs();

  enviarStatusSerial();
}


void lerSensores() {


  if (!lerDHT22(&temperaturaReal, &umidade)) {

    temperaturaReal = -1;
    umidade = -1;
  }


  if (!overrideLimite) {

    int leituraLimite = analogRead(POT_LIMITE);

    limiteTemperatura =
      map(leituraLimite, 0, 4095, 0, 100);

  } else {

    limiteTemperatura = limiteManual;
  }


  if (!overrideTemp) {

    int leituraTemp = analogRead(POT_TEMP);

    temperaturaSimulada =
      map(leituraTemp, 0, 4095, 0, 100);

  } else {

    temperaturaSimulada = tempManual;
  }
}


void lerBotoes() {

  if (digitalRead(BTN_START) == LOW) {

    if (estadoAtual == PARADO) {

      estadoAtual = SUCESSO;

      Serial.println("START acionado");
    }

    delay(200);
  }

  if (digitalRead(BTN_FAIL) == LOW) {

    estadoAtual = FALHA;

    Serial.println("Falha forçada");

    delay(200);
  }

  if (digitalRead(BTN_RESET) == LOW) {

    estadoAtual = PARADO;

    Serial.println("Sistema resetado");

    delay(200);
  }
}


void logicaSistema() {

  if (estadoAtual == SUCESSO) {

    if (temperaturaSimulada > limiteTemperatura) {

      estadoAtual = FALHA;

      Serial.println("FALHA: temperatura acima do limite");
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


void lerComandosSerial() {

  if (Serial.available()) {

    String comando =
      Serial.readStringUntil('\n');

    comando.trim();

    comando.toUpperCase();

    if (comando == "START") {

      estadoAtual = SUCESSO;

      Serial.println("START via serial");
    }

    else if (comando == "RESET") {

      estadoAtual = PARADO;

      Serial.println("RESET via serial");
    }

    else if (comando == "FAIL") {

      estadoAtual = FALHA;

      Serial.println("FAIL via serial");
    }

    else if (comando == "STATUS") {

      imprimirStatusCompleto();
    }

    else if (comando.startsWith("SETLIMITE=")) {

      String valor =
        comando.substring(11);

      limiteManual =
        valor.toFloat();

      overrideLimite = true;

      Serial.print("Novo limite: ");
      Serial.println(limiteManual);
    }

    else if (comando.startsWith("SETTEMP=")) {

      String valor =
        comando.substring(8);

      tempManual =
        valor.toFloat();

      overrideTemp = true;

      Serial.print("Nova temperatura: ");
      Serial.println(tempManual);
    }

    else if (comando == "AUTO") {

      overrideLimite = false;
      overrideTemp = false;

      Serial.println("Controle voltou aos potenciometros");
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


void imprimirStatusCompleto() {

  Serial.println("===== STATUS =====");

  Serial.print("Estado: ");
  Serial.println(obterEstadoTexto());

  Serial.print("Temperatura DHT22: ");
  Serial.println(temperaturaReal);

  Serial.print("Umidade: ");
  Serial.println(umidade);

  Serial.print("Temperatura simulada: ");
  Serial.println(temperaturaSimulada);

  Serial.print("Limite: ");
  Serial.println(limiteTemperatura);

  Serial.println("==================");
}

String obterEstadoTexto() {

  switch (estadoAtual) {

    case PARADO:
      return "PARADO";

    case SUCESSO:
      return "SUCESSO";

    case FALHA:
      return "FALHA";
  }

  return "DESCONHECIDO";
}

bool lerDHT22(float *temp, float *hum) {

  uint8_t data[5] = {0, 0, 0, 0, 0};

  pinMode(DHTPIN, OUTPUT);

  digitalWrite(DHTPIN, LOW);

  delayMicroseconds(1100);

  digitalWrite(DHTPIN, HIGH);

  delayMicroseconds(30);

  pinMode(DHTPIN, INPUT_PULLUP);

  unsigned long timeout = micros();

  while (digitalRead(DHTPIN) == HIGH) {
    if (micros() - timeout > 100) return false;
  }

  timeout = micros();

  while (digitalRead(DHTPIN) == LOW) {
    if (micros() - timeout > 100) return false;
  }

  timeout = micros();

  while (digitalRead(DHTPIN) == HIGH) {
    if (micros() - timeout > 100) return false;
  }


  for (int i = 0; i < 40; i++) {

    while (digitalRead(DHTPIN) == LOW);

    unsigned long inicio = micros();

    while (digitalRead(DHTPIN) == HIGH);

    unsigned long duracao =
      micros() - inicio;

    data[i / 8] <<= 1;

    if (duracao > 40) {
      data[i / 8] |= 1;
    }
  }


  uint8_t checksum =
    data[0] + data[1] +
    data[2] + data[3];

  if (checksum != data[4]) {
    return false;
  }

  *hum =
    ((data[0] << 8) | data[1]) * 0.1;

  int16_t rawTemp =
    ((data[2] & 0x7F) << 8) | data[3];

  *temp = rawTemp * 0.1;

  if (data[2] & 0x80) {
    *temp *= -1;
  }

  return true;
}