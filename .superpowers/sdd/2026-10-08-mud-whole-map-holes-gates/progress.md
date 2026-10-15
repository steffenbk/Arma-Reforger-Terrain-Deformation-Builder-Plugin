# SDD ledger — plan: docs/superpowers/plans/2026-10-08-mud-whole-map-holes-gates.md
Setup: Ruling: no git, no worktree, no commits — user CLAUDE.md forbids git for solo mod work — cost if wrong: no rollback points (tiles reproducible from pak).
Pre-flight: Task 2 consumes only Task 1 gate result (results file); no code interface shared.
Task 1: steps 1-4 done (13 passed); step 5 census written
Task 1: step 6 baseline 22:42 Exp 192624: box 0..4096 cells 2048x2048, done holes 0 runs 0 ms 701 truncated 0
Task 1: step 7 done 22:48: tiles 1/16/18 copied (35930/36004/36934 B), holes T1 (1,2) 8, T16 (2,0) 8, T18 (3,3) 9 = 25 cells
Task 1: step 9-10 22:49: override census holes 25 runs 9 ms 389 truncated 0; checker MATCH global (exit 0). No .meta created, no tile log lines. Step 12 pinned (14 passed).
Task 1: Ruling: step 11 physical check moved to a land hole (cells 640-643 z 1100-1103 = world x1280-1288 z2200-2208, written into Terrain_18) — seabed holes not visible under water — cost if wrong: none, adds 16 cells (census expect 41)
Task 1: step 11 22:58 user: fell through land hole x1280-1288 z2200-2208 (no WB restart needed after rewriting a tile: reopen world was enough)
Task 1: Ruling: land-hole census missing — Arlnd_Layers/default.layer re-saved by WB 22:55 without MUD_HoleCensusEntity (house moved to 1260.776 2209.114); user fall-through 22:58 accepted as proof the rewritten tile loaded — cost if wrong: none for gate (25-cell census already MATCH global). Unresolved why the class-only entity dropped on save.
Task 1: 23:01 land census holes 41 runs 13 ms 400 truncated 0; checker MATCH global, 41/41 (supersedes the 'census missing' ruling)
Task 1: step 13 pack 23:03 data.pak holds 3 tiles byte-identical; step 14 results + spec rows written. Task 1: complete (tests: python -m pytest Tools/ -> 14 passed) — cleanup of override tiles pending world close
Task 1: step 15 cleanup 23:09: override tiles deleted. Task 2: step 5 layer set (skin r200 64 m)
Task 2: Ruling: Start log line split in two — string.Format max 9 args (compile error line 156), plan's own fallback — cost if wrong: none
Task 2: step 4 compile clean 23:11; step 5 smoke 23:11: 47 chunks 227 ms wall worst 23 ms, heights 97 meshes 11 colliders 17, mem +6710 KB, diag A 0 B 18484 tie 29644, misses 0, frame 16.28 -> 14.46 ms
Task 2: step 6 full render+collider 23:13: 4096 chunks 4460544 verts 8388608 tris, 512 frames 22292 ms wall worst 29 ms, heights 8270 meshes 1128 colliders 1576, mem +586979 KB (+603133 after), frame 16.99 -> 17.88 ms, diag A 1861 B 2492933 tie 1699510, misses 4223 (~= 2*64*33 far-edge verts, inference)
Task 2: step 7 collider-only 23:14: 20325 ms wall worst 25, heights 8231 colliders 1957, mem +392969 KB, frame 15.91 -> 16.54. Step 8 results + spec row written. Step 9: layer left as full collider-only test block. Task 2: complete (tests: python -m pytest Tools/ -> 14 passed; in-engine logs in results/)
Final review: self-review (no subagent: user rules say spawn only on request). Code ran in engine: census 3 runs consistent, skin 3 runs, checksums hold. Final: minor (deferred): skin probe ~389 MB not returned between plays (unresolved). Final: minor (deferred): publish pack includes .remember/backups + test maps.
