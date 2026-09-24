// ─────────────────────────────────────────────────────────────────────────────
//  LunaClient Backend Server
//  - Serves the React UI (static files from ../ui/dist)
//  - WebSocket for real-time communication with the frontend
//  - Named pipe client to talk to the injected DLL
//  - Spawns LunaInjector.exe for DLL injection
// ─────────────────────────────────────────────────────────────────────────────

const http = require('http');
const fs = require('fs');
const path = require('path');
const { WebSocketServer } = require('ws');
const net = require('net');
const { execFile, exec } = require('child_process');

const PORT = 3710;
const PIPE_NAME = '\\\\.\\pipe\\LunaClient';

// ── Protocol constants (must match shared/protocol.h) ────────────────────────
const LUNA_MAGIC = 0x4C554E41;
const MSG_TYPES = {
    Execute: 1,
    Output: 2,
    Error: 3,
    Status: 4,
    Ping: 5,
    Pong: 6,
};

// ── State ────────────────────────────────────────────────────────────────────
let pipeClient = null;
let pipeConnected = false;
let wsClients = new Set();
let reconnectTimer = null;

// ── Broadcast to all WebSocket clients ───────────────────────────────────────
function broadcast(type, data) {
    const msg = JSON.stringify({ type, ...data });
    for (const ws of wsClients) {
        if (ws.readyState === 1) ws.send(msg);
    }
}

// ── Named Pipe Protocol ──────────────────────────────────────────────────────
function makeHeader(msgType, payloadLen) {
    const buf = Buffer.alloc(12);
    buf.writeUInt32LE(LUNA_MAGIC, 0);
    buf.writeUInt32LE(msgType, 4);
    buf.writeUInt32LE(payloadLen, 8);
    return buf;
}

function sendToPipe(msgType, payload = '') {
    if (!pipeClient || !pipeConnected) return false;
    try {
        const payloadBuf = Buffer.from(payload, 'utf8');
        const header = makeHeader(msgType, payloadBuf.length);
        pipeClient.write(Buffer.concat([header, payloadBuf]));
        return true;
    } catch (e) {
        return false;
    }
}

// ── Pipe message parser ──────────────────────────────────────────────────────
class PipeParser {
    constructor() {
        this.buffer = Buffer.alloc(0);
    }

    feed(data) {
        this.buffer = Buffer.concat([this.buffer, data]);
        const messages = [];

        while (this.buffer.length >= 12) {
            const magic = this.buffer.readUInt32LE(0);
            if (magic !== LUNA_MAGIC) {
                this.buffer = Buffer.alloc(0);
                break;
            }

            const msgType = this.buffer.readUInt32LE(4);
            const payloadLen = this.buffer.readUInt32LE(8);
            const totalLen = 12 + payloadLen;

            if (this.buffer.length < totalLen) break;

            const payload = this.buffer.subarray(12, totalLen).toString('utf8');
            messages.push({ type: msgType, payload });
            this.buffer = this.buffer.subarray(totalLen);
        }

        return messages;
    }
}

// ── Named Pipe Connection ────────────────────────────────────────────────────
const parser = new PipeParser();

function connectPipe() {
    if (pipeConnected) return;

    pipeClient = net.connect(PIPE_NAME, () => {
        pipeConnected = true;
        console.log('[Pipe] Connected to DLL');
        broadcast('pipe_status', { connected: true });
    });

    pipeClient.on('data', (data) => {
        const messages = parser.feed(data);
        for (const msg of messages) {
            switch (msg.type) {
                case MSG_TYPES.Output:
                    broadcast('output', { text: msg.payload });
                    break;
                case MSG_TYPES.Error:
                    broadcast('error', { text: msg.payload });
                    break;
                case MSG_TYPES.Status:
                    broadcast('status', { text: msg.payload });
                    break;
                case MSG_TYPES.Pong:
                    broadcast('pong', {});
                    break;
            }
        }
    });

    pipeClient.on('error', () => {
        pipeConnected = false;
        pipeClient = null;
    });

    pipeClient.on('close', () => {
        pipeConnected = false;
        pipeClient = null;
        broadcast('pipe_status', { connected: false });
        scheduleReconnect();
    });
}

function scheduleReconnect() {
    if (reconnectTimer) return;
    reconnectTimer = setInterval(() => {
        if (!pipeConnected) {
            connectPipe();
        } else {
            clearInterval(reconnectTimer);
            reconnectTimer = null;
        }
    }, 2000);
}

// Start trying to connect immediately
scheduleReconnect();

// ── Static file server (serves built React app) ─────────────────────────────
const MIME_TYPES = {
    '.html': 'text/html',
    '.js': 'application/javascript',
    '.css': 'text/css',
    '.json': 'application/json',
    '.png': 'image/png',
    '.jpg': 'image/jpeg',
    '.svg': 'image/svg+xml',
    '.ico': 'image/x-icon',
    '.woff': 'font/woff',
    '.woff2': 'font/woff2',
    '.ttf': 'font/ttf',
    '.wasm': 'application/wasm',
};

const DIST_DIR = path.join(__dirname, '..', 'ui', 'dist');

const server = http.createServer((req, res) => {
    // API endpoint: inject
    if (req.method === 'POST' && req.url === '/api/inject') {
        let body = '';
        req.on('data', (chunk) => body += chunk);
        req.on('end', () => {
            const injectorPath = path.join(__dirname, '..', 'build', 'bin', 'LunaInjector.exe');

            // Check if injector exists, also try next to server
            const candidates = [
                injectorPath,
                path.join(__dirname, 'LunaInjector.exe'),
                path.join(__dirname, '..', 'LunaInjector.exe'),
            ];

            let exePath = null;
            for (const p of candidates) {
                if (fs.existsSync(p)) { exePath = p; break; }
            }

            if (!exePath) {
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({
                    success: false,
                    error: 'LunaInjector.exe not found. Build the C++ project first.'
                }));
                return;
            }

            let dllPath = '';
            try { dllPath = JSON.parse(body).dllPath || ''; } catch (e) {}

            const args = dllPath ? [dllPath] : [];

            execFile(exePath, args, { timeout: 15000 }, (err, stdout, stderr) => {
                res.writeHead(200, { 'Content-Type': 'application/json' });
                if (err) {
                    try {
                        // The injector outputs JSON even on error
                        res.end(stdout || JSON.stringify({ success: false, error: stderr || err.message }));
                    } catch (e) {
                        res.end(JSON.stringify({ success: false, error: err.message }));
                    }
                } else {
                    try {
                        res.end(stdout);
                    } catch (e) {
                        res.end(JSON.stringify({ success: true, message: 'Injected' }));
                    }
                }

                // After injection, start trying to connect pipe
                setTimeout(() => connectPipe(), 2000);
            });
        });
        return;
    }

    // Serve static files
    let filePath = req.url === '/' ? '/index.html' : req.url;
    filePath = path.join(DIST_DIR, filePath);

    // Security: prevent directory traversal
    if (!filePath.startsWith(DIST_DIR)) {
        res.writeHead(403);
        res.end('Forbidden');
        return;
    }

    const ext = path.extname(filePath).toLowerCase();
    const contentType = MIME_TYPES[ext] || 'application/octet-stream';

    fs.readFile(filePath, (err, data) => {
        if (err) {
            // SPA fallback: serve index.html for any unknown route
            fs.readFile(path.join(DIST_DIR, 'index.html'), (err2, indexData) => {
                if (err2) {
                    res.writeHead(404);
                    res.end('Not found');
                } else {
                    res.writeHead(200, { 'Content-Type': 'text/html' });
                    res.end(indexData);
                }
            });
        } else {
            res.writeHead(200, { 'Content-Type': contentType });
            res.end(data);
        }
    });
});

// ── WebSocket server ─────────────────────────────────────────────────────────
const wss = new WebSocketServer({ server });

wss.on('connection', (ws) => {
    wsClients.add(ws);
    console.log('[WS] Client connected');

    // Send current state
    ws.send(JSON.stringify({
        type: 'pipe_status',
        connected: pipeConnected,
    }));

    ws.on('message', (raw) => {
        try {
            const msg = JSON.parse(raw.toString());

            switch (msg.action) {
                case 'execute':
                    if (sendToPipe(MSG_TYPES.Execute, msg.script || '')) {
                        ws.send(JSON.stringify({ type: 'status', text: '[Execute] Script sent.' }));
                    } else {
                        ws.send(JSON.stringify({ type: 'error', text: '[Execute] Not connected to DLL.' }));
                    }
                    break;

                case 'ping':
                    sendToPipe(MSG_TYPES.Ping);
                    break;

                case 'reconnect':
                    connectPipe();
                    break;
            }
        } catch (e) {
            console.error('[WS] Bad message:', e.message);
        }
    });

    ws.on('close', () => {
        wsClients.delete(ws);
    });
});

// ── Start ────────────────────────────────────────────────────────────────────
server.listen(PORT, () => {
    console.log(`\n  ╔══════════════════════════════════════════╗`);
    console.log(`  ║        LunaClient v1.0.0                 ║`);
    console.log(`  ║        http://localhost:${PORT}              ║`);
    console.log(`  ╚══════════════════════════════════════════╝\n`);

    // Auto-open browser
    exec(`start http://localhost:${PORT}`);
});
