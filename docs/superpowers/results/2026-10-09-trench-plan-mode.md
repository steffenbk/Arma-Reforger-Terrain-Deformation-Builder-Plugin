# Trench plan mode - results

Plan: docs/superpowers/plans/2026-10-09-mud-trench-plan-mode.md. Game: Experimental 192624, Workbench play mode.
All numbers recounted 2026-10-10 from the raw console logs below (grep), not copied from the ledger.
Log A = logs/logs_2026-10-09_19-46-01/console.log, Log C = logs/logs_2026-10-10_00-59-17/console.log.

## Stage 1 - Tasks 1+2 (layout, CarveTrench, TEMP trench)

| When | Log line | Result |
|---|---|---|
| 23:48:13 | A 7163-7166 | self-test FAIL 2 of 46 (stretch closed end) -> fixed |
| 23:48:14 | A 7200 | TEMP check: inside 271, at depth 271, WRONG 0, outside dug 0 |
| 23:51:56 | A 7547 | self-test PASS (46 checks) |
| 23:51:57 | A 7580 | TEMP check: inside 420, at depth 420, WRONG 0, outside dug 0 (checksum 420 = 420) |

## Stage 2 - Task 3 (sites, ghosts, Dig trench)

| When | Log line | Result |
|---|---|---|
| 23:58:41 | A 8064-8065 | sites 1 (3 pieces, 2 layers) and 2 (13 pieces) placed |
| 00:01:13 | A 8407 | site 1 check: inside 65, at depth 65, WRONG 0, outside dug 0; deleted |

## Task 3b - free dig, slopes, fill (Log A)

| When | Log lines | Result |
|---|---|---|
| 00:27-00:29 | A 9826-10080 | self-test PASS (51 checks); slope free digs layers 1-5 |
| 00:43-00:45 | A 10393-10604 | self-test PASS 51; 1x1 digs layers 1-6; whole-hole fill (rejected by user: traps player) |
| 00:48-00:52 | A 11110-11309 | self-test PASS 51; fill 1.5 -> 0 in 6 swings, then 'nothing to fill' |

Log A totals (grep -c): 'MUD trench test: PASS' 7 lines (46 x3 at 7547/8041/8788, 51 x4 at 9827/10393/11110/11629), 'MUD trench dig: site' 64,
'MUD free dig: at ... layer' 111, 'MUD fill: at' 10, 0xc0000374 0.

## Task 4 - plan mode + debug instant (Log C) - PASSED 01:09

| Site | Placed (line) | Pieces | Instant swings (line) | pieces x 6 layers | Check (line) |
|---|---|---|---|---|---|
| 1 | 1132 | 12 | 72 (1279) | 72 | inside 187, at depth 187, WRONG 0, outside dug 0 (1277) |
| 2 | 1306 | 11 | 66 (1441) | 66 | inside 185, at depth 185, WRONG 0, outside dug 0 (1439) |
| 3 | 1687 | 35 | 210 (2110) | 210 | inside 567, at depth 567, WRONG 0, outside dug 0 (2108) |

Checksum: swings 72 + 66 + 210 = 348 = (12 + 11 + 35) x 6 = 58 x 6.
Log C totals: self-test PASS 51 x2 (lines at 00:59:44, 01:04:43), 'MUD trench dig: site' 366
(18 before line 1132 by shovel + 348 from line 1132 on, all instant; 0 skipped), free dig 15, fill 6, 0xc0000374 0,
'Game destroyed' 6 (clean stops, last at 2137).
Hitch: site 3 placed 01:08:19.770 -> check 01:08:20.439 = 0.67 s for 35 pieces (DEBUG_INSTANT only).

## Not measured
- Smoothness of diagonal vs straight trenches: user satisfied by eye (01:09), no screenshot compare.
- Character model dipping into deformed ground while moving: open, out of scope.
