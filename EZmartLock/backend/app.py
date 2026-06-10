import json
import logging
import os
import threading
import time
from collections import deque
from datetime import datetime, timedelta

from flask import Flask, jsonify, render_template, request

logging.basicConfig(level=logging.INFO, format="%(asctime)s  %(message)s",
                    datefmt="%H:%M:%S")
log = logging.getLogger("ezmartlock")

TIMEOUT_ONLINE = float(os.environ.get("TIMEOUT_ONLINE", "10"))
ALERTA_SEGUNDOS = float(os.environ.get("ALERTA_SEGUNDOS", "60"))
AUTO_TRANCAR_DELAY_S = float(os.environ.get("AUTO_TRANCAR_DELAY_S", "10"))
CONFIG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "config.json")

app = Flask(__name__)

_lock = threading.Lock()

_estado = {
    "online":        False,
    "porta":         "-",
    "tranca":        "-",
    "obstruida":     "-",
    "presenca":      "-",
    "atualizado_em": None,
}

_config = {
    "monitor_ativo": True,
    "modo_24h":      False,
    "inicio":        "20:00",
    "fim":           "08:00",
    "auto_trancar":  False,
}

_historico = deque(maxlen=80)
_ultima_tranca = None
_ultima_porta = None
_comando_pendente = None
_ultimo_contato = 0.0

_porta_aberta_desde = None
_destrancada_desde = None

_alertas = {
    "aberta":      {"adiar_ate": 0.0},
    "destrancada": {"adiar_ate": 0.0},
}

_monitor_desligado_dia = None


def _carregar_config():
    try:
        with open(CONFIG_PATH, "r", encoding="utf-8") as f:
            dados = json.load(f)
        for k in _config:
            if k in dados:
                _config[k] = dados[k]
        log.info("Configuração carregada de %s", CONFIG_PATH)
    except FileNotFoundError:
        pass
    except (ValueError, OSError) as e:
        log.warning("Falha ao ler config (%s); usando padrões", e)


def _salvar_config():
    try:
        with open(CONFIG_PATH, "w", encoding="utf-8") as f:
            json.dump(_config, f, ensure_ascii=False, indent=2)
    except OSError as e:
        log.warning("Falha ao salvar config: %s", e)


def _hora_valida(texto):
    try:
        h, m = str(texto).split(":")
        return 0 <= int(h) <= 23 and 0 <= int(m) <= 59
    except (ValueError, AttributeError):
        return False


def _hhmm_para_min(texto):
    try:
        h, m = str(texto).split(":")
        return (int(h) % 24) * 60 + (int(m) % 60)
    except (ValueError, AttributeError):
        return 0


def _dentro_da_janela(agora=None):
    if not _config["monitor_ativo"]:
        return False
    if _monitor_desligado_dia is not None and _monitor_desligado_dia == _data_sessao(agora):
        return False
    if _config["modo_24h"]:
        return True
    agora = agora or datetime.now()
    atual = agora.hour * 60 + agora.minute
    ini = _hhmm_para_min(_config["inicio"])
    fim = _hhmm_para_min(_config["fim"])
    if ini == fim:
        return True
    if ini < fim:
        return ini <= atual < fim
    return atual >= ini or atual < fim


def _data_sessao(agora=None):
    agora = agora or datetime.now()
    if not _config["modo_24h"]:
        ini = _hhmm_para_min(_config["inicio"])
        fim = _hhmm_para_min(_config["fim"])
        atual = agora.hour * 60 + agora.minute
        if ini > fim and atual < fim:
            agora = agora - timedelta(days=1)
    return agora.strftime("%Y-%m-%d")


def _alerta_suprimido(tipo):
    return time.time() < _alertas[tipo]["adiar_ate"]


def _registrar_evento(tipo, valor):
    _historico.appendleft({
        "tipo":          tipo,
        "valor":         valor,
        "registrado_em": datetime.now().strftime("%d/%m/%Y %H:%M:%S"),
    })


@app.route("/api/device", methods=["POST"])
def api_device():
    global _ultima_tranca, _ultima_porta, _comando_pendente, _ultimo_contato
    global _porta_aberta_desde, _destrancada_desde
    dados = request.get_json(silent=True) or {}
    with _lock:
        if dados:
            _estado.update(dados)
        _estado["online"] = True
        _estado["atualizado_em"] = datetime.now().strftime("%H:%M:%S")
        _ultimo_contato = time.monotonic()

        em_janela = _dentro_da_janela()

        porta = dados.get("porta")
        if porta is not None:
            if _ultima_porta is not None and porta != _ultima_porta and em_janela:
                _registrar_evento("porta", porta)
            _ultima_porta = porta
            if porta == "ABERTA":
                if _porta_aberta_desde is None:
                    _porta_aberta_desde = time.monotonic()
            else:
                _porta_aberta_desde = None

        tranca = dados.get("tranca")
        if tranca is not None:
            if _ultima_tranca is not None and tranca != _ultima_tranca and em_janela:
                _registrar_evento("tranca", tranca)
            _ultima_tranca = tranca
            if tranca == "DESTRANCADA":
                if _destrancada_desde is None:
                    _destrancada_desde = time.monotonic()
            else:
                _destrancada_desde = None

        destrancada_ha = (
            (time.monotonic() - _destrancada_desde)
            if _destrancada_desde is not None else 0.0
        )
        if (_config["auto_trancar"] and em_janela
                and _estado.get("porta") == "FECHADA"
                and _estado.get("tranca") == "DESTRANCADA"
                and destrancada_ha >= AUTO_TRANCAR_DELAY_S
                and _comando_pendente is None):
            _comando_pendente = "lock"
            log.info("Auto-trancar: LOCK enfileirado (destrancada ha %.0fs, porta fechada)",
                     destrancada_ha)

        cmd = _comando_pendente or "none"
        _comando_pendente = None

    if cmd != "none":
        log.info("Comando entregue ao dispositivo: %s", cmd)
    return jsonify({"cmd": cmd})


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/status")
def api_status():
    with _lock:
        e = dict(_estado)
        online = (time.monotonic() - _ultimo_contato) <= TIMEOUT_ONLINE
        e["online"] = online

        em_janela = _dentro_da_janela()
        e["em_monitoramento"] = em_janela
        e["monitor_desligado_hoje"] = bool(
            _monitor_desligado_dia is not None
            and _monitor_desligado_dia == _data_sessao())

        dur_aberta = (time.monotonic() - _porta_aberta_desde) if _porta_aberta_desde else 0.0
        dur_destr = (time.monotonic() - _destrancada_desde) if _destrancada_desde else 0.0

        e["alerta_aberta"] = bool(
            online and em_janela and e.get("porta") == "ABERTA"
            and dur_aberta >= ALERTA_SEGUNDOS and not _alerta_suprimido("aberta"))
        e["alerta_destrancada"] = bool(
            online and em_janela and e.get("tranca") == "DESTRANCADA"
            and dur_destr >= ALERTA_SEGUNDOS and not _alerta_suprimido("destrancada"))

        e["config"] = dict(_config)
        return jsonify(e)


@app.route("/api/historico")
def api_historico():
    with _lock:
        return jsonify(list(_historico))


@app.route("/api/config", methods=["GET"])
def api_config_get():
    with _lock:
        return jsonify(dict(_config))


@app.route("/api/config", methods=["POST"])
def api_config_post():
    dados = request.get_json(silent=True) or {}
    with _lock:
        if "monitor_ativo" in dados:
            _config["monitor_ativo"] = bool(dados["monitor_ativo"])
        if "modo_24h" in dados:
            _config["modo_24h"] = bool(dados["modo_24h"])
        if "auto_trancar" in dados:
            _config["auto_trancar"] = bool(dados["auto_trancar"])
        if "inicio" in dados and _hora_valida(dados["inicio"]):
            _config["inicio"] = dados["inicio"]
        if "fim" in dados and _hora_valida(dados["fim"]):
            _config["fim"] = dados["fim"]
        _salvar_config()
        cfg = dict(_config)
    log.info("Configuração atualizada: %s", cfg)
    return jsonify({"ok": True, "config": cfg})


@app.route("/api/trancar", methods=["POST"])
def api_trancar():
    global _comando_pendente
    with _lock:
        if _estado.get("porta") != "FECHADA":
            log.info("Comando TRANCAR recusado: porta nao esta FECHADA")
            return jsonify({"ok": False, "motivo": "porta_aberta"}), 409
        _comando_pendente = "lock"
    log.info("Comando TRANCAR enfileirado pelo site")
    return jsonify({"ok": True, "aguardando": True})


@app.route("/api/destrancar", methods=["POST"])
def api_destrancar():
    global _comando_pendente
    with _lock:
        _comando_pendente = "unlock"
    log.info("Comando DESTRANCAR enfileirado pelo site")
    return jsonify({"ok": True, "aguardando": True})


@app.route("/api/monitoramento/desligar-hoje", methods=["POST"])
def api_monitor_desligar_hoje():
    global _monitor_desligado_dia
    with _lock:
        _monitor_desligado_dia = _data_sessao()
    log.info("Monitoramento desligado pelo usuário até o próximo período")
    return jsonify({"ok": True})


@app.route("/api/monitoramento/reativar", methods=["POST"])
def api_monitor_reativar():
    global _monitor_desligado_dia
    with _lock:
        _monitor_desligado_dia = None
    log.info("Monitoramento reativado manualmente pelo usuário")
    return jsonify({"ok": True})


@app.route("/api/alerta/<tipo>/adiar", methods=["POST"])
def api_alerta_adiar(tipo):
    if tipo not in _alertas:
        return jsonify({"ok": False, "motivo": "tipo_invalido"}), 400
    dados = request.get_json(silent=True) or {}
    try:
        minutos = max(1, int(dados.get("minutos", 10)))
    except (ValueError, TypeError):
        minutos = 10
    with _lock:
        _alertas[tipo]["adiar_ate"] = time.time() + minutos * 60
    log.info("Alerta '%s' adiado por %d min", tipo, minutos)
    return jsonify({"ok": True, "minutos": minutos})


_carregar_config()


def main():
    porta = int(os.environ.get("PORT", "5000"))
    log.info("=== EZmartLock backend ===")
    log.info("Aguardando o dispositivo enviar status em POST /api/device")
    log.info("Dashboard em http://localhost:%d", porta)
    app.run(host="0.0.0.0", port=porta, debug=False)


if __name__ == "__main__":
    main()
