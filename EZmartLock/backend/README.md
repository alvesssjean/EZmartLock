# EZmartLock — Backend + Frontend

Central em Python (Flask) que recebe os dados do firmware (Arduino Uno +
ESP8266), guarda o último status + histórico de eventos, serve o dashboard e
repassa os comandos de **TRANCAR / DESTRANCAR**.

## Arquitetura (o ESP é CLIENTE)

O ESP8266 em modo AT como **servidor** é instável: só aguenta uma conexão por
vez e satura sob acesso contínuo. Por isso o **dispositivo é o cliente** — ele
faz `POST` do status para o backend e recebe, na resposta, o comando pendente.
O backend nunca conecta no Arduino.

```
  ┌────────────┐  POST /api/device (status JSON)  ┌──────────────┐  GET /api/status  ┌───────────┐
  │  Arduino   │ ───────────────────────────────▶ │   Backend    │ ◀──────────────── │  Página   │
  │ + ESP8266  │ ◀── resposta {"cmd":"lock"...} ── │   (Flask)    │ ────────────────▶ │   Web     │
  │ (cliente)  │      (aplica no próximo ciclo)    │              │ ◀─ POST /trancar ─│ (browser) │
  └────────────┘                                   └──────────────┘                   └───────────┘
```

Consequência: ao clicar em Trancar/Destrancar há uma latência de ~1 ciclo
(o ESP só aplica no próximo envio). A página mostra "Aguardando dispositivo...".

## Pré-requisitos

1. Descubra o **IP deste PC** na rede (é o `SERVIDOR_IP` do firmware). Rode
   `python testar_conexao.py` (mostra o IP) ou use `ipconfig`.
2. No firmware [../src/main.cpp](../src/main.cpp), ajuste e **regrave o Arduino**:
   ```cpp
   const String SERVIDOR_IP    = "192.168.99.4";  // IP do SEU PC
   const int    SERVIDOR_PORTA = 5000;
   ```
3. **Libere a porta 5000 no Firewall do Windows** (entrada). Como o Arduino
   agora conecta *no PC*, o firewall precisa permitir. Em um PowerShell **como
   administrador**:
   ```powershell
   New-NetFirewallRule -DisplayName "EZmartLock backend" -Direction Inbound `
     -Action Allow -Protocol TCP -LocalPort 5000
   ```
4. PC e Arduino na **mesma rede Wi-Fi** (2.4 GHz; sem isolamento de AP/rede de
   convidados).

## Instalação

```powershell
cd backend
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```

## Execução

```powershell
python app.py
```

Abra no navegador: <http://localhost:5000>

> Outros dispositivos da rede (celular, etc.) acessam pelo IP do PC,
> ex.: `http://192.168.99.4:5000`.

## Variáveis de ambiente

| Variável         | Padrão | Descrição                                      |
| ---------------- | ------ | ---------------------------------------------- |
| `PORT`           | `5000` | Porta em que o backend escuta                  |
| `TIMEOUT_ONLINE` | `10`   | Segundos sem contato p/ marcar como offline    |

## API

| Método | Rota              | Quem usa     | Descrição                                  |
| ------ | ----------------- | ------------ | ------------------------------------------ |
| POST   | `/api/device`     | dispositivo  | Recebe status; responde `{"cmd": ...}`     |
| GET    | `/`               | navegador    | Página web (dashboard)                     |
| GET    | `/api/status`     | navegador    | Último status + `online` e `atualizado_em` |
| GET    | `/api/historico`  | navegador    | Últimos eventos de tranca (com horário)    |
| POST   | `/api/trancar`    | navegador    | Enfileira comando TRANCAR                   |
| POST   | `/api/destrancar` | navegador    | Enfileira comando DESTRANCAR               |

Exemplo de `/api/status`:

```json
{
  "online": true,
  "porta": "FECHADA",
  "tranca": "TRANCADA",
  "obstruida": "NAO",
  "presenca": "NAO",
  "dist": 2,
  "hora_tranca": "14:30:05",
  "atualizado_em": "14:31:10"
}
```

## Solução de problemas

### O dashboard mostra "dispositivo offline"

Significa que o backend não recebe `POST /api/device` há mais de `TIMEOUT_ONLINE`
segundos. Verifique, em ordem:

1. **O backend está rodando?** Rode `python testar_conexao.py` (com o backend
   ativo): ele simula um POST e mostra o IP que deve ir no firmware. Se isso
   funcionar mas o Arduino não, o problema está entre o Arduino e o PC.
2. **`SERVIDOR_IP` certo no firmware?** Deve ser o IP **deste PC** (não o do
   Arduino). Confira com `testar_conexao.py` / `ipconfig`. Se o IP do PC mudou
   (DHCP), atualize o firmware ou fixe o IP do PC no roteador.
3. **Firewall do Windows** liberando a porta 5000 (passo 3 dos pré-requisitos).
   É a causa mais comum de "tudo certo mas nada chega".
4. **Monitor serial do Arduino:** deve mostrar `Conectado com sucesso!` e, a
   cada ciclo, a tentativa de envio. Se aparecer `Falha ao conectar no backend`,
   é IP errado ou firewall.
5. **Mesma rede Wi-Fi** (2.4 GHz, sem isolamento de AP).

### O botão demora para refletir

Normal: o ESP só aplica o comando no próximo ciclo de envio (~3 s). A página
mostra "Aguardando dispositivo..." até o estado mudar.
