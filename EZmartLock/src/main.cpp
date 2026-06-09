#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <RTClib.h>

const int PINO_LED     = 13; 
const int PINO_TRIG    = 8;
const int PINO_ECHO    = 9;
const int PINO_PIR     = 7; 

SoftwareSerial esp8266(10, 11); 
RTC_DS3231 rtc;

const int DISTANCIA_LIMITE = 3; 

// SUBSTITUA PELOS DADOS DA SUA REDE
const String WIFI_SSID = "Torres";
const String WIFI_PASS = "Mftl552912";

bool trancaDestrancada = false; 
int distancia = 0;
bool temPresenca = false;
bool portaAberta = false;
bool portaFechada = false;
bool obstruida = false;

unsigned long tempoAnteriorPisca = 0;
unsigned long tempoAnteriorSerial = 0;
unsigned long tempoAnteriorSensores = 0; 
bool estadoLedPisca = LOW;

void configurarHardware() {
  pinMode(PINO_LED, OUTPUT);
  pinMode(PINO_TRIG, OUTPUT);
  pinMode(PINO_ECHO, INPUT);
  pinMode(PINO_PIR, INPUT); 

  Wire.begin();
  if (rtc.begin()) {
    if (rtc.lostPower()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }
}

boolean enviarComandoAT(String comando, const int timeout) {
  while (esp8266.available()) esp8266.read();

  esp8266.println(comando);
  long int tempoLimitrofe = millis() + timeout;
  String bufferTexto = "";
  
  while (millis() < tempoLimitrofe) {
    while (esp8266.available()) {
      char c = esp8266.read();
      Serial.print(c);
      
      bufferTexto += c;
      if (bufferTexto.length() > 40) {
        bufferTexto = bufferTexto.substring(bufferTexto.length() - 20);
      }
      
      if (bufferTexto.indexOf("OK") != -1 || bufferTexto.indexOf("ALREADY CONNECTED") != -1) {
        return true; 
      }
    }
  }
  return false;
}

void exibirLinkAcesso() {
  while(esp8266.available()) esp8266.read();
  esp8266.println(F("AT+CIFSR"));
  long tempoLimite = millis() + 2000;
  while (millis() < tempoLimite) {
    while (esp8266.available()) {
      char c = esp8266.read();
      Serial.print(c);
    }
  }
}

void configurarConexao() {
  esp8266.begin(9600); 
  
  Serial.println(F("[Wi-Fi] Iniciando modulo..."));
  enviarComandoAT(F("AT"), 1000);
  enviarComandoAT(F("AT+RST"), 2000);
  delay(2000);
  enviarComandoAT(F("AT+CWMODE=1"), 1000);

  Serial.println(F("[Wi-Fi] Conectando ao Roteador..."));
  String comandoConexao = "AT+CWJAP=\"" + WIFI_SSID + "\",\"" + WIFI_PASS + "\"";
  
  if (enviarComandoAT(comandoConexao, 15000)) {
    Serial.println(F("[Wi-Fi] Autenticado com sucesso!"));
  } else {
    Serial.println(F("[AVISO] Falha na conexao."));
  }

  enviarComandoAT(F("AT+CIPMUX=1"), 1000);
  delay(500); 
  enviarComandoAT(F("AT+CIPSERVER=1,80"), 1000);
  delay(500);
  exibirLinkAcesso();
}

// LOG DO RELÓGIO (Acionado pela API)
void registrarEventoTranca(String acao) {
  DateTime agora = rtc.now();
  Serial.print(F("[API - "));
  if (agora.hour() < 10) Serial.print('0');
  Serial.print(agora.hour(), DEC);
  Serial.print(':');
  if (agora.minute() < 10) Serial.print('0');
  Serial.print(agora.minute(), DEC);
  Serial.print(':');
  if (agora.second() < 10) Serial.print('0');
  Serial.print(agora.second(), DEC);
  Serial.print(F("] Comando recebido: "));
  Serial.println(acao);
}

void lerSensoresFisicos(unsigned long tempoAtual) {
  if (tempoAtual - tempoAnteriorSensores >= 400) {
    tempoAnteriorSensores = tempoAtual;

    int presencaDetectada = digitalRead(PINO_PIR);
    
    digitalWrite(PINO_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PINO_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PINO_TRIG, LOW);
    
    long duracao = pulseIn(PINO_ECHO, HIGH, 10000); 
    if (duracao == 0) distancia = 999; 
    else distancia = duracao * 0.034 / 2;
    
    temPresenca = (presencaDetectada == HIGH);
    portaAberta = (distancia > DISTANCIA_LIMITE);
    portaFechada = (distancia <= DISTANCIA_LIMITE);
    obstruida = (temPresenca && portaFechada && !trancaDestrancada);
  }
}

void atualizarAlertaLED(unsigned long tempoAtual) {
  if (!trancaDestrancada) { 
    if (portaAberta) {
      if (tempoAtual - tempoAnteriorPisca >= 300) { 
        tempoAnteriorPisca = tempoAtual;
        estadoLedPisca = !estadoLedPisca; 
        digitalWrite(PINO_LED, estadoLedPisca);
      }
    } else { digitalWrite(PINO_LED, HIGH); }
  } else { digitalWrite(PINO_LED, LOW); }
}

void atenderRequisicoesWeb() {
  if (esp8266.available()) {
    if (esp8266.find("+IPD,")) {
      int connectionId = esp8266.parseInt(); 

      String requisicao = "";
      long tempoEsvaziar = millis() + 100; 
      while (millis() < tempoEsvaziar) {
        while (esp8266.available()) {
          char c = esp8266.read();
          if (requisicao.length() < 25) requisicao += c; 
          tempoEsvaziar = millis() + 10; 
        }
      }

      if (requisicao.indexOf("favicon") != -1) {
        esp8266.print("AT+CIPCLOSE=");
        esp8266.println(connectionId);
        return;
      }

      // ROTEADOR DA API: Verifica qual comando o Frontend enviou
      if (requisicao.indexOf("GET /destrancar") != -1) {
        trancaDestrancada = true;
        registrarEventoTranca(F("DESTRANCAR"));
      } else if (requisicao.indexOf("GET /trancar") != -1) {
        trancaDestrancada = false;
        registrarEventoTranca(F("TRANCAR"));
      }

      String statusPorta = "FECHADA";
      if (obstruida) statusPorta = "OBSTRUIDA";
      else if (portaAberta) statusPorta = "ABERTA";

      String statusTranca = trancaDestrancada ? "DESTRANCADA" : "TRANCADA";

      // MONTA O PACOTE JSON PARA A API
      String json = "{\"porta\":\"" + statusPorta + "\",\"tranca\":\"" + statusTranca + "\"}";

      // Cabeçalho modificado com permissão de acesso (CORS)
      String pacote = "HTTP/1.1 200 OK\r\n";
      pacote += "Content-Type: application/json\r\n";
      pacote += "Access-Control-Allow-Origin: *\r\n"; 
      pacote += "Connection: close\r\n\r\n";
      pacote += json;

      while (esp8266.available()) esp8266.read(); 

      esp8266.print("AT+CIPSEND=");
      esp8266.print(connectionId);
      esp8266.print(",");
      esp8266.println(pacote.length());
      
      if (esp8266.find(">")) esp8266.print(pacote); 

      delay(150);
      esp8266.print("AT+CIPCLOSE=");
      esp8266.println(connectionId);
    }
  }
}

void imprimirStatusSerial(unsigned long tempoAtual) {
  if (tempoAtual - tempoAnteriorSerial >= 1500) {
    tempoAnteriorSerial = tempoAtual;
    Serial.print(F("[INFO] Dist: "));
    if (distancia == 999) Serial.print(F(">170")); else Serial.print(distancia);
    Serial.print(F("cm | Presenca: "));
    Serial.print(temPresenca ? F("SIM") : F("NAO"));
    Serial.print(F(" | Destrancada: "));
    Serial.print(trancaDestrancada ? F("SIM") : F("NAO"));
    Serial.print(F(" | Aberta: "));
    Serial.println(portaAberta ? F("SIM") : F("NAO"));
  }
}

void setup() {
  Serial.begin(9600);
  configurarHardware();
  Serial.println(F("========================================="));
  Serial.println(F("     -- EZmartLock API REST --    "));
  Serial.println(F("========================================="));
  configurarConexao(); 
}

void loop() {
  unsigned long tempoAtual = millis(); 
  lerSensoresFisicos(tempoAtual);
  atualizarAlertaLED(tempoAtual);
  atenderRequisicoesWeb(); 
  imprimirStatusSerial(tempoAtual);
}