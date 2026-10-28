import http from 'http';
import dgram from 'dgram';
import crypto from 'crypto';

// Load environment variables from .env if present
try {
    if (typeof process.loadEnvFile === 'function') {
        process.loadEnvFile();
    }
} catch (e) {
    // Ignore if .env doesn't exist
}

// Configuration
export const CONFIG = {
    HTTP_PORT: parseInt(process.env.HTTP_PORT || '8080', 10),
    UDP_PORT: parseInt(process.env.UDP_PORT || '31850', 10),
    PUBLIC_HOST: process.env.PUBLIC_HOST || '127.0.0.1',
    PUBLIC_UDP_PORT: parseInt(process.env.PUBLIC_UDP_PORT || process.env.UDP_PORT || '31850', 10),
    HEARTBEAT_TIMEOUT_MS: parseInt(process.env.HEARTBEAT_TIMEOUT_MS || '30000', 10),
    ROOM_CLEANUP_INTERVAL_MS: 2000,
    MAX_PLAYERS_PER_ROOM: 4,
    CODE_ALPHABET: '2345679ACDEFGHJKLMNPQRSTUVWXYZ', // Avoids 0/O, 1/I, 8/B visual ambiguities
};

// In-Memory Storage
const roomsById = new Map();
const roomsByCode = new Map();

// Helper: Generate clean 4-character code with collision prevention
export function generateUniqueRoomCode() {
    const chars = CONFIG.CODE_ALPHABET;
    for (let attempt = 0; attempt < 500; attempt++) {
        let code = '';
        const bytes = crypto.randomBytes(4);
        for (let i = 0; i < 4; i++) {
            code += chars[bytes[i] % chars.length];
        }
        if (!roomsByCode.has(code)) {
            return code;
        }
    }
    throw new Error('Room code collision exhaustion: too many active rooms');
}

// Room Factory
export function createRoom({ hostName = 'Host', gameMode = 'ARCADE', isPrivate = false } = {}) {
    const roomId = crypto.randomUUID();
    const code = generateUniqueRoomCode();
    const hostToken = crypto.randomBytes(16).toString('hex');
    const now = Date.now();

    const hostPlayer = {
        slot: 0,
        token: hostToken,
        name: hostName.slice(0, 24),
        isHost: true,
        lastSeen: now,
        udpEndpoint: null
    };

    const room = {
        id: roomId,
        code: code,
        name: `${hostName}'s Room`.slice(0, 32),
        gameMode: gameMode,
        isPrivate: Boolean(isPrivate),
        maxPlayers: CONFIG.MAX_PLAYERS_PER_ROOM,
        status: 'WAITING', // 'WAITING' | 'RACING' | 'CLOSED'
        createdAt: now,
        lastActivity: now,
        players: [hostPlayer]
    };

    roomsById.set(roomId, room);
    roomsByCode.set(code, room);
    return { room, hostToken };
}

export function getPublicRooms() {
    const result = [];
    for (const room of roomsByCode.values()) {
        if (!room.isPrivate && room.status === 'WAITING' && room.players.length < room.maxPlayers) {
            result.push({
                code: room.code,
                name: room.name,
                currentPlayers: room.players.length,
                maxPlayers: room.maxPlayers,
                gameMode: room.gameMode,
                status: room.status,
                hasPassword: false
            });
        }
    }
    return result;
}

export function findRoomByCode(code) {
    if (!code || typeof code !== 'string') return null;
    return roomsByCode.get(code.trim().toUpperCase()) || null;
}

export function joinRoom(code, { playerName = 'Player' } = {}) {
    const room = findRoomByCode(code);
    if (!room) {
        return { error: 'NOT_FOUND', message: 'Sala nao encontrada' };
    }
    if (room.status !== 'WAITING') {
        return { error: 'GAME_STARTED', message: 'Partida ja iniciada' };
    }
    if (room.players.length >= room.maxPlayers) {
        return { error: 'ROOM_FULL', message: 'Sala cheia' };
    }

    // Assign next available slot (0..3)
    const takenSlots = new Set(room.players.map(p => p.slot));
    let nextSlot = -1;
    for (let s = 1; s < room.maxPlayers; s++) {
        if (!takenSlots.has(s)) {
            nextSlot = s;
            break;
        }
    }
    if (nextSlot === -1) {
        return { error: 'ROOM_FULL', message: 'Nenhuma vaga restante' };
    }

    const token = crypto.randomBytes(16).toString('hex');
    const player = {
        slot: nextSlot,
        token: token,
        name: playerName.slice(0, 24),
        isHost: false,
        lastSeen: Date.now(),
        udpEndpoint: null
    };

    room.players.push(player);
    room.lastActivity = Date.now();
    return {
        room,
        player,
        slot: nextSlot,
        token: token
    };
}

export function leaveRoom(code, token) {
    const room = findRoomByCode(code);
    if (!room) return { success: false, error: 'NOT_FOUND' };

    const idx = room.players.findIndex(p => p.token === token);
    if (idx === -1) return { success: false, error: 'UNAUTHORIZED' };

    const leavingPlayer = room.players[idx];
    if (leavingPlayer.isHost) {
        // If Host leaves, dismantle the room
        room.status = 'CLOSED';
        roomsByCode.delete(room.code);
        roomsById.delete(room.id);
        return { success: true, closed: true };
    } else {
        room.players.splice(idx, 1);
        room.lastActivity = Date.now();
        return { success: true, closed: false };
    }
}

export function heartbeatRoom(code, token) {
    const room = findRoomByCode(code);
    if (!room) return { success: false, error: 'NOT_FOUND' };

    const player = room.players.find(p => p.token === token);
    if (!player) return { success: false, error: 'UNAUTHORIZED' };

    const now = Date.now();
    player.lastSeen = now;
    room.lastActivity = now;
    return {
        success: true,
        status: room.status,
        playerCount: room.players.length,
        maxPlayers: room.maxPlayers
    };
}

export function startRoomRace(code, token) {
    const room = findRoomByCode(code);
    if (!room) return { success: false, error: 'NOT_FOUND' };

    const player = room.players.find(p => p.token === token);
    if (!player || !player.isHost) {
        return { success: false, error: 'FORBIDDEN', message: 'Apenas o host pode iniciar a partida' };
    }
    if (room.players.length < 2) {
        return { success: false, error: 'NOT_ENOUGH_PLAYERS', message: 'Minimo de 2 jogadores para iniciar' };
    }

    room.status = 'RACING';
    room.lastActivity = Date.now();
    return { success: true, status: room.status };
}

// Cleanup Inactive Rooms & Dead Players
export function pruneInactiveRooms() {
    const now = Date.now();
    for (const [code, room] of roomsByCode.entries()) {
        // While the race is actively running, do not prune the room based on HTTP heartbeats
        if (room.status === 'RACING') {
            // Keep room alive up to 30 minutes of total race inactivity
            if ((now - room.lastActivity) > 30 * 60 * 1000) {
                room.status = 'CLOSED';
                roomsByCode.delete(code);
                roomsById.delete(room.id);
                console.log(`[LOBBY] Sala ${code} (em corrida) finalizada por tempo limite excedido.`);
            }
            continue;
        }

        // Filter players whose heartbeat timed out (waiting in lobby)
        const alivePlayers = room.players.filter(p => (now - p.lastSeen) <= CONFIG.HEARTBEAT_TIMEOUT_MS);
        const hostAlive = alivePlayers.some(p => p.isHost);

        if (!hostAlive || alivePlayers.length === 0) {
            room.status = 'CLOSED';
            roomsByCode.delete(code);
            roomsById.delete(room.id);
            console.log(`[LOBBY] Sala ${code} expirada e removida por inatividade.`);
        } else if (alivePlayers.length !== room.players.length) {
            console.log(`[LOBBY] Removendo jogadores desconectados da sala ${code} (${room.players.length} -> ${alivePlayers.length})`);
            room.players = alivePlayers;
            room.lastActivity = now;
        }
    }
}

// JSON Body Parser helper
function readJsonBody(req) {
    return new Promise((resolve, reject) => {
        let body = '';
        req.on('data', chunk => {
            body += chunk;
            if (body.length > 65536) {
                req.socket.destroy();
                reject(new Error('Payload too large'));
            }
        });
        req.on('end', () => {
            if (!body) return resolve({});
            try {
                resolve(JSON.parse(body));
            } catch (err) {
                reject(err);
            }
        });
        req.on('error', reject);
    });
}

function sendJson(res, statusCode, data) {
    const json = JSON.stringify(data);
    res.writeHead(statusCode, {
        'Content-Type': 'application/json; charset=utf-8',
        'Content-Length': Buffer.byteLength(json),
        'Access-Control-Allow-Origin': '*',
        'Access-Control-Allow-Headers': 'Content-Type, Authorization, X-Session-Token',
        'Access-Control-Allow-Methods': 'GET, POST, OPTIONS'
    });
    res.end(json);
}

// HTTP Server
export function createHttpServer() {
    return http.createServer(async (req, res) => {
        const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
        const pathname = url.pathname;

        // CORS Preflight
        if (req.method === 'OPTIONS') {
            res.writeHead(204, {
                'Access-Control-Allow-Origin': '*',
                'Access-Control-Allow-Headers': 'Content-Type, Authorization, X-Session-Token',
                'Access-Control-Allow-Methods': 'GET, POST, OPTIONS'
            });
            return res.end();
        }

        try {
            // Healthcheck
            if (req.method === 'GET' && pathname === '/api/health') {
                return sendJson(res, 200, {
                    status: 'OK',
                    activeRooms: roomsByCode.size,
                    publicHost: CONFIG.PUBLIC_HOST,
                    relayUdpPort: CONFIG.PUBLIC_UDP_PORT
                });
            }

            // List Rooms
            if (req.method === 'GET' && pathname === '/api/rooms') {
                const list = getPublicRooms();
                return sendJson(res, 200, { rooms: list, count: list.length });
            }

            // Create Room
            if (req.method === 'POST' && pathname === '/api/rooms') {
                const body = await readJsonBody(req);
                const { room, hostToken } = createRoom({
                    hostName: body.hostName || 'Host',
                    gameMode: body.gameMode || 'ARCADE',
                    isPrivate: body.isPrivate || false
                });

                console.log(`[LOBBY] Nova sala criada: [${room.code}] (Host: ${room.players[0].name})`);
                return sendJson(res, 201, {
                    success: true,
                    code: room.code,
                    roomId: room.id,
                    token: hostToken,
                    slot: 0,
                    relayHost: CONFIG.PUBLIC_HOST,
                    relayPort: CONFIG.PUBLIC_UDP_PORT,
                    room: {
                        code: room.code,
                        name: room.name,
                        currentPlayers: 1,
                        maxPlayers: room.maxPlayers,
                        gameMode: room.gameMode
                    }
                });
            }

            // Match /api/rooms/:code routes
            const roomCodeMatch = pathname.match(/^\/api\/rooms\/([A-Za-z0-9]{4})(\/.*)?$/);
            if (roomCodeMatch) {
                const code = roomCodeMatch[1].toUpperCase();
                const subPath = roomCodeMatch[2] || '';

                // Get Room Info
                if (req.method === 'GET' && subPath === '') {
                    const room = findRoomByCode(code);
                    if (!room) {
                        return sendJson(res, 404, { error: 'NOT_FOUND', message: 'Sala nao encontrada' });
                    }
                    const token = req.headers['x-session-token'] || req.headers['authorization'];
                    if (token) {
                        const p = room.players.find(pl => pl.token === token);
                        if (p) {
                            const now = Date.now();
                            p.lastSeen = now;
                            room.lastActivity = now;
                        }
                    }
                    return sendJson(res, 200, {
                        code: room.code,
                        name: room.name,
                        status: room.status,
                        currentPlayers: room.players.length,
                        maxPlayers: room.maxPlayers,
                        gameMode: room.gameMode,
                        isPrivate: room.isPrivate,
                        players: room.players.map(p => ({ slot: p.slot, name: p.name, isHost: p.isHost }))
                    });
                }

                // Join Room
                if (req.method === 'POST' && subPath === '/join') {
                    const body = await readJsonBody(req);
                    const result = joinRoom(code, { playerName: body.playerName || 'Player' });
                    if (result.error) {
                        const status = result.error === 'NOT_FOUND' ? 404 : 409;
                        return sendJson(res, status, result);
                    }
                    console.log(`[LOBBY] Jogador '${result.player.name}' entrou na sala [${code}] (Slot: ${result.slot})`);
                    return sendJson(res, 200, {
                        success: true,
                        code: result.room.code,
                        roomId: result.room.id,
                        token: result.token,
                        slot: result.slot,
                        relayHost: CONFIG.PUBLIC_HOST,
                        relayPort: CONFIG.PUBLIC_UDP_PORT,
                        totalPlayers: result.room.players.length
                    });
                }

                // Helper to extract session token from header or body
                const getAuthToken = (body) => {
                    return req.headers['x-session-token'] ||
                        (req.headers['authorization']?.replace(/^Bearer\s+/i, '')) ||
                        body?.token || '';
                };

                // Leave Room
                if (req.method === 'POST' && subPath === '/leave') {
                    const body = await readJsonBody(req);
                    const token = getAuthToken(body);
                    const result = leaveRoom(code, token);
                    if (!result.success) {
                        return sendJson(res, result.error === 'NOT_FOUND' ? 404 : 403, result);
                    }
                    console.log(`[LOBBY] Jogador saiu da sala [${code}] (Fechada: ${result.closed})`);
                    return sendJson(res, 200, result);
                }

                // Heartbeat
                if (req.method === 'POST' && subPath === '/heartbeat') {
                    const body = await readJsonBody(req);
                    const token = getAuthToken(body);
                    const result = heartbeatRoom(code, token);
                    if (!result.success) {
                        return sendJson(res, result.error === 'NOT_FOUND' ? 404 : 403, result);
                    }
                    return sendJson(res, 200, result);
                }

                // Start Game
                if (req.method === 'POST' && subPath === '/start') {
                    const body = await readJsonBody(req);
                    const token = getAuthToken(body);
                    const result = startRoomRace(code, token);
                    if (!result.success) {
                        return sendJson(res, result.error === 'NOT_FOUND' ? 404 : 403, result);
                    }
                    console.log(`[LOBBY] Host iniciou a corrida na sala [${code}]!`);
                    return sendJson(res, 200, result);
                }
            }

            // Route Not Found
            return sendJson(res, 404, { error: 'NOT_FOUND', message: 'Rota nao encontrada' });
        } catch (err) {
            console.error('[HTTP] Server error:', err);
            return sendJson(res, 500, { error: 'INTERNAL_ERROR', message: err.message });
        }
    });
}

// UDP Relay Server
// Datagram Header (6 bytes):
// [0..3]: Room Code (ASCII, e.g. "XK82")
// [4]:    Sender Slot (0 = Host, 1..3 = Clients)
// [5]:    Packet Type or Relay Flags
// [6..N]: Original CTR native adhoc packet
export function createUdpRelayServer() {
    const socket = dgram.createSocket('udp4');

    socket.on('message', (msg, rinfo) => {
        // Minimum header is 6 bytes
        if (msg.length < 6 || msg.length > 2048) {
            return;
        }

        const roomCode = msg.subarray(0, 4).toString('ascii').toUpperCase();
        const slot = msg[4];

        if (slot > 3) {
            return; // Invalid player slot
        }

        const room = roomsByCode.get(roomCode);
        if (!room) {
            return; // Room not found
        }

        // Validate sender slot and register/update its UDP endpoint
        const player = room.players.find(p => p.slot === slot);
        if (!player) {
            return; // Player not in room
        }

        if (!player.udpEndpoint ||
            player.udpEndpoint.address !== rinfo.address ||
            player.udpEndpoint.port !== rinfo.port) {
            player.udpEndpoint = { address: rinfo.address, port: rinfo.port };
        }
        player.lastSeen = Date.now();
        room.lastActivity = Date.now();

        // Forward packet to all OTHER players in the same room
        for (const recipient of room.players) {
            if (recipient.slot !== slot && recipient.udpEndpoint) {
                socket.send(msg, recipient.udpEndpoint.port, recipient.udpEndpoint.address, (err) => {
                    if (err) {
                        console.error(`[RELAY] Erro encaminhando pacote para slot ${recipient.slot}:`, err.message);
                    }
                });
            }
        }
    });

    socket.on('error', (err) => {
        console.error('[RELAY] Socket error:', err);
    });

    return socket;
}

import { fileURLToPath } from 'url';
const isMain = process.argv[1] && (fileURLToPath(import.meta.url) === process.argv[1] || process.argv[1].endsWith('server.js'));

// Runner
if (isMain) {
    const httpServer = createHttpServer();
    const udpSocket = createUdpRelayServer();
    const cleanupTimer = setInterval(pruneInactiveRooms, CONFIG.ROOM_CLEANUP_INTERVAL_MS);

    httpServer.listen(CONFIG.HTTP_PORT, '0.0.0.0', () => {
        console.log('================================================================');
        console.log('   CRASH TEAM RACING: TURBOCHARGED - MULTIPLAYER MASTER SERVER  ');
        console.log('================================================================');
        console.log(`[HTTP] Lobby API escutando em: http://0.0.0.0:${CONFIG.HTTP_PORT}`);
        console.log(`[UDP]  Relay Server escutando em: 0.0.0.0:${CONFIG.UDP_PORT}`);
        console.log(`[INFO] Public Host: ${CONFIG.PUBLIC_HOST}:${CONFIG.PUBLIC_UDP_PORT}`);
        console.log('Servidor pronto para conexoes! Pressione Ctrl+C para encerrar.');
        console.log('----------------------------------------------------------------');
    });

    udpSocket.bind(CONFIG.UDP_PORT, '0.0.0.0');

    const shutdown = () => {
        console.log('\n[INFO] Encerrando servidor com seguranca...');
        clearInterval(cleanupTimer);
        httpServer.close();
        udpSocket.close();
        process.exit(0);
    };

    process.on('SIGINT', shutdown);
    process.on('SIGTERM', shutdown);
}
