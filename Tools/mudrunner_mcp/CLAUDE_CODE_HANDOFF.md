# Prompt for Claude Code (paste this)

You are finishing a MudRunner telemetry MCP in `Tools/mudrunner_mcp` of this Arma Reforger mod project. Read
README.md, agent.js and server.py there first, and docs/superpowers/research/2026-10-14-mudrunner-ida.md +
2026-10-15-sinking-deep-dive.md for the background. IDA Pro MCP is connected with MudRunner.exe loaded
(G:\SteamLibrary\steamapps\common\MudRunner) - use it whenever an offset is in doubt.

Goal: Claude can attach to a running MudRunner, record every wheel's mud depth / slip / speed, and drive the
truck with scripted keys, so we can measure MudRunner's sinking and copy that feel into Reforger.

Steps (ask the user to start MudRunner and load a map with mud before step 3):
1. `py -m pip install frida mcp`. Check `py -c "import frida, mcp; print(frida.__version__)"`.
2. Test the agent WITHOUT MCP first: a tiny script that frida.attach("MudRunner.exe"), loads agent.js,
   prints the 'ready' message and 3 s of samples. Fix agent.js until 'ready' shows how=rva and samples arrive.
   (Frida 17 removed Module.findBaseAddress - agent uses Process.getModuleByName; adjust if the API differs.)
3. Sanity-check values with the user driving into mud: radius / width look like real tyres (0.4-0.8 m),
   depth rises from 0 when entering mud, ground_speed matches the speedo (km/h / 3.6), spin x radius ~=
   ground speed on hard ground. If a field is wrong, check it in IDA (sub_140930110) and fix the offset.
4. Check the body velocity at wheel+24 -> +560; drop it if it is garbage.
5. Check MudRunner's key binds (game settings) and that SendInput scan codes move the truck (drive tool).
   If not, try a virtual gamepad (vgamepad) instead and tell the user.
6. Register the server in the user's Claude config (README), restart, call attach / live / drive.
7. Optional next: hook sub_1409310D0 (truck body in mud, same (mud, truck) call shape, 48-byte signature
   '48 8B C4 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 88 FB FF FF 48 81 EC 40 05 00 00 48 C7 85 F0 02 00 00
   FE FF FF FF 48 89 58 18 0F 29 70 B8 0F') for chassis position / belly drag; group wheels per truck.
8. Write what you proved in Tools/mudrunner_mcp/README.md ("Not verified yet" -> move items to proven).

Rules: plain simple English in notes, label things PROVEN / INFERENCE, keep the user's files LF, no git here.
Single-player game, user's own copy - fine to hook. Don't write to game memory yet (a later 'poke' tool may).
