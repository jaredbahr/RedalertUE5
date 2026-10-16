import test from 'node:test';
import assert from 'node:assert/strict';
import { createSimulation } from './simulation.mjs';
import { createHost, connectClient } from './loopback.mjs';
import { smoke } from './smoke.mjs';

test('valid human and bot commands share ownership, economy, and target rules', () => {
  const sim = createSimulation();
  assert.equal(sim.command('human', { seq: 1, type: 'move', unit: 1, x: 2, y: 0 }).ok, true);
  assert.equal(sim.command('bot', { seq: 1, type: 'move', unit: 3, x: 2, y: 0 }).ok, true);
  assert.equal(sim.command('human', { seq: 2, type: 'attack', unit: 1, target: 3 }).ok, true);
  assert.equal(sim.command('bot', { seq: 2, type: 'attack', unit: 3, target: 1 }).ok, true);
  for (const owner of ['human', 'bot']) {
    for (let seq = 3; seq <= 6; seq++) assert.equal(sim.command(owner, { seq, type: 'produce' }).ok, true);
    const before = sim.snapshot();
    assert.equal(sim.command(owner, { seq: 7, type: 'produce' }).reason, 'resources');
    assert.deepEqual(sim.snapshot(), before);
  }
});

test('invalid, spoofed, out-of-order, replayed, and out-of-bounds commands do not mutate game state', () => {
  const sim = createSimulation();
  const initial = sim.snapshot();
  for (const value of [null, [], {}, { seq: 1, type: 'produce', owner: 'bot' }, { seq: 1, type: '__proto__' }])
    assert.equal(sim.command('human', value).reason, 'malformed');
  assert.equal(sim.command('stranger', { seq: 1, type: 'produce' }).reason, 'unknown-player');
  assert.equal(sim.command('human', { seq: 2, type: 'produce' }).reason, 'sequence');
  assert.equal(sim.command('human', { seq: 1, type: 'move', unit: 3, x: 1, y: 1 }).reason, 'ownership');
  assert.equal(sim.command('human', { seq: 1, type: 'produce' }).reason, 'sequence');
  assert.equal(sim.command('human', { seq: 2, type: 'move', unit: 1, x: Infinity, y: 1 }).reason, 'position');
  assert.equal(sim.command('human', { seq: 3, type: 'attack', unit: 1, target: 2 }).reason, 'target');
  assert.equal(sim.command('human', { seq: 4, type: 'attack', unit: 1, target: 999 }).reason, 'target');
  assert.deepEqual(sim.snapshot(), initial);
});

test('command allowance is configurable and resets on host tick', () => {
  const sim = createSimulation({ commandsPerTick: 1 });
  assert.equal(sim.command('human', { seq: 1, type: 'produce' }).ok, true);
  assert.equal(sim.command('human', { seq: 2, type: 'produce' }).reason, 'rate');
  sim.tick();
  assert.equal(sim.command('human', { seq: 3, type: 'produce' }).ok, true);
});

test('snapshots are detached copies and combat causes host-owned damage', () => {
  const sim = createSimulation({ unitsPerPlayer: 1 });
  const copy = sim.snapshot(); copy.players.human.credits = 999;
  assert.equal(sim.snapshot().players.human.credits, 200);
  sim.command('human', { seq: 1, type: 'attack', unit: 1, target: 2 });
  sim.command('bot', { seq: 1, type: 'attack', unit: 2, target: 1 });
  for (let i = 0; i < 20; i++) sim.tick();
  assert.ok(sim.snapshot().units.every(u => u.hp === 0));
});

test('wire connection identity rejects claiming another player', { timeout: 5000 }, async () => {
  const host = await createHost();
  let client;
  try {
    client = await connectClient(host.port);
    assert.equal(client.welcome.owner, 'human');
    assert.equal((await client.command({ seq: 1, type: 'move', unit: 3, x: 0, y: 0 })).reason, 'ownership');
    assert.equal((await client.command({ seq: 2, type: 'produce', owner: 'bot' })).reason, 'malformed');
  } finally { client?.close(); await host.close(); }
});

test('two TCP clients receive matching host state during movement, combat, and bot commands', { timeout: 10000 }, async () => {
  const result = await smoke();
  assert.equal(result.snapshots, result.ticks * 2);
  assert.ok(result.damagedTicks > 0 && result.movedTicks > 0 && result.botCommands > 0);
});
