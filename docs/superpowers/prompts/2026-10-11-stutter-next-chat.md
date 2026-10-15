Driver stutter hunt, next step. Read .remember/remember.md and the end of
docs/superpowers/research/2026-10-11-driver-stutter-deep-dive.md first (sections from "DS-client runs 4-10" down to
"Replay-delay test 12:22-12:25"). Also the "Driver stutter" section of docs/superpowers/research/mud-gotchas.md.

Where we are (all measured unless marked):
- The driver-only MP stutter is the client applying server corrections. PROVEN: client diag "C: Disable Correction" gives
  0 snaps / 100 s on dirt vs 31 / 120 s without it. Front-wheel stick-slip on dirt is real but does not jolt the car.
- Each snap: body + visual jump 0.21-0.35 m almost purely ALONG the path (6-10 physics steps of travel), both directions.
- IDA read (not run): the server checks its CURRENT transform against the client's reported result for the oldest queued move.
  If the queue is empty it steps with null inputs ("NVMC NO MOVE TO CONSUME"). On failure it sends its current transform.
  Replay delay (server a1+352 = diag value + 2) gates the checks. SetAllowance values are not reset; the 11:23 run really had a
  fixed 1 m limit and still had normal snap rates.
- Inference (unproven): the server's step count drifts from the client's move ids (queue dry / rebuffering), so a correction
  puts the client k steps off along its path.
- 12:25 replay-delay test was inconclusive: Workbench launched the dedicated server again, the DS has no diag menu, and
  0 NVMC lines appeared in any log.

Goal of this chat: prove or kill the queue-starvation theory.
1. Help me set up Workbench AS THE HOST (Peer Tool with the dedicated server turned off, like the 11:29 test) with the
   PeerPlugin1 client driving the robot on the same dirt spot (~1045-1068 / 2579-2599, GM_Arland).
2. On the WORKBENCH Diag menu (Vehicles > Vehicle Networking and Reconciliation): "Debug path correction" + "SimState to CSV".
   I run 2x with "S: Replay delay" default, then 2x with it at 8.
3. You read the logs yourself (newest ArmaReforgerWorkbench/logs, PeerPlugin1/logs, server/logs, and the
   MUD_framelog_client_runN CSVs in PeerPlugin1/profile). Count snaps per run, count "NO MOVE TO CONSUME" and
   "SERVER CORRECTING" lines, and which allowance failed. Check that the NVMC lines reach console.log at all before trusting a 0.
4. Write the whole per-run table into the deep-dive doc, with a checksum.
5. If starvation is proven: look for a script-side fix (only SetAllowance / SetPrediction / EnableInterpolation are scriptable;
   replay delay is not).

After that (already in the handoff): rut cap decision, then remove the TEMP code (MUD_FrameLog.c + its call in
MUD_JoltProbe.Tick, MUD_StutterRobot.c, MUD_JoltProbe.c + EOnFrame call + ProbeGround, m_fColChangeAt, TEMP_SKIN_ONLY switch,
MUD_AllowanceTest unless shipped, m_fRutSpeed 3 -> 1).

Rules: measure before claiming, label guesses, keep MUD_AllowanceTest/robot/probe/framelog as TEMP. IDA HTTP fallback:
POST JSON-RPC to 127.0.0.1:13337/mcp (helper scripts in %TEMP%/fl: dc.py decompile, xa.py xrefs).
Robot: driver seat, stopped, hold Space 3 s -> 20 s reverse circle; logs 'MUD robot START/STOP'.
