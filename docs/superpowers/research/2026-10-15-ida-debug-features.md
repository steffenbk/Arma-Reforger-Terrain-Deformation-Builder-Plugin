# Engine debug features for displaced ruts (Stage B0, 2026-10-15)

Exe: `ArmaReforgerSteamDiag.exe` (Arma Reforger Experimental, G:\SteamLibrary), IDA base 0x140000000.
Labels: IDA = read in the exe / shipped scripts, not tried in play. PROVEN = tried in play (says when).
Raw list of all 1729 engine diag registrations (address, name, menu, values) was dumped by an IDA script
(calls to the register function sub_1409F4F90); the relevant rows are below.

## How engine diag entries work (IDA)
- Engine entries are registered at startup by small functions calling `sub_1409F4F90(&id_var, shortcut, name,
  menu, values, -1, 0)`. Ids are handed out in startup order, so script cannot address them by name (also in
  2026-10-11-driver-stutter-deep-dive.md).
- Open the menu: Diag exe / Workbench play (shortcut not checked here - the one used on 2026-10-11). F1 with the menu open =
  diagmenu.save, keeps settings across restarts (PROVEN 2026-10-11, mud-gotchas).
- Engine diag draws only on the machine that runs it. A dedicated server shows nothing (no window); diag that
  logs (e.g. "Dump contacts") logs on that machine (IDA).

## 1. Engine diag entries that help here (IDA)
Menu path / entry / values / register call site:

Physics (parent of "Bullet")
| Entry | Values | Use for displaced ruts |
|---|---|---|
| Show bodies (lalt+num6) | disabled, flat-mesh, transp-mesh, flat+mesh, transp+mesh | see our brick colliders as drawn = holes / cracks in the BODY |
| Show collider type | all, sphere, box, capsule, cylinder, convex, trimesh | set to trimesh: only our bricks (+ other static trimeshes) |
| Color by | collider type, activation state, body type, by entity | "by entity" = each brick a colour: brick lines visible |
| Show body type | all, static, dynamic, kinematic | static = our bricks |
| Show trimesh mat | (item) | game material per trimesh (sub_140150790) |
| Show layer, Show COM, Show simulation state | | - |
| Show overlapping pairs, Show broadphase AABB | false, true | which bodies the wheels test against |
| Dump contacts | false, true | LOGS every non-static/static manifold: ">AppliedImpulse %f", body A / B, then "Total %d contacts" (sub_140B90270). Wheel-on-rut contacts in the log (heavy: debug only) |
| Show stats | disabled, basic, detailed, all | body / contact counts |

Physics > Bullet
| Entry | Values | Use |
|---|---|---|
| Draw Bullet wireframe | enabled, disabled | every collider triangle as lines: the 2x2 split is visible in the body |
| Draw Bullet contacts | enabled, disabled | contact points + normals: wheel contacts on rut walls / brick lines |
| Draw Bullet shape, Draw Bullet shape AABB | | per-shape draw / box |
| Max. collider distance | 20 .. 500 | draw range |
| Profile Bullet | disabled, >1ms .. >50ms | logs slow physics steps (new bigger trimeshes) |

Vehicles (engine, player vehicle)
| Entry | Use |
|---|---|
| Show contacts | wheel contact points on the rut |
| Show raycast / Test raycast | suspension rays (do they fall into a pulled seam?) |
| Show suspension, Show wheels, Show slope | suspension travel: jitter check of plan Step 4 |
| Player vehicle only (default true) | |
| Vehicle Networking and Reconciliation > Debug path correction, S: Input/Output Diags, Determinism testing > SimState to CSV | MP Step 5 (known, deep-dive 2026-10-11) |

Render
| Entry | Values | Use |
|---|---|---|
| Render debug mode (rctrl+ralt+w) | none, wire, wire only, ..., world norm, ..., vertex norms, ... global layer, ... | wire = the split mesh; vertex norms / world norm = dark faceting check (plan Step 4 "no dark faceting") |

Statistics
| Entry | Values | Use |
|---|---|---|
| Script profiler | off, frame, average, memory | per-function script time on screen: Displace / CarveFloor ms without our own maths |
| MeshObject stats | none, count, mesh, faces, verts | how many faces our script meshes add |
| FPS (lctrl+num1), Counters (lctrl+num3) | | fps over ruts |

World: Show entity bounds (disabled, enabled) - brick boxes, all entities.
Terrain menu: Blocks' / Tiles' bounding boxes - not ours.

Not found (IDA string search): any "internal edge" / btAdjustInternalEdgeContacts string or diag, any per-trimesh
"degenerate triangle" check on runtime meshes ("Degenerated triangles detected: %s" sits in a 4 KB function
sub_14149F690 with no other strings - likely asset import, unresolved). Bullet internal-edge handling cannot be
switched or seen from a menu.

## 2. Script debug API (IDA + shipped scripts)
- `DiagMenu` is bound in the GAME exe (sub_140E4AE70: RegisterMenu, RegisterItem, Unregister, RegisterBool,
  RegisterRange, GetBool, GetValue, SetValue, GetRangeValue, SetRangeValue, Set/GetEngineValue,
  Set/GetEngineRangeValue, SelectMenuByName, SelectMenuById). Binding runs only when the script module has flag
  0x10 (INFERENCE: the diag build).
- Script ids: id + an engine offset (dword_142602F70); id must be < 512 or "Too many Diags (maximum is 512)"
  (sub_140E4B0C0; PROVEN earlier, mud-gotchas). Vanilla uses `SCR_DebugMenuID` (scripts/Game/Utilities/
  DebugMenuID.c, ~300 auto-numbered values ending at DEBUGUI_DAMAGE_AREA_MARKERS). Ours must start after that
  and stay < 512.
- Vanilla pattern (SCR_DataCollectorDiagComponent.c, SCR_DestructionDiagComponent.c): every DiagMenu / DbgUI use
  is inside `#ifdef ENABLE_DIAG ... #endif`; register in an init (`RegisterMenu(id, "Data Collection",
  "Statistics")`, `RegisterBool(id, "", "Enable debug menu", "Data Collection")`), read with `DiagMenu.GetBool(id)`.
  So: ours too, or the retail game / DS fails to compile.
- `DbgUI` (bound at sub_140E49060): Begin / Text / Button / InputFloat / InputInt / Spacer / End windows (used by
  SCR_DestructionDiagComponent). Live numbers on screen (counters) without logging.
- `Shape` and `DebugTextWorldSpace` / `DebugTextScreenSpace` are bound in the game exe (strings present); we use
  them already (MUD_WorldGround.c). Whether they work in a retail (non-diag) build: unresolved (we only ran WB / Diag).
- `TraceParam` outputs (API): TraceEnt, TraceNorm (hit polygon normal), TraceDist, ColliderIndex, SurfaceProps,
  ColliderName. A down trace onto a brick returns the hit TRIANGLE's normal: a hole check can also flag a
  wall-facing or flipped normal under a rut (fold drop gone wrong).
- No script profiler-scope API found (no EnProfiler / ProfileScope string; "SetCategory" belongs to other
  classes). Timing from script stays System.GetTickCount; the engine's Statistics > Script profiler shows
  per-function time on screen.

## 3. Which engine feature replaces which Stage B script tool
| Stage B tool | Engine feature | Verdict |
|---|---|---|
| 1 DISP_LOG_EVERY_S summary | none logs our counters; Script profiler shows ms on screen only | KEEP (logs are the record; MP machines compared by log) |
| 2 DISP_DRAW_FLOOR | none (our data) | KEEP |
| 3 DISP_DRAW_BRICKS (split / overflow / fold) | Bullet wireframe + Render wire show which bricks are split (4x triangle density), "Color by entity" shows brick lines; nothing shows overflow / fold | KEEP, smaller: only overflow + fold bricks matter, split is visible in wire mode |
| 4 DISP_HOLE_CHECK | Show bodies (flat-mesh) + Bullet wireframe show holes by eye; nothing logs them | KEEP (log + TraceNorm check); look with the engine draw |
| 5 DISP_HASH | none | KEEP |
| displace ms | Statistics > Script profiler (Displace, CarveFloor rows) | use the profiler for the look; keep ms total / worst in the summary line |
| wheel feel on ruts (Step 4) | Vehicles > Show contacts / Show suspension / Show raycast, Bullet contacts | USE ENGINE, no script tool |
| faceting check (Step 4) | Render debug mode: vertex norms / world norm | USE ENGINE |

DiagMenu works from script in the game Diag exe and Workbench (IDA + vanilla pattern) -> Stage B puts the DISP_*
switches on a "Mud > Displaced ruts" diag menu, inside `#ifdef ENABLE_DIAG`, ids after SCR_DebugMenuID's last
value, < 512. Retail / DS builds: the switches read as off.

## Step 4 look setup (suggested, from the table)
1. Physics > Show collider type = trimesh, Show bodies = flat+mesh, Color by = by entity.
2. Render debug mode = wire (split), then vertex norms (faceting).
3. Vehicles > Show contacts + Show suspension while driving a rut along / across a 4 m line.
4. Statistics > Script profiler = average, look for Displace / CarveFloor.
