#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <RTClib.h>
#include <string.h>

const int PINO_LED  = 13;
const int PINO_TRIG = 8;
const int PINO_ECHO = 9;
const int PINO_PIR  = 7;
const int DISTANCIA_LIMITE = 2;

const int MOTOR_IN1 = 4;
const int MOTOR_IN2 = 5;
const int MOTOR_IN3 = 6;
const int MOTOR_IN4 = 12;

const int PASSOS_TRANCA   = 512;
const int ATRASO_PASSO_MS = 3;

const uint8_t SEQUENCIA_PASSOS[4][4] PROGMEM = {
    {1, 0, 0, 0},
    {0, 1, 0, 0},
    {0, 0, 1, 0},
    {0, 0, 0, 1}
};
const uint8_t N_ESTADOS = 4;

const String WIFI_SSID = "Torres";
const String WIFI_PASS = "Mftl552912";

const String SERVIDOR_IP    = "192.168.99.4";
const int    SERVIDOR_PORTA = 5000;

const unsigned long INTERVALO_ENVIO = 3000;

SoftwareSerial esp8266(10, 11);
RTC_DS3231 rtc;

bool  trancada           = false;
int   distancia          = 0;
bool  temPresenca        = false;
bool  portaAberta        = false;
bool  portaFechada       = false;
bool  obstruida          = false;

unsigned long tempoAnteriorPisca    = 0;
unsigned long tempoAnteriorEnvio    = 0;
unsigned long tempoAnteriorSensores = 0;
unsigned long tempoAnteriorSerial   = 0;
bool estadoLedPisca = LOW;

void    configurarHardware();
boolean enviarComandoAT(const String& comando, int timeout);
void    exibirLinkAcesso();
void    configurarConexao();
void    lerSensoresFisicos(unsigned long tempoAtual);
void    atualizarAlertaLED(unsigned long tempoAtual);
void    montarJSON(char* destino, int tamanho);
void    registrarEventoTranca();
void    desligarBobinasMotor();
void    aplicarPassoMotor(int indice);
void    girarMotor(int passos, bool sentidoHorario);
void    trancarPorta();
void    destrancarPorta();
void    aplicarComando(const char* comando);
void    enviarStatusParaServidor();
void    imprimirStatusSerial(unsigned long tempoAtual);

void configurarHardware() {
    pinMode(PINO_LED,  OUTPUT);
    pinMode(PINO_TRIG, OUTPUT);
    pinMode(PINO_ECHO, INPUT);
    pinMode(PINO_PIR,  INPUT);

    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    pinMode(MOTOR_IN3, OUTPUT);
    pinMode(MOTOR_IN4, OUTPUT);
    desligarBobinasMotor();

    Wire.begin();
    if (rtc.begin()) {
        if (rtc.lostPower()) {
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
    }
}

boolean enviarComandoAT(const String& comando, const int timeout) {
    while (esp8266.available()) esp8266.read();

    esp8266.println(comando);

    const unsigned long inicio = millis();
    char janela[24];
    uint8_t n = 0;
    janela[0] = '\0';

    while (millis() - inicio < (unsigned long)timeout) {
        while (esp8266.available()) {
            char c = esp8266.read();
            Serial.print(c);
            if (n >= sizeof(janela) - 1) {
                memmove(janela, janela + 1, sizeof(janela) - 2);
                n = sizeof(janela) - 2;
            }
            janela[n++] = c;
            janela[n]   = '\0';
            if (strstr(janela, "OK") || strstr(janela, "ALREADY CONNECTED")) {
                return true;
            }
        }
    }
    return false;
}

void exibirLinkAcesso() {
    while (esp8266.available()) esp8266.read();
    esp8266.println(F("AT+CIFSR"));
    const unsigned long inicio = millis();
    while (millis() - inicio < 2000) {
        while (esp8266.available()) {
            Serial.print((char)esp8266.read());
        }
    }
}

void configurarConexao() {
    esp8266.begin(9600);

    Serial.println(F("[Wi-Fi] Iniciando modulo..."));
    enviarComandoAT(F("AT"), 1000);

    Serial.println(F("[Wi-Fi] Resetando..."));
    enviarComandoAT(F("AT+RST"), 2000);
    delay(2000);

    Serial.println(F("[Wi-Fi] Modo Station..."));
    enviarComandoAT(F("AT+CWMODE=1"), 1000);

    Serial.println(F("[Wi-Fi] Conectando ao Wi-Fi..."));
    String cmd = "AT+CWJAP=\"" + WIFI_SSID + "\",\"" + WIFI_PASS + "\"";
    if (enviarComandoAT(cmd, 15000)) {
        Serial.println(F("[Wi-Fi] Conectado com sucesso!"));
    } else {
        Serial.println(F("[AVISO] Falha na conexao Wi-Fi."));
    }

    enviarComandoAT(F("AT+CIPMUX=0"), 1000); delay(300);

    exibirLinkAcesso();
    Serial.print(F("[Backend] Enviando status para "));
    Serial.print(SERVIDOR_IP);  Serial.print(':');
    Serial.println(SERVIDOR_PORTA);
}

void lerSensoresFisicos(unsigned long tempoAtual) {
    if (tempoAtual - tempoAnteriorSensores < 400) return;
    tempoAnteriorSensores = tempoAtual;

    temPresenca = (digitalRead(PINO_PIR) == HIGH);

    digitalWrite(PINO_TRIG, LOW);  delayMicroseconds(2);
    digitalWrite(PINO_TRIG, HIGH); delayMicroseconds(10);
    digitalWrite(PINO_TRIG, LOW);

    long duracao = pulseIn(PINO_ECHO, HIGH, 10000);
    distancia = (duracao == 0) ? 999 : (int)(duracao * 0.034 / 2);

    portaAberta  = (distancia > DISTANCIA_LIMITE);
    portaFechada = (distancia <= DISTANCIA_LIMITE);
    obstruida    = (temPresenca && portaFechada && trancada);
}

void atualizarAlertaLED(unsigned long tempoAtual) {
    if (trancada) {
        if (portaAberta) {
            if (tempoAtual - tempoAnteriorPisca >= 300) {
                tempoAnteriorPisca = tempoAtual;
                estadoLedPisca = !estadoLedPisca;
                digitalWrite(PINO_LED, estadoLedPisca);
            }
        } else {
            digitalWrite(PINO_LED, HIGH);
        }
    } else {
        digitalWrite(PINO_LED, LOW);
    }
}

void montarJSON(char* destino, int tamanho) {
    const char* estadoPorta;
    if (obstruida)        estadoPorta = "OBSTRUIDA";
    else if (portaAberta) estadoPorta = "ABERTA";
    else                  estadoPorta = "FECHADA";

    snprintf_P(destino, tamanho,
        PSTR("{\"porta\":\"%s\",\"tranca\":\"%s\",\"obstruida\":\"%s\","
             "\"presenca\":\"%s\"}"),
        estadoPorta,
        trancada    ? "TRANCADA" : "DESTRANCADA",
        obstruida   ? "SIM" : "NAO",
        temPresenca ? "SIM" : "NAO");
}

void registrarEventoTranca() {
    Serial.print(F("[WEB] Tranca alterada para: "));
    Serial.println(trancada ? F("TRANCADA") : F("DESTRANCADA"));
}

void desligarBobinasMotor() {
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    digitalWrite(MOTOR_IN3, LOW);
    digitalWrite(MOTOR_IN4, LOW);
}

void aplicarPassoMotor(int indice) {
    digitalWrite(MOTOR_IN1, pgm_read_byte(&SEQUENCIA_PASSOS[indice][0]));
    digitalWrite(MOTOR_IN2, pgm_read_byte(&SEQUENCIA_PASSOS[indice][1]));
    digitalWrite(MOTOR_IN3, pgm_read_byte(&SEQUENCIA_PASSOS[indice][2]));
    digitalWrite(MOTOR_IN4, pgm_read_byte(&SEQUENCIA_PASSOS[indice][3]));
}

void girarMotor(int passos, bool sentidoHorario) {
    for (int i = 0; i < passos; i++) {
        int indice = sentidoHorario ? (i % N_ESTADOS)
                                    : (N_ESTADOS - 1 - (i % N_ESTADOS));
        aplicarPassoMotor(indice);
        delay(ATRASO_PASSO_MS);
    }
    desligarBobinasMotor();
}

void trancarPorta() {
    girarMotor(PASSOS_TRANCA, true);
}

void destrancarPorta() {
    girarMotor(PASSOS_TRANCA, false);
}

void aplicarComando(const char* comando) {
    if (strcmp(comando, "unlock") == 0) {
        if (trancada) {
            trancada = false;
            destrancarPorta();
            registrarEventoTranca();
        }
    } else if (strcmp(comando, "lock") == 0) {
        if (!trancada && portaFechada) {
            trancada = true;
            trancarPorta();
            registrarEventoTranca();
        } else if (!trancada && !portaFechada) {
            Serial.println(F("[WEB] TRANCAR ignorado: porta nao esta fechada."));
        }
    }
}

void enviarStatusParaServidor() {
    char json[140];
    montarJSON(json, sizeof(json));

    char cmd[48];
    snprintf_P(cmd, sizeof(cmd),
        PSTR("AT+CIPSTART=\"TCP\",\"%s\",%d"),
        SERVIDOR_IP.c_str(), SERVIDOR_PORTA);
    if (!enviarComandoAT(cmd, 4000)) {
        Serial.println(F("[ENVIO] Falha ao conectar no backend."));
        enviarComandoAT(F("AT+CIPCLOSE"), 800);
        return;
    }

    char req[260];
    snprintf_P(req, sizeof(req),
        PSTR("POST /api/device HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n\r\n"
             "%s"),
        SERVIDOR_IP.c_str(), SERVIDOR_PORTA, (int)strlen(json), json);

    esp8266.print(F("AT+CIPSEND="));
    esp8266.println((int)strlen(req));

    unsigned long inicio = millis();
    bool prompt = false;
    while (millis() - inicio < 3000) {
        if (esp8266.available() && esp8266.read() == '>') { prompt = true; break; }
    }
    if (!prompt) {
        Serial.println(F("[ENVIO] Sem prompt '>' do ESP."));
        enviarComandoAT(F("AT+CIPCLOSE"), 800);
        return;
    }

    esp8266.print(req);

    char    janela[24];
    uint8_t n = 0;
    janela[0] = '\0';
    bool achouUnlock = false;
    bool achouLock   = false;

    inicio = millis();
    unsigned long ultimoByte = inicio;
    while (millis() - inicio < 3000) {
        while (esp8266.available()) {
            char c = esp8266.read();
            if (n >= sizeof(janela) - 1) {
                memmove(janela, janela + 1, sizeof(janela) - 2);
                n = sizeof(janela) - 2;
            }
            janela[n++] = c;
            janela[n]   = '\0';
            if (strstr(janela, "unlock"))    achouUnlock = true;
            else if (strstr(janela, "lock")) achouLock   = true;
            ultimoByte = millis();
        }
        if (n > 0 && millis() - ultimoByte > 250) break;
    }

    if (achouUnlock)    aplicarComando("unlock");
    else if (achouLock) aplicarComando("lock");

    enviarComandoAT(F("AT+CIPCLOSE"), 800);
}

void imprimirStatusSerial(unsigned long tempoAtual) {
    if (tempoAtual - tempoAnteriorSerial < 1500) return;
    tempoAnteriorSerial = tempoAtual;

    Serial.print(F("[INFO] Presenca: "));
    Serial.print(temPresenca   ? F("SIM") : F("NAO"));
    Serial.print(F(" | Trancada: "));
    Serial.print(trancada      ? F("SIM") : F("NAO"));
    Serial.print(F(" | Aberta: "));
    Serial.println(portaAberta ? F("SIM") : F("NAO"));

    if (obstruida) {
        Serial.println(F("[CRITICO] Porta OBSTRUIDA!"));
    } else if (portaAberta && trancada) {
        Serial.println(F("[ALERTA] Porta ABERTA com tranca ATIVADA!"));
    }
}

void setup() {
    Serial.begin(9600);
    configurarHardware();

    Serial.println(F("========================================="));
    Serial.println(F("           -- EZmartLock --              "));
    Serial.println(F("========================================="));

    configurarConexao();
}

void loop() {
    unsigned long tempoAtual = millis();

    lerSensoresFisicos(tempoAtual);
    atualizarAlertaLED(tempoAtual);

    if (tempoAtual - tempoAnteriorEnvio >= INTERVALO_ENVIO) {
        tempoAnteriorEnvio = tempoAtual;
        enviarStatusParaServidor();
    }

    imprimirStatusSerial(tempoAtual);
}
