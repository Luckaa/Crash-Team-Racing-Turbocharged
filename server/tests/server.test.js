import test from 'node:test';
import assert from 'node:assert/strict';
import dgram from 'dgram';
import {
    CONFIG,
    generateUniqueRoomCode,
    createRoom,
    getPublicRooms,
    findRoomByCode,
    joinRoom,
    leaveRoom,
    heartbeatRoom,
    startRoomRace,
    pruneInactiveRooms,
    createHttpServer,
    createUdpRelayServer
} from '../src/server.js';

process.env.NODE_ENV = 'test';
process.env.RUN_AS_MODULE = 'true';

test('1. Room code generation format and alphabet', () => {
    const code = generateUniqueRoomCode();
    assert.equal(typeof code, 'string');
    assert.equal(code.length, 4);
    assert.match(code, /^[2345679ACDEFGHJKLMNPQRSTUVWXYZ]{4}$/);
});

test('2. Room creation, unique codes and initial host slot', () => {
    const { room, hostToken } = createRoom({ hostName: 'Lucas', gameMode: 'ARCADE' });
    assert.ok(room.id);
    assert.ok(room.code);
    assert.equal(room.players.length, 1);
    assert.equal(room.players[0].slot, 0);
    assert.equal(room.players[0].isHost, true);
    assert.equal(room.players[0].name, 'Lucas');
    assert.equal(room.players[0].token, hostToken);
    assert.equal(room.status, 'WAITING');

    const found = findRoomByCode(room.code);
    assert.equal(found.id, room.id);
});

test('3. Public vs Private rooms in listing', () => {
    const pub = createRoom({ hostName: 'PublicHost', isPrivate: false });
    const priv = createRoom({ hostName: 'PrivateHost', isPrivate: true });

    const list = getPublicRooms();
    const hasPub = list.some(r => r.code === pub.room.code);
    const hasPriv = list.some(r => r.code === priv.room.code);

    assert.equal(hasPub, true);
    assert.equal(hasPriv, false); // Private rooms must NOT show in public list
});

test('4. Lookup by valid and invalid room code', () => {
    const { room } = createRoom({ hostName: 'Player1' });
    assert.ok(findRoomByCode(room.code));
    assert.ok(findRoomByCode(room.code.toLowerCase())); // Case insensitive
    assert.equal(findRoomByCode('ZZZZ999'), null);
    assert.equal(findRoomByCode(''), null);
});

test('5. Player joining slots and room capacity (up to 4)', () => {
    const { room, hostToken } = createRoom({ hostName: 'Host' });

    // Join P2
    const p2 = joinRoom(room.code, { playerName: 'Player2' });
    assert.equal(p2.slot, 1);
    assert.equal(room.players.length, 2);

    // Join P3
    const p3 = joinRoom(room.code, { playerName: 'Player3' });
    assert.equal(p3.slot, 2);
    assert.equal(room.players.length, 3);

    // Join P4
    const p4 = joinRoom(room.code, { playerName: 'Player4' });
    assert.equal(p4.slot, 3);
    assert.equal(room.players.length, 4);

    // Join P5 (Room Full)
    const p5 = joinRoom(room.code, { playerName: 'Player5' });
    assert.equal(p5.error, 'ROOM_FULL');
    assert.equal(room.players.length, 4);
});

test('6. Leave room (Client leave vs Host leave)', () => {
    const { room, hostToken } = createRoom({ hostName: 'Host' });
    const p2 = joinRoom(room.code, { playerName: 'Player2' });

    // P2 leaves
    const leaveRes1 = leaveRoom(room.code, p2.token);
    assert.equal(leaveRes1.success, true);
    assert.equal(leaveRes1.closed, false);
    assert.equal(room.players.length, 1);

    // Host leaves -> Room dismantled
    const leaveRes2 = leaveRoom(room.code, hostToken);
    assert.equal(leaveRes2.success, true);
    assert.equal(leaveRes2.closed, true);
    assert.equal(findRoomByCode(room.code), null);
});

test('7. Race start authorization (Host only, min 2 players)', () => {
    const { room, hostToken } = createRoom({ hostName: 'Host' });

    // 1 player only -> cannot start
    const start1 = startRoomRace(room.code, hostToken);
    assert.equal(start1.error, 'NOT_ENOUGH_PLAYERS');

    // Add Player 2
    const p2 = joinRoom(room.code, { playerName: 'Client' });

    // Client tries to start -> Forbidden
    const start2 = startRoomRace(room.code, p2.token);
    assert.equal(start2.error, 'FORBIDDEN');

    // Host starts -> Success
    const start3 = startRoomRace(room.code, hostToken);
    assert.equal(start3.success, true);
    assert.equal(room.status, 'RACING');

    // No new players can join once RACING
    const p3 = joinRoom(room.code, { playerName: 'LatePlayer' });
    assert.equal(p3.error, 'GAME_STARTED');
});

test('8. Heartbeat and idle room pruning', () => {
    const { room, hostToken } = createRoom({ hostName: 'Host' });
    const hb = heartbeatRoom(room.code, hostToken);
    assert.equal(hb.success, true);

    // Fake lastSeen to 20 seconds ago
    room.players[0].lastSeen = Date.now() - 20000;
    pruneInactiveRooms();

    // Host timed out, room should be pruned
    assert.equal(findRoomByCode(room.code), null);
});

test('9. HTTP API Endpoints (Health, Create, List, Join, Leave)', async () => {
    const server = createHttpServer();
    await new Promise(r => server.listen(0, '127.0.0.1', r));
    const port = server.address().port;
    const baseUrl = `http://127.0.0.1:${port}`;

    try {
        // Health
        const resHealth = await fetch(`${baseUrl}/api/health`);
        assert.equal(resHealth.status, 200);
        const dataHealth = await resHealth.json();
        assert.equal(dataHealth.status, 'OK');

        // Create Room
        const resCreate = await fetch(`${baseUrl}/api/rooms`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ hostName: 'TestHost' })
        });
        assert.equal(resCreate.status, 201);
        const dataCreate = await resCreate.json();
        assert.ok(dataCreate.code);
        assert.ok(dataCreate.token);
        assert.equal(dataCreate.slot, 0);

        // List Rooms
        const resList = await fetch(`${baseUrl}/api/rooms`);
        const dataList = await resList.json();
        assert.ok(dataList.rooms.some(r => r.code === dataCreate.code));

        // Join Room
        const resJoin = await fetch(`${baseUrl}/api/rooms/${dataCreate.code}/join`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ playerName: 'Guest' })
        });
        assert.equal(resJoin.status, 200);
        const dataJoin = await resJoin.json();
        assert.equal(dataJoin.slot, 1);
        assert.ok(dataJoin.token);

        // Leave Room
        const resLeave = await fetch(`${baseUrl}/api/rooms/${dataCreate.code}/leave`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json', 'X-Session-Token': dataJoin.token }
        });
        assert.equal(resLeave.status, 200);
    } finally {
        await new Promise(r => server.close(r));
    }
});

test('10. UDP Relay packet forwarding and room isolation', async () => {
    const relaySocket = createUdpRelayServer();
    await new Promise(r => relaySocket.bind(0, '127.0.0.1', r));
    const relayPort = relaySocket.address().port;

    // Create Room A and Room B
    const roomA = createRoom({ hostName: 'HostA' });
    const p2A = joinRoom(roomA.room.code, { playerName: 'P2A' });

    const roomB = createRoom({ hostName: 'HostB' });
    const p2B = joinRoom(roomB.room.code, { playerName: 'P2B' });

    // Sockets for clients in Room A
    const clientHostA = dgram.createSocket('udp4');
    const clientP2A = dgram.createSocket('udp4');
    // Socket for client in Room B
    const clientP2B = dgram.createSocket('udp4');

    await Promise.all([
        new Promise(r => clientHostA.bind(0, '127.0.0.1', r)),
        new Promise(r => clientP2A.bind(0, '127.0.0.1', r)),
        new Promise(r => clientP2B.bind(0, '127.0.0.1', r))
    ]);

    try {
        // Register clientHostA and clientP2A with initial ping to relay
        // Header: [4 bytes roomCode] [1 byte slot] [1 byte flags] [payload]
        const makePacket = (code, slot, payloadStr) => {
            const buf = Buffer.alloc(6 + payloadStr.length);
            buf.write(code, 0, 4, 'ascii');
            buf[4] = slot;
            buf[5] = 0; // standard packet
            buf.write(payloadStr, 6);
            return buf;
        };

        // Send registration from Host A (slot 0) and P2A (slot 1)
        clientHostA.send(makePacket(roomA.room.code, 0, 'HELLO_HOST'), relayPort, '127.0.0.1');
        clientP2A.send(makePacket(roomA.room.code, 1, 'HELLO_P2A'), relayPort, '127.0.0.1');
        clientP2B.send(makePacket(roomB.room.code, 1, 'HELLO_P2B'), relayPort, '127.0.0.1');

        await new Promise(r => setTimeout(r, 50)); // let relay register endpoints

        // Now test: When Host A sends a gameplay packet, P2A should receive it, but P2B should NOT!
        let p2aReceived = null;
        let p2bReceived = false;

        clientP2A.on('message', (msg) => {
            p2aReceived = msg.subarray(6).toString();
        });
        clientP2B.on('message', () => {
            p2bReceived = true;
        });

        clientHostA.send(makePacket(roomA.room.code, 0, 'GAMEPLAY_FRAME_42'), relayPort, '127.0.0.1');

        await new Promise(r => setTimeout(r, 100));

        assert.equal(p2aReceived, 'GAMEPLAY_FRAME_42');
        assert.equal(p2bReceived, false, 'Packet leaked to another room!');
    } finally {
        clientHostA.close();
        clientP2A.close();
        clientP2B.close();
        relaySocket.close();
    }
});
