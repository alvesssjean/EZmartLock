import threading
import json
import serial
from fastapi import FastAPI
from contextlib import asynccontextmanager

# Configuração da porta - Ajuste para a sua porta do Arduino (ex: COM3, COM4, etc)
PORTA_SERIAL = "COM3" 
BAUD_RATE = 9600

estado_hardware = {}

def ler_arduino():
    global estado_hardware
    try:
        ser = serial.Serial(PORTA_SERIAL, BAUD_RATE, timeout=1)
        print(f"Conectado com sucesso ao Arduino na porta {PORTA_SERIAL}!")
        
        while True:
            if ser.in_waiting > 0:
                linha = ser.readline().decode('utf-8').strip()
                
                # Valida se a linha recebida é o JSON do Arduino
                if linha.startswith("{") and linha.endswith("}"):
                    try:
                        dados = json.loads(linha)
                        estado_hardware = dados
                        
                        # Aqui no Python você gerencia os prints de Alerta se quiser:
                        print(f"[STATUS] Distancia: {dados['distancia']}cm | Porta Aberta: {dados['portaAberta']}")
                        
                        if dados['obstruida']:
                            print("[ALERTA CRÍTICO] Porta OBSTRUIDA! Presença detectada com tranca ativada.")
                        elif dados['portaAberta'] and not dados['trancaDestrancada']:
                            print("[ALERTA] A porta está ABERTA, mas a tranca está ATIVADA!")
                            
                    except json.JSONDecodeError:
                        pass
    except Exception as e:
        print(f"Erro ao conectar na porta serial: {e}")

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Inicializa a thread de leitura do USB junto com o FastAPI
    thread = threading.Thread(target=ler_arduino, daemon=True)
    thread.start()
    yield

app = FastAPI(lifespan=lifespan)

@app.get("/status")
def obter_status():
    return estado_hardware