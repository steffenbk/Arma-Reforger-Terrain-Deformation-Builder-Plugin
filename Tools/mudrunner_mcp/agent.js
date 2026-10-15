// MudRunner wheel-in-mud telemetry agent (Frida). Loaded by server.py.
// Hooks sub_140930110(mud, wheel) - the per-wheel, per-physics-step mud/water force function
// (IDA 2026-10-15, MudRunner.exe image base 0x140000000; research/2026-10-14-mudrunner-ida.md).
// On leave the wheel's depth fields are fresh, so everything is read there.

'use strict';

const MOD = 'MudRunner.exe';
const RVA_WHEEL = 0x930110;
const SIG_WHEEL = '48 8B C4 55 53 56 57 48 8D A8 08 FF FF FF 48 81 EC D8 01 00 00 48 C7 45 A8 FE FF FF FF 0F 29 70 C8 0F 29 78 B8 44 0F 29 40 A8 44 0F 29 48 98 44';
const RVA_BODY = 0x9310D0;    // sub_1409310D0(mud, truck|0, body) - truck body in mud
const SIG_BODY = '48 8B C4 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 88 FB FF FF 48 81 EC 40 05 00 00 48 C7 85 F0 02 00 00 FE FF FF FF 48 89 58 18 0F 29 70 B8 0F';
const MAX_HZ = 60;            // per wheel + mud object
const FLUSH_MS = 100;

let buf = [];
let bbuf = [];
const blast = {};
const last = {};
let calls = 0;

function findFn(rva, sig) {
  const m = Process.getModuleByName(MOD);
  const guess = m.base.add(rva);
  try {
    const want = sig.split(' ').map(h => parseInt(h, 16));
    const got = new Uint8Array(guess.readByteArray(want.length));
    let ok = true;
    for (let i = 0; i < want.length; i++) if (got[i] !== want[i]) { ok = false; break; }
    if (ok) return { addr: guess, how: 'rva' };
  } catch (e) { }
  const hits = Memory.scanSync(m.base, m.size, sig);
  if (hits.length === 1) return { addr: hits[0].address, how: 'scan' };
  return { addr: null, how: 'not found (' + hits.length + ' scan hits)' };
}

function f(p, off) { return p.add(off).readFloat(); }

// One row per wheel (max MAX_HZ). The game calls us once per wheel per mud/water object, and each call only
// writes its own depth (+272 mud, +276 water), so both depths are read every time. Substance values come from
// the last MUD (non-water) object that touched the wheel; water = 1 if a water object touched it in the last 0.5 s.
const mudOf = {};    // wheel -> last non-water mud object
const waterAt = {};  // wheel -> time a water object last touched it

function sample(mud, w) {
  const now = Date.now();
  const key = w.toString();
  if (mud.add(60).readU32() === 105) waterAt[key] = now;
  else mudOf[key] = mud;
  if (last[key] && now - last[key] < 1000 / MAX_HZ) return;
  last[key] = now;

  const m = mudOf[key] || mud;
  const s = {
    t: now,
    wheel: key,
    mud: m.toString(),
    water: (waterAt[key] && now - waterAt[key] < 500) ? 1 : 0,
    // substance (mud object +40..+52): SubstanceFriction, LinResistance, FullPushDepth, PushResistance
    sub_fric: f(m, 40), lin_res: f(m, 44), full_push: f(m, 48), push_res: f(m, 52),
    width: f(w, 116), radius: f(w, 120), wheel_fric: f(w, 128),
    x: f(w, 212), y: f(w, 216), z: f(w, 220),
    fx: f(w, 224), fy: f(w, 228), fz: f(w, 232),
    spin: f(w, 260), ground_speed: f(w, 264),
    throttle: f(w, 168), drive: f(w, 332),
    mud_depth: f(w, 272), water_depth: f(w, 276),
  };
  // wheel rigid body (+24) + 560: velocity (x/z follow the wheel, ~15 % low; y has a +0.4 offset at rest)
  try {
    const body = w.add(24).readPointer();
    s.vx = f(body, 560); s.vy = f(body, 564); s.vz = f(body, 568);
  } catch (e) { s.vx = s.vy = s.vz = 0; }
  buf.push(s);
}

// Truck body: body = a2 ? *(a2+16) : a3. Havok body at body+336: centre of mass +448 (= +336+112), linear
// velocity +560 (= +336+224). IDA: the point velocity there is lin(+560) + ang(+576) x (p - com(+448)).
function sampleBody(mud, a2, a3) {
  const body = a2.isNull() ? a3 : a2.add(16).readPointer();
  const now = Date.now();
  const key = body.toString();
  if (blast[key] && now - blast[key] < 1000 / MAX_HZ) return;
  blast[key] = now;
  bbuf.push({
    t: now, body: key, truck: a2.toString(), mud: mud.toString(),
    water: mud.add(60).readU32() === 105 ? 1 : 0,
    cx: f(body, 448), cy: f(body, 452), cz: f(body, 456),
    vx: f(body, 560), vy: f(body, 564), vz: f(body, 568),
  });
}

const hit = findFn(RVA_WHEEL, SIG_WHEEL);
if (hit.addr === null) {
  send({ type: 'error', msg: 'wheel function ' + hit.how });
} else {
  Interceptor.attach(hit.addr, {
    onEnter(args) { this.mud = args[0]; this.w = args[1]; },
    onLeave() {
      calls++;
      try { sample(this.mud, this.w); } catch (e) { }
    }
  });
  send({ type: 'ready', addr: hit.addr.toString(), how: hit.how });
}

const bhit = findFn(RVA_BODY, SIG_BODY);
if (bhit.addr === null) {
  send({ type: 'warn', msg: 'body function ' + bhit.how });
} else {
  Interceptor.attach(bhit.addr, {
    onEnter(args) { try { sampleBody(args[0], args[1], args[2]); } catch (e) { } }
  });
  send({ type: 'body_ready', addr: bhit.addr.toString(), how: bhit.how });
}

setInterval(() => {
  if (buf.length || bbuf.length) {
    send({ type: 'samples', calls: calls, data: buf, bodies: bbuf });
    buf = []; bbuf = [];
  }
}, FLUSH_MS);
