"""
Diagnóstico do EZmartLock (arquitetura: ESP como CLIENTE).

1) Mostra o IP deste PC na rede local — é o valor que você deve colocar em
   SERVIDOR_IP no firmware (src/main.cpp).
2) Simula um POST do dispositivo (POST /api/device) para confirmar que o
   backend recebe o status e responde o comando pendente.

Uso (com o backend rodando em outra janela):
    python testar_conexao.py
"""
import socket

import requests


def ip_local():
    """Descobre o IP da interface usada para sair na rede (sem enviar nada)."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except Exception:
        return "127.0.0.1"
    finally:
        s.close()


ip = ip_local()
print(f"IP deste PC na rede: {ip}")
print(f'  -> No firmware (src/main.cpp), use:  SERVIDOR_IP = "{ip}"')
print()

porta = 5000
url = f"http://127.0.0.1:{porta}/api/device"
exemplo = {
    "porta": "FECHADA", "tranca": "TRANCADA", "obstruida": "NAO",
    "presenca": "NAO", "hora_tranca": "--:--:--", "dist": 3,
}

print(f"Simulando POST do dispositivo em {url} ...")
try:
    r = requests.post(url, json=exemplo, timeout=4)
    print(f"HTTP {r.status_code} | resposta do backend: {r.json()}")
    print("\nOK! O backend recebeu o status e respondeu o comando.")
    print(f"Abra http://localhost:{porta} — deve aparecer 'FECHADA / TRANCADA'.")
    print("\nLembre-se: quem envia de verdade é o Arduino. Confirme que")
    print(f"SERVIDOR_IP no firmware é {ip} e que a porta {porta} está liberada")
    print("no Firewall do Windows (veja o README).")
except Exception as e:
    print(f"FALHOU: {type(e).__name__} - {e}")
    print("O backend está rodando nesta máquina? (python app.py)")
