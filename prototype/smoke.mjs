import assert from 'node:assert/strict';
import { performance } from 'node:perf_hooks';
import { pathToFileURL } from 'node:url';
import { createHost, connectClient } from './loopback.mjs';

export async function smoke({ unitsPerPlayer = 6, ticks = 24 } = {}) {
  const started = performance.now();
  const host = await createHost({ unitsPerPlayer });
  const clients = [];
  let snapshots = 0, bytes = 0, movedTicks = 0, damagedTicks = 0, combinedTicks = 0, botCommands = 0;
  try {
    clients.push(await connectClient(host.port));
    clients.push(await connectClient(host.port));
    assert.deepEqual(clients[0].welcome.state, clients[1].welcome.state);
    const sequences = { human: 0, bot: 0 };
    let previous = host.snapshot();
    for (let tick = 0; tick < ticks; tick++) {
      for (const client of clients) {
        const owner = client.welcome.owner;
        const own = previous.units.filter(u => u.owner === owner && u.hp > 0);
        const enemies = previous.units.filter(u => u.owner !== owner && u.hp > 0);
        if (!enemies.length) continue;
        // Both drivers use the same wire format. The bot simply picks live targets.
        for (let i = 0; i < own.length; i++) {
          const unit = own[i], target = enemies[i % enemies.length];
          const cmd = owner === 'human' && i === 0
            ? { type: 'move', unit: unit.id, x: 4, y: (tick % 4) - 2 }
            : { type: 'attack', unit: unit.id, target: target.id };
          const ack = await client.command({ seq: ++sequences[owner], ...cmd });
          assert.equal(ack.ok, true);
          if (owner === 'bot') botCommands++;
        }
      }
      const authoritative = host.tick();
      const replicated = await Promise.all(clients.map(c => c.receive()));
      for (const message of replicated) {
        assert.equal(message.kind, 'state');
        assert.deepEqual(message.state, authoritative);
        snapshots++; bytes += Buffer.byteLength(JSON.stringify(message) + '\n');
      }
      const moved = authoritative.units.some((u, i) => u.x !== previous.units[i].x || u.y !== previous.units[i].y);
      const damaged = authoritative.units.some((u, i) => u.hp < previous.units[i].hp);
      if (moved) movedTicks++;
      if (damaged) damagedTicks++;
      if (moved && damaged) combinedTicks++;
      previous = authoritative;
    }
    assert.ok(combinedTicks > 0 && botCommands > 0);
    return { experiment: 'toy-loopback-combined', initialUnits: unitsPerPlayer * 2, ticks,
      clients: clients.length, movedTicks, damagedTicks, combinedTicks, botCommands, snapshots,
      snapshotPayloadBytes: bytes, wallMs: Math.round((performance.now() - started) * 100) / 100 };
  } finally { for (const client of clients) client.close(); await host.close(); }
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  // Fail a stuck smoke run instead of leaving a server running indefinitely.
  const watchdog = setTimeout(() => { console.error('Smoke timeout'); process.exit(1); }, 10000);
  try { console.log(JSON.stringify(await smoke(), null, 2)); }
  finally { clearTimeout(watchdog); }
}
