# Plan 3 Task 3 - skin keeps its chunks; cells can be covered

Build: Experimental 192624. Log `logs/logs_2026-10-09_09-07-37/console.log`, play 11:57:01 (run 2; run 1 at 11:52:58 had
no test point, its build lines are identical except time). Test point m_vTestCover 1263 0 2203 (Task 1 hole point).

| Line | Plan 2 (results/2026-10-09-plan2-task3-skin.md) | Plan 3 Task 3 (11:57) | Equal? |
|---|---|---|---|
| built | 1697 + 2399 = 4096, hole cells 1340627, tris 2681254, via init | 1697 + 2399 = 4096, hole cells 1340627, tris 2681254, via init | yes |
| work | 5230 ms (heights 3594, meshes 423, colliders 1202) | 8708 ms (heights 5270, meshes 418, colliders 1927) | time only (same trace count; heights were 5.3 s in the Plan 2 00:05 run too) |
| misses / joined | 0 / 0 | 0 / 0 | yes |
| mem | +263778 KB | +278538 KB | +14760 KB = 14.4 MB (1697 records x 8.4 KB = 14.3 MB expected) |
| non-terrain / retraced | 0 / 0 | 0 / 0 | yes |
| check (build) | 2000, hit 2000, within 5 cm 2000, worst 0.000480652 m | same | yes |
| check (late) | 2000, hit 2000, within 5 cm 2000, worst 0.000480652 m | same | yes |

Cover test (cells 628 1100, 4 x 4 = 16 cells, 12 of them holes; 12 + 4 kept = 16):

```
MUD coarse cover test: cells 628 1100, covered 12, rebuild 1 ms
MUD coarse cover test: hole cells 12, skin hits while covered 0 (want 0), uncovered 12, rebuild 1 ms
MUD coarse cover test: skin hits after uncover 12 (want 12)
```

Per-patch skin swap cost (what Task 4 pays): cover rebuild 1 ms, uncover rebuild 1 ms (one 64 m chunk each).
