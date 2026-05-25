#include <Arduino.h>
#include <IRremote.hpp>

const int PINO_IR      = 5;  
const int PINO_LED     = 13; 
const int PINO_TRIG    = 8;
const int PINO_ECHO    = 9;
const int PINO_PIR     = 7; 

const int DISTANCIA_LIMITE = 3; 

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

void verificarControleIR() {
  if (IrReceiver.decode()) {
    if (!(IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)) {
      if(IrReceiver.decodedIRData.command != 0){
          trancaDestrancada = !trancaDestrancada; 
          Serial.println(trancaDestrancada ? "DESTRANCADA" : "TRANCADA");
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

void alertaDeLED(unsigned long tempoAtual) {
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

void imprimirStatusPorta(unsigned long tempoAtual) {
  if (tempoAtual - tempoAnteriorSerial >= 1000) {
    tempoAnteriorSerial = tempoAtual;
    
    Serial.print("[INFO] Distancia: ");
    if (distancia == 999) {
      Serial.print("> 170");
    } else {
      Serial.print(distancia);
    }
    Serial.print(" cm | Presenca: ");
    Serial.print(temPresenca ? "SIM" : "NAO");
    Serial.print(" | Tranca: ");
    Serial.print(trancaDestrancada ? "DESTRANCADA" : "TRANCADA");
    Serial.print(" | Porta: ");
    Serial.println(portaAberta ? "ABERTA" : "FECHADA");

    if (obstruida) {
      Serial.println("[ALERTA CRÍTICO] Porta OBSTRUIDA! Presenca detectada na porta trancada e fechada.");
    } else if (portaAberta && !trancaDestrancada) {
      Serial.println("[ALERTA] A porta está ABERTA, mas a tranca esta ATIVADA!");
    }
  }
}

void setup() {
  Serial.begin(9600);
  configurarHardware();
  
  Serial.println("=========================================");
  Serial.println("            -- EZmartLock --             ");
  Serial.println("=========================================");
}

void loop() {
  unsigned long tempoAtual = millis();

  verificarControleIR();
  lerSensoresFisicos(tempoAtual);
  alertaDeLED(tempoAtual);
  imprimirStatusPorta(tempoAtual);
}