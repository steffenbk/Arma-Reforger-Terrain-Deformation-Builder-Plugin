# Mud lab D1 overlay + D2 pressed lift - archived 2026-10-20

User call after the stage 2 DS run (2026-10-20 22:43-22:50): sheet local ruts look better and run well, so the
other two surface modes leave the numpad 3 cycle. D2 "wasn't too bad, just slow to update".
Checkpoint v259 = everything before this archive.

## What moved here
- `MUD_MudOverlay.c` - the whole D1 overlay (MUD_MudOverlay, MUD_MudOverlayEntity, MUD_OvTile / OvGround / OvStamp /
  OvPass / OvVeh). Deleted from Scripts/Game/Mud.
- `MUD_MudLab_D1_D2_modes.c` - the two lab mode classes MUD_LabPressLift (D2) and MUD_LabOverlay (D1), cut from the end
  of MUD_MudLab.c.

## Call sites that were changed
- MUD_MudLab.c Init: `s_aModes.Insert(new MUD_LabPressLift());` and `s_aModes.Insert(new MUD_LabOverlay());` removed;
  header + Cycle comments now say off -> sheet local -> off.
- MUD_WorldGround.c EOnFrame: `MUD_MudOverlay.ENABLED ||` dropped from the MUD_MudLab.Tick condition.
- MUD_WorldGround.c SetPressLift: the block that turned the overlay off when pressed lift came on is removed.
- MUD_WorldGround.c ~550 comment: "(MUD_MudOverlay.Cycle)" -> "(was MUD_MudOverlay.Cycle; archived 2026-10-20)".

## What stays (dormant)
D2's engine code is NOT removed: MUD_WorldGroundEntity.PRESS_LIFT (default false), SetPressLift, PressLog,
s_fPressLiftM / s_fPressM / s_fWearPress, and its readers in MUD_CoarseSkin.c (~641, ~749-752, PressLiftRebuild).
It is spread through the core ground files; ripping it out is a separate risky edit. Nothing turns it on now.

## To bring one back
Copy the mode class back to the end of MUD_MudLab.c and re-add its `s_aModes.Insert` in Init (order = cycle order).
For D1 also copy MUD_MudOverlay.c back and restore the WorldGround lines above (or restore v259).
