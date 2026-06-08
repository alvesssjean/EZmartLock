/* CODIGO PRA FUNCIONAR JUNTO COM O ARDUINO
const http = require('http');
const fs = require('fs');
const path = require('path');
const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');

const PORTA_SERIAL = 'COM3'; // Modifique para a porta do seu Arduino
const BAUD_RATE = 9600;
const PORTA_WEB = 3000;

/*let estadoHardware = {
    distancia: "---",
    presenca: "---",
    tranca: "---",
    porta: "---",
    alerta: "NENHUM"
};

// Dados simulados para testar o Frontend sem o Arduino plugado
let estadoHardware = {
    distancia: 12,
    presenca: "SIM",
    tranca: "TRANCADA",
    porta: "ABERTA",
    alerta: "[ALERTA] A porta está ABERTA, mas a tranca esta ATIVADA!"
};

// --- CONEXÃO SERIAL COM O ARDUINO ---
try {
    const arduinoPort = new SerialPort({ path: PORTA_SERIAL, baudRate: BAUD_RATE });
    const parser = arduinoPort.pipe(new ReadlineParser({ delimiter: '\r\n' }));

    console.log(`[SERIAL] Conectando ao Arduino em ${PORTA_SERIAL}...`);

    parser.on('data', (linha) => {
        linha = linha.trim();
        if (linha.startsWith('{') && linha.endsWith('}')) {
            try {
                estadoHardware = JSON.parse(linha);
                console.log(`[LOG] Porta: ${estadoHardware.porta} | Alerta: ${estadoHardware.alerta}`);
            } catch (e) {}
        }
    });
} catch (err) {
    console.error("[ERRO SERIAL]", err.message);
}

// --- SERVIDOR HTTP NATIVO ---
const server = http.createServer((req, res) => {
    
    // Rota que o script do HTML vai chamar para atualizar o DOM
    if (req.url === '/api/status' && req.method === 'GET') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify(estadoHardware));
    }

    // Rota que lê o arquivo index.html físico e envia para o navegador
    if (req.url === '/' && req.method === 'GET') {
        fs.readFile(path.join(__dirname, 'index.html'), (err, conteudo) => {
            if (err) {
                res.writeHead(500, { 'Content-Type': 'text/plain; charset=utf-8' });
                return res.end('Erro interno ao carregar o arquivo HTML.');
            }
            res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
            res.end(conteudo);
        });
        return;
    }

    res.writeHead(404, { 'Content-Type': 'text/plain' });
    res.end('404 - Não Encontrado');
});

server.listen(PORTA_WEB, () => {
    console.log(`[SERVIDOR] Rodando em http://localhost:${PORTA_WEB}`);
});
*/
// CODIGO PARA FUNCIONAR SEM O ARDUINO
const http = require('http');
const fs = require('fs');
const path = require('path');

const PORTA_WEB = 3000;

// Dados simulados para testar o Frontend sem o Arduino plugado
let estadoHardware = {
    distancia: 15,
    presenca: "SIM",
    tranca: "TRANCADA",
    porta: "ABERTA",
    alerta: "[ALERTA] A porta está ABERTA, mas a tranca esta ATIVADA!"
};

// --- SERVIDOR HTTP NATIVO ---
const server = http.createServer((req, res) => {
    
    // Rota que o script do HTML vai chamar para atualizar o DOM
    if (req.url === '/api/status' && req.method === 'GET') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify(estadoHardware));
    }

    // Rota que lê o arquivo index.html físico e envia para o navegador
    if (req.url === '/' && req.method === 'GET') {
        fs.readFile(path.join(__dirname, 'index.html'), (err, conteudo) => {
            if (err) {
                res.writeHead(500, { 'Content-Type': 'text/plain; charset=utf-8' });
                return res.end('Erro interno ao carregar o arquivo HTML.');
            }
            res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
            res.end(conteudo);
        });
        return;
    }

    res.writeHead(404, { 'Content-Type': 'text/plain' });
    res.end('404 - Não Encontrado');
});

server.listen(PORTA_WEB, () => {
    console.log(`[SERVIDOR] Rodando em http://localhost:${PORTA_WEB}`);
});