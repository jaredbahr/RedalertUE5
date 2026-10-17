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

test('command types must be exact string enums before field validation', () => {
  const sim = createSimulation();
  const initial = sim.snapshot();
  for (const type of [['attack'], { toString: 'attack' }, null]) {
    assert.equal(sim.command('human', { seq: 1, type, unit: 1, target: 3 }).reason, 'malformed');
    assert.deepEqual(sim.snapshot(), initial);
  }
  // Malformed envelopes must not consume the next valid request sequence.
  assert.equal(sim.command('human', { seq: 1, type: 'attack', unit: 1, target: 3 }).ok, true);
});

test('phased movement and damage give mirrored physical outcomes regardless of unit insertion order', () => {
  function duel(mirrored) {
    const sim = createSimulation({ unitsPerPlayer: 1 });
    sim.command('human', { seq: 1, type: 'move', unit: 1, x: mirrored ? 5 : 2.5, y: 0 });
    sim.command('bot', { seq: 1, type: 'move', unit: 2, x: mirrored ? 2.5 : 5, y: 0 });
    for (let i = 0; i < 11; i++) sim.tick();
    sim.command('human', { seq: 2, type: 'attack', unit: 1, target: 2 });
    sim.command('bot', { seq: 2, type: 'attack', unit: 2, target: 1 });
    const history = [];
    for (let i = 0; i < 11; i++) {
      // Physical left/right units swap insertion order and identities between runs.
      history.push(sim.tick().units.map(({ x, y, hp }) => ({ x, y, hp })).sort((a, b) => a.x - b.x));
    }
    return history;
  }
  const normal = duel(false), mirrored = duel(true);
  assert.deepEqual(normal, mirrored);
  assert.deepEqual(normal[0], [{ x: 3, y: 0, hp: 90 }, { x: 4.5, y: 0, hp: 90 }]);
  assert.ok(normal.at(-1).every(unit => unit.hp === 0));
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

test('TCP wire rejects non-string command types without mutating state or consuming sequence', { timeout: 5000 }, async () => {
  const host = await createHost();
  let client;
  try {
    client = await connectClient(host.port);
    const initial = host.snapshot();
    for (const type of [['attack'], { toString: 'attack' }, null]) {
      assert.equal((await client.command({ seq: 1, type, unit: 1, target: 3 })).reason, 'malformed');
      assert.deepEqual(host.snapshot(), initial);
    }
    assert.equal((await client.command({ seq: 1, type: 'attack', unit: 1, target: 3 })).ok, true);
  } finally { client?.close(); await host.close(); }
});

test('two TCP clients receive matching host state during movement, combat, and bot commands', { timeout: 10000 }, async () => {
  const result = await smoke();
  assert.equal(result.snapshots, result.ticks * 2);
  assert.ok(result.damagedTicks > 0 && result.movedTicks > 0 && result.botCommands > 0);
});
