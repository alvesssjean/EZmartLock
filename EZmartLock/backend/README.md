# EZmartLock — Backend (Flask)

Central em Python que recebe o status do firmware (Arduino Uno + ESP8266),
guarda o estado atual, serve o dashboard web e devolve os comandos de
**TRANCAR / DESTRANCAR**. Também cuida do **monitoramento por horário**
(janela, alertas e trancamento automático) — tudo no site, nada é gravado na
memória do Arduino.

## Como funciona (o ESP é CLIENTE)

O ESP8266 em modo AT como servidor é instável, então **o dispositivo é o
cliente**: ele faz `POST` do status a cada ~3 s e recebe, na resposta, o comando
pendente. O backend nunca conecta no Arduino.

```
  ┌────────────┐  POST /api/device (status JSON)  ┌──────────────┐  GET /api/status  ┌───────────┐
  │  Arduino   │ ───────────────────────────────▶ │   Backend    │ ◀──────────────── │  Página   │
  │ + ESP8266  │ ◀── resposta {"cmd":"lock"...} ── │   (Flask)    │ ────────────────▶ │   Web     │
  │ (cliente)  │      (aplica no próximo ciclo)    │              │ ◀─ POST /trancar ─│ (browser) │
  └────────────┘                                   └──────────────┘                   └───────────┘
```

Como o ESP só aplica o comando no próximo envio, ao clicar em Trancar/Destrancar
há uma latência de ~1 ciclo. A página mostra "Aguardando dispositivo..." nesse
intervalo.

O backend mantém em memória: o último status, o histórico de eventos do período
e o comando pendente. A **configuração do monitoramento** é persistida em
`config.json` (criado automaticamente ao salvar; sobrevive a reinícios).

## Instalação e execução

```powershell
cd backend
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python app.py
```

Abra <http://localhost:5000>. Outros aparelhos na mesma rede acessam pelo IP do
PC, ex.: `http://192.168.99.4:5000`.

> O `SERVIDOR_IP` no firmware ([../src/main.cpp](../src/main.cpp)) deve ser o IP
> **deste PC**, e a **porta 5000 precisa estar liberada no Firewall** do Windows
> (regra de entrada), pois é o Arduino que conecta no PC.

## Variáveis de ambiente

| Variável               | Padrão | Descrição                                            |
| ---------------------- | ------ | ---------------------------------------------------- |
| `PORT`                 | `5000` | Porta em que o backend escuta                        |
| `TIMEOUT_ONLINE`       | `10`   | Segundos sem contato p/ marcar o dispositivo offline |
| `ALERTA_SEGUNDOS`      | `60`   | Segundos aberta/destrancada p/ disparar o alerta     |
| `AUTO_TRANCAR_DELAY_S` | `10`   | Segundos destrancada antes do auto-trancar agir      |

## Monitoramento

Configurável pela seção "Configurações" do dashboard (ou via `POST /api/config`):

- **Janela de horário** (`inicio`/`fim`) em que a porta deveria ficar fechada e
  trancada — aceita período que cruza a meia-noite (ex.: 20:00–08:00). Há também
  o modo **24 horas**.
- **Histórico**: cada mudança de porta/tranca **durante o período** é registrada
  com a hora do servidor (não usa o RTC do Arduino).
- **Alertas (> 1 min):** se a porta ficar **ABERTA** ou **DESTRANCADA** por mais
  que `ALERTA_SEGUNDOS` dentro do período, o site mostra um alerta. O usuário
  pode **TRANCAR** (se fechada), **adiar** por X minutos ou **desligar o
  monitoramento por hoje** (volta no próximo período; dá para reativar na hora).
- **Auto-trancar** (opcional): no período, se a porta estiver FECHADA e
  DESTRANCADA por `AUTO_TRANCAR_DELAY_S`, o backend enfileira `lock`.

## API

| Método | Rota                              | Quem usa    | Descrição                                       |
| ------ | --------------------------------- | ----------- | ----------------------------------------------- |
| POST   | `/api/device`                     | dispositivo | Recebe status; responde `{"cmd": ...}`          |
| GET    | `/`                               | navegador   | Dashboard                                       |
| GET    | `/api/status`                     | navegador   | Status atual + monitoramento + alertas          |
| GET    | `/api/historico`                  | navegador   | Eventos (porta/tranca) do período               |
| GET    | `/api/config`                     | navegador   | Configuração do monitoramento                   |
| POST   | `/api/config`                     | navegador   | Atualiza a configuração                         |
| POST   | `/api/trancar`                    | navegador   | Enfileira TRANCAR (só se a porta estiver FECHADA) |
| POST   | `/api/destrancar`                 | navegador   | Enfileira DESTRANCAR                            |
| POST   | `/api/monitoramento/desligar-hoje`| navegador   | Desliga o monitoramento até o próximo período   |
| POST   | `/api/monitoramento/reativar`     | navegador   | Reativa o monitoramento agora                   |
| POST   | `/api/alerta/<tipo>/adiar`        | navegador   | Adia o alerta (`aberta`\|`destrancada`) N min   |

O dispositivo envia no `POST /api/device` um JSON com `porta`, `tranca`,
`obstruida` e `presenca`. Exemplo de `/api/status`:

```json
{
  "online": true,
  "porta": "FECHADA",
  "tranca": "TRANCADA",
  "obstruida": "NAO",
  "presenca": "NAO",
  "atualizado_em": "14:31:10",
  "em_monitoramento": true,
  "monitor_desligado_hoje": false,
  "alerta_aberta": false,
  "alerta_destrancada": false,
  "config": { "monitor_ativo": true, "modo_24h": false,
              "inicio": "20:00", "fim": "08:00", "auto_trancar": false }
}
```

## Solução de problemas

**Dashboard mostra "dispositivo offline":** o backend não recebe
`POST /api/device` há mais de `TIMEOUT_ONLINE` segundos. Verifique, em ordem:

1. O backend está rodando? Rode `python testar_conexao.py` (com o backend ativo):
   ele simula um POST e mostra o IP que deve ir no firmware.
2. `SERVIDOR_IP` no firmware é o IP **deste PC** (não o do Arduino)? Se mudou
   (DHCP), atualize o firmware ou fixe o IP do PC no roteador.
3. Firewall do Windows liberando a porta 5000 (causa mais comum).
4. Monitor serial do Arduino deve mostrar `Conectado com sucesso!`; se aparecer
   `Falha ao conectar no backend`, é IP errado ou firewall.
5. PC e Arduino na mesma rede Wi-Fi (2.4 GHz, sem isolamento de AP).

**O botão demora para refletir:** normal — o ESP aplica o comando no próximo
ciclo (~3 s).
