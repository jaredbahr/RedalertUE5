import net from 'node:net';
import { once } from 'node:events';
import { createSimulation } from './simulation.mjs';

// Loopback only. Connection slots bind identity; callers cannot claim an owner.
export async function createHost(options = {}) {
  const simulation = createSimulation(options);
  const peers = new Set();
  let connectionCount = 0;
  const server = net.createServer(socket => {
    if (connectionCount >= 2) { socket.destroy(); return; }
    const owner = ['human', 'bot'][connectionCount++];
    peers.add(socket);
    socket.setEncoding('utf8');
    socket.on('error', () => {});
    socket.on('close', () => peers.delete(socket));
    let buffer = '';
    socket.on('data', chunk => {
      buffer += chunk;
      // Fixed framing bound is defensive parser policy, not a game unit cap.
      if (Buffer.byteLength(buffer) > 65536) { socket.destroy(); return; }
      let newline;
      while ((newline = buffer.indexOf('\n')) >= 0) {
        const line = buffer.slice(0, newline); buffer = buffer.slice(newline + 1);
        let response;
        try { response = simulation.command(owner, JSON.parse(line)); }
        catch { response = { ok: false, reason: 'json' }; }
        send(socket, { kind: 'ack', ...response });
      }
    });
    send(socket, { kind: 'welcome', owner, state: simulation.snapshot() });
  });
  function send(socket, value) {
    if (socket.writableLength > 1048576) { socket.destroy(); return; }
    socket.write(JSON.stringify(value) + '\n');
  }
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  return {
    port: server.address().port,
    snapshot: simulation.snapshot,
    tick() { const state = simulation.tick(); for (const peer of peers) send(peer, { kind: 'state', state }); return state; },
    async close() { for (const peer of peers) peer.destroy(); await new Promise(resolve => server.close(resolve)); }
  };
}

export async function connectClient(port) {
  const socket = net.createConnection({ port, host: '127.0.0.1' });
  const inbox = [], waiting = [];
  let buffer = '';
  socket.setEncoding('utf8');
  socket.on('data', chunk => {
    buffer += chunk;
    let newline;
    while ((newline = buffer.indexOf('\n')) >= 0) {
      const message = JSON.parse(buffer.slice(0, newline)); buffer = buffer.slice(newline + 1);
      const waiter = waiting.shift(); if (waiter) waiter(message); else inbox.push(message);
    }
  });
  // Test/demo helper: callers must preserve request/response ordering.
  const receive = () => inbox.length ? Promise.resolve(inbox.shift()) : new Promise(resolve => waiting.push(resolve));
  await once(socket, 'connect');
  const welcome = await receive();
  return { welcome, receive, async command(cmd) { socket.write(JSON.stringify(cmd) + '\n'); return receive(); }, close() { socket.destroy(); } };
}
