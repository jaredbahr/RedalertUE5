// Original, dependency-free command-contract experiment. No Unreal integration.
export function createSimulation({ commandsPerTick = 32, unitsPerPlayer = 2 } = {}) {
  if (!Number.isSafeInteger(commandsPerTick) || commandsPerTick < 1 ||
      !Number.isSafeInteger(unitsPerPlayer) || unitsPerPlayer < 1) throw new Error('Invalid experiment configuration');
  const state = { tick: 0, players: { human: { credits: 200 }, bot: { credits: 200 } }, units: [] };
  let nextId = 1;
  for (const owner of Object.keys(state.players)) for (let i = 0; i < unitsPerPlayer; i++) {
    state.units.push({ id: nextId++, owner, x: owner === 'human' ? 0 : 8, y: i, hp: 100, order: null });
  }
  const sequences = { human: 0, bot: 0 };
  const counts = { human: 0, bot: 0 };
  const snapshot = () => structuredClone(state);
  function command(owner, cmd) {
    const reject = reason => ({ ok: false, reason });
    if (!Object.hasOwn(state.players, owner)) return reject('unknown-player');
    if (!cmd || typeof cmd !== 'object' || Array.isArray(cmd)) return reject('malformed');
    const fields = { move: ['seq', 'type', 'unit', 'x', 'y'], attack: ['seq', 'type', 'unit', 'target'], produce: ['seq', 'type'] };
    if (typeof cmd.type !== 'string' || !Object.hasOwn(fields, cmd.type)) return reject('malformed');
    if (Object.keys(cmd).some(k => !fields[cmd.type].includes(k)) ||
        fields[cmd.type].some(k => !Object.hasOwn(cmd, k))) return reject('malformed');
    if (!Number.isSafeInteger(cmd.seq) || cmd.seq !== sequences[owner] + 1) return reject('sequence');
    // Consume ordered requests even when gameplay validation rejects them.
    sequences[owner] = cmd.seq;
    if (++counts[owner] > commandsPerTick) return reject('rate');
    if (cmd.type === 'produce') {
      if (state.players[owner].credits < 50) return reject('resources');
      state.players[owner].credits -= 50;
      state.units.push({ id: nextId++, owner, x: owner === 'human' ? 0 : 8, y: 0, hp: 100, order: null });
      return { ok: true };
    }
    if (!Number.isSafeInteger(cmd.unit)) return reject('malformed');
    const unit = state.units.find(u => u.id === cmd.unit && u.hp > 0);
    if (!unit || unit.owner !== owner) return reject('ownership');
    if (cmd.type === 'move') {
      if (![cmd.x, cmd.y].every(n => Number.isFinite(n) && Math.abs(n) <= 1000)) return reject('position');
      unit.order = { type: 'move', x: cmd.x, y: cmd.y };
    } else {
      if (!Number.isSafeInteger(cmd.target)) return reject('malformed');
      const target = state.units.find(u => u.id === cmd.target && u.hp > 0);
      if (!target || target.owner === owner) return reject('target');
      unit.order = { type: 'attack', target: target.id };
    }
    return { ok: true };
  }
  function tick() {
    state.tick++;
    counts.human = counts.bot = 0;
    // Phase 1: derive every movement intent from the same starting state.
    const starting = snapshot().units.filter(u => u.hp > 0);
    const startingById = new Map(starting.map(u => [u.id, u]));
    const movement = new Map();
    const clearedOrders = new Set();
    for (const unit of starting) {
      const order = unit.order;
      if (!order) continue;
      const target = order.type === 'attack' ? startingById.get(order.target) : order;
      if (!target) { clearedOrders.add(unit.id); continue; }
      const dx = target.x - unit.x, dy = target.y - unit.y, distance = Math.hypot(dx, dy);
      if (distance > 0 && (order.type !== 'attack' || distance > 2)) {
        const step = Math.min(0.5, distance);
        movement.set(unit.id, { x: unit.x + dx / distance * step, y: unit.y + dy / distance * step });
      }
    }
    // Phase 2: commit movement together; no unit observes a partly moved world.
    for (const unit of state.units) {
      const position = movement.get(unit.id);
      if (position) Object.assign(unit, position);
      if (clearedOrders.has(unit.id)) unit.order = null;
    }
    // Phase 3: resolve range from post-movement state, then apply damage together.
    const postMove = snapshot().units.filter(u => u.hp > 0);
    const postMoveById = new Map(postMove.map(u => [u.id, u]));
    const damage = new Map();
    for (const unit of postMove) {
      if (unit.order?.type !== 'attack') continue;
      const target = postMoveById.get(unit.order.target);
      if (target && Math.hypot(target.x - unit.x, target.y - unit.y) <= 2)
        damage.set(target.id, (damage.get(target.id) ?? 0) + 10);
    }
    for (const unit of state.units) unit.hp = Math.max(0, unit.hp - (damage.get(unit.id) ?? 0));
    return snapshot();
  }
  return { command, tick, snapshot };
}
