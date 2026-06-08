#include <Arduino.h>
#include <IRremote.hpp>
#include <SoftwareSerial.h>

const int PINO_IR      = 5;  
const int PINO_LED     = 13; 
const int PINO_TRIG    = 8;
const int PINO_ECHO    = 9;
const int PINO_PIR     = 7; 

SoftwareSerial esp8266(10, 11); 

const int DISTANCIA_LIMITE = 3; 

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
  
  IrReceiver.begin(PINO_IR, DISABLE_LED_FEEDBACK);
}

boolean enviarComandoAT(String comando, const int timeout) {
  while (esp8266.available()) {
    esp8266.read();
  }

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

  Serial.println(F("[Wi-Fi] Resetando configurações..."));
  enviarComandoAT(F("AT+RST"), 2000);
  delay(2000);

  Serial.println(F("[Wi-Fi] Modo Station..."));
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

void verificarControleIR() {
  if (IrReceiver.decode()) {
    if (!(IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)) {
      if(IrReceiver.decodedIRData.command != 0){
          trancaDestrancada = !trancaDestrancada;
          Serial.println(trancaDestrancada ? F("DESTRANCADA") : F("TRANCADA"));
      }
    }
    IrReceiver.resume(); 
  }
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
    
    if (duracao == 0) {
      distancia = 999; 
    } else {
      distancia = duracao * 0.034 / 2;
    }
    
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
    } else {
      digitalWrite(PINO_LED, HIGH); 
    }
  } else {
    digitalWrite(PINO_LED, LOW); 
  }
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
          if (requisicao.length() < 20) {
            requisicao += c; 
          }
          tempoEsvaziar = millis() + 10; 
        }
      }

      if (requisicao.indexOf("favicon") != -1) {
        esp8266.print("AT+CIPCLOSE=");
        esp8266.println(connectionId);
        return;
      }

      String statusPorta = "FECHADA";
      if (obstruida) {
        statusPorta = "OBSTRUIDA";
      } else if (portaAberta) {
        statusPorta = "ABERTA";
      }

      String statusTranca = trancaDestrancada ? "DESTRANCADA" : "TRANCADA";

      String pacote = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
      pacote += "<!DOCTYPE html><html><body>";
      pacote += "<h1>Porta: " + statusPorta + "</h1>";
      pacote += "<h1>Tranca: " + statusTranca + "</h1>";
      pacote += "</body></html>";

      while (esp8266.available()) esp8266.read(); 

      esp8266.print("AT+CIPSEND=");
      esp8266.print(connectionId);
      esp8266.print(",");
      esp8266.println(pacote.length());
      
      if (esp8266.find(">")) {
        esp8266.print(pacote); 
        Serial.println(F("Informações enviadas com sucesso!"));
      }

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
    if (distancia == 999) { Serial.print(F(">170")); } else { Serial.print(distancia); }
    Serial.print(F("cm | Presenca: "));
    Serial.print(temPresenca ? F("SIM") : F("NAO"));
    Serial.print(F(" | Destrancada: "));
    Serial.print(trancaDestrancada ? F("SIM") : F("NAO"));
    Serial.print(F(" | Aberta: "));
    Serial.println(portaAberta ? F("SIM") : F("NAO"));

    if (obstruida) {
      Serial.println(F("[CRÍTICO] Porta OBSTRUIDA!"));
    } else if (portaAberta && !trancaDestrancada) {
      Serial.println(F("[ALERTA] A porta foi ABERTA com a tranca ATIVADA!"));
    }
  }
}

void setup() {
  Serial.begin(9600);
  configurarHardware();
  
  Serial.println(F("========================================="));
  Serial.println(F("         -- EZmartLock --        "));
  Serial.println(F("========================================="));
  
  configurarConexao(); 
}

void loop() {
  unsigned long tempoAtual = millis(); 

  verificarControleIR();
  lerSensoresFisicos(tempoAtual);
  atualizarAlertaLED(tempoAtual);
  atenderRequisicoesWeb(); 
  imprimirStatusSerial(tempoAtual);
}