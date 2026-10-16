# Ruts and soft soil - literature research (2026-10-16)

User: "do a super mega research on this, find online, find papers" - after the "push spheres down with the wheel"
idea and the rut cells plan. Labels: READ (web search result text; this session's network policy blocked opening most
pages, so details come from abstracts / snippets), MEMORY (my knowledge of a paper, not re-checked this session),
CODE / MEASURED / IDA (our own files), INFERENCE. Nothing built.

## 1. Short answer
- **The sphere idea = DEM (discrete element method).** It is the most faithful soil model there is, and it is the
  slowest: SCM gives results that "match well" with DEM at "two to three orders of magnitude" less time (READ, Chrono
  arXiv 2407.18903); Chrono's GPU DEM "does not target realtime" (READ, Granular Gym). In our engine it also fails
  on the wheel ray (IDA item 5), on MP (dynamic bodies are not deterministic) and on looks (spheres are not a surface).
- **What every real-time system does instead: a grid of vertical "soil columns / nodes" pushed down by the wheel,
  with rules for where the pushed soil goes.** That is exactly our rut floor field. The literature adds three things
  we do not have yet, all deterministic and cheap:
  1. **Depth from load, not a fixed step** - Bekker pressure-sinkage (SCM, Sun/Di/Cui 2015, Chen 2013).
  2. **Soil memory over passes** - the first pass sinks most, later passes less (SCM plastic memory; Helexa et al.:
     "largest increase during the first few passes").
  3. **Bulldozing + angle of repose** - pushed soil goes to the rut edges as ridges, and walls steeper than the soil
     can hold slump back (Sumner 1998, SCM bulldozing, Gilardi 2016 drift-diffusion). This ALSO caps the wall slope -
     which is the IDA item 5 jitter risk (one centre ray, raw triangle normal). Realism and feel point the same way.
- **The hybrid trick for the "particles" feel:** keep the ground a heightfield and use particles only for what you
  see fly (Papadimitriou thesis; Shallow Sand Equations' particle tracking). For us: mud spray / chunks as the
  engine's existing wheel mud particle effects + vehicle mud (IDA item 12: WheelsMud effect, Set/GetWheelMud /
  BodyMud), triggered by the same rut ops.
- **Recommended: "SCM-lite" on our floor field** (section 4) - same op stream, same floor tiles, works with the rut
  field today and with rut cells later.

## 2. What the sources say
### Terramechanics (engineering, real-time)
- **SCM - Soil Contact Model** (DLR, Krenn & Hirzinger; Chrono's implementation by Tasora / Serban et al.):
  - soil = digital elevation grid, nodes move only vertically (READ, Springer road / soft-soil article; Chrono sync
    docs: "only nodes that have been deformed are kept in a hash map");
  - pressure-sinkage = Bekker, shear = Janosi-Hanamoto, generalised to any 3D contact; the Bekker width b comes from
    contact PATCHES found by flood-filling neighbouring hit nodes (patch perimeter / area) (READ, ASME JCND
    "Real-Time Simulation of Ground Vehicles on Deformable Terrain");
  - real time or near it on a commodity CPU (OpenMP) (READ); validated against DEM, 2-3 orders of magnitude faster
    (READ, arXiv 2407.18903);
  - MEMORY: each node keeps plastic history (sinkage only grows when the new pressure beats what the node already
    carried; an elastic part springs back), and optional bulldozing moves the displaced volume to the rut border
    with an erosion step limited by a slope angle.
- **Bekker pressure-sinkage:** p = (kc / b + kphi) * z^n -> z = (p / (kc / b + kphi))^(1/n); b = contact width,
  kc / kphi / n = soil constants (READ, several). Published values disagree and units are often missing (READ:
  n 0.13 .. 1.6 depending on soil and test) - use it as a SHAPE, tune numbers per soil in game.
- **Simplified Bekker for visual simulation:** Sun, Di, Cui 2015 ("A real-time vehicle-terramechanics model for
  visual simulation", Atlantis Press MEITA-15) and Xiao Chen's GSU dissertation 2013: track deformation from vehicle
  load, velocity, tyre size and soil type, in real time, aimed at visuals (READ). Closest published match to what we
  want.
- **Multi-pass ruts:** Helexa et al. (Forests, doi 10.3390/f17101133): silty loam, 3 wheel loads, repeated passes ->
  rut depth grows NONLINEARLY, biggest step on the first passes, energy highest on pass 1 (READ). Other studies see
  near-linear growth on wet clay under tractors (READ, CSIRO SR17093); a forwarder field study needed 20+ passes for
  > 10 cm (READ). Shape depends on soil and moisture -> a per-soil curve.
- **Real-time tracked vehicle on soft soil** (Meywerk et al. 2016): terrain height + soil properties in lookup tables
  that can be changed by passing wheels (READ) - the same "grid + soil table" design.
- **Madsen et al. SAE 2012-01-0767:** soil columns as discretised volumes, Boussinesq / Cerruti stress spreading
  (READ) - more physics, more cost.

### Graphics / games
- **Sumner, O'Brien, Hodgins 1998/99 "Animating Sand, Mud, and Snow"** (GI '98, CGF 18): heightfield ground deformed
  by rigid bodies; footprints and bicycle tracks; five largely independent parameters (READ). MEMORY of the method:
  columns under the object compress (part of the volume disappears) and the rest is pushed to the columns just outside
  the contact; then an erosion pass moves material from any column whose slope to a neighbour exceeds a "roughness"
  angle, a few iterations; "liquidity" shapes how far it spreads. -> our ridges + slope cap.
- **Gilardi, Watten, Newbury 2016 "Drift-Diffusion Based Real-Time Dynamic Terrain Deformation"** (EG short): a
  dynamic displacement map updated by a modified drift-diffusion equation + a soil description map, added to the
  heightmap; tracks vary with soil; friction and angle of repose modelled (READ).
- **Alvarado et al. 2022 "Real-Time Locomotion on Soft Grounds With Dynamic Footprints"** (Frontiers in VR): force
  per foot from weight / balance / speed / ground type, stress-strain with stiffness + Poisson ratio for compression
  AND lateral spread, applied to refined terrain patches (READ) - the same "refine only where touched" idea as rut
  cells.
- **Dvořák & Drahanský 2007 "Real-Time Terrain Deformations"** (IEEE ICIG): mass-spring + Lennard-Jones on a LOCAL
  sub-graph of a large terrain (READ) - the spring version of the sphere idea, local only.
- **Zhang, Zheng, Hu 2011/12 "Deformation models for dynamic terrain visualization"**: deformation models added to a
  multi-resolution terrain, tyre tracks as the example (READ).
- **Aquilio 2006 (GSU) "Real-time GPU-based simulation of dynamic terrain"**: dynamically-displaced height map on the
  GPU, ground vehicle on soft terrain (READ). GPU - not for us.
- **Spintires / MudRunner** (Zagrebelnyy breakdowns): 16 m blocks, only blocks near the player's truck are active;
  CPU mud physics and GPU mud rendering use DIFFERENT data; vertex-texture-fetch heightmaps (READ). Our split is the
  same idea with one mesh for both (look = feel).
- **GPU snow / mud trails** (Batman: Arkham Origins GDC 2014 displacement heightmaps + relief mapping / DX11
  tessellation; AC3; Rise of the Tomb Raider "deferred deformation"; Horizon Frozen Wilds long-lasting trails) (READ)
  - all shader-side, not possible for mods; useful only for look references.

### Particles / granular
- **DEM:** best physics (localized shear, dilatancy), needs huge particle counts for a sensible particle-to-tyre size
  ratio, tiny time steps (READ, ISTVS 2019 preprint; arXiv 2307.03445 dual-GPU DEM).
- **Hybrid heightfield + particles:** Papadimitriou (Bournemouth MSc): heightfield for the invisible bulk, particles
  for the visible surface, cheap enough for real time (READ). Shallow Sand Equations (TVCG 2019): height-field
  granular flow in real time on the GPU + particle tracking for visuals (READ).
- **Unified Particle Physics** (Macklin et al. 2014, PBD, sand with friction) - MEMORY, not re-checked; GPU, not for
  us.

## 3. Why this fits our constraints (CODE / IDA)
| Constraint | DEM / springs (spheres) | Grid + rules (SCM-lite) |
|---|---|---|
| Wheel = one centre ray, raw triangle normal (IDA 5) | ray lands on a sphere; no push | floor field -> mesh; slope cap helps |
| MP determinism | dynamic bodies differ per machine | deterministic maths per op (disp hash matched 517 / 645) |
| Cost | 16 bodies / m2 at 0.25 m | 1 float (+1 for ridge) per 0.25 m sample, only touched tiles |
| Look | needs a mesh over the spheres anyway | the mesh we build (rut field / rut cells) |
| Whole map | no | sparse tiles (rut field), small rebuilds (rut cells) |

## 4. Proposal - "SCM-lite" on the rut floor (INFERENCE, design only)
All inside the rut op that already exists (server decides the numbers, every machine applies the same maths):
1. **Load-based target depth.** Server: per wheel, p = wheel load / contact area (load = vehicle mass / wheel count,
   or the suspension's real load if the API gives it; area = tyre width x a contact length). Soil from the terrain
   game material under the wheel (option G table): per soil a SHAPE - z_first = first-pass sinkage at a reference
   load, exponent from Bekker n, cap. Put the result into the op's existing step (m_fDepth) / cap (m_fB) fields - no
   new net data. Slip (WheelGetLongitudinalSlip) raises it (spinning digs).
2. **Soil memory.** Per floor sample keep the pressure it has carried (or simply: the new floor only goes deeper when
   the new target is deeper than the current floor - which CarveFloor's min() already does). Add the decelerating
   curve: each pass cuts a share of the REMAINING depth to the cap (e.g. 40 % of (cap - current)), so pass 1 is the
   biggest step, later passes smaller, as Helexa measured. Deterministic.
3. **Bulldozing ridges.** A share of the removed volume (soil-dependent, sand high, clay low) goes to a ridge value
   just outside the rut edge (2nd field value, max blend; the old heightfield berm 0.9 m / 0.08 m and the raised-ruts
   idea A). Ridges count for ground cost and feel only where they are drawn (rut field / rut cells).
4. **Angle-of-repose relaxation.** After the cut, 1-2 passes over the touched floor samples: where the slope between
   two neighbours exceeds the soil's angle (sand ~30-35 deg, clay steeper; tune), move height from the high one to the
   low one (Sumner's erosion). Caps rut walls (feel: IDA item 5) and rounds rims (look). Local to the op box,
   deterministic (fixed order).
5. **Particles for the look only.** Rut ops on soft ground fire the engine's wheel mud effects and raise the vehicle's
   mud (SetWheelMud / SetBodyMud) - client-side, no geometry.

What it needs from us: a soil table (gamemat -> z_first, exponent, cap, bulldoze share, repose angle); the ridge
value in MUD_RutTile; a relaxation pass in CarveFloor. Works on the rut field now and the rut cells later.

## 5. Next steps (user decides)
1. Offline first: Tools/scm_lite_proto.py on the displace-proto shapes - fixed step vs load-based + memory + ridges +
   repose; measure wall angles (target <= ~45 deg where wheels run), ridge height, pass-by-pass depth curve.
2. Read 3 full texts locally if possible (this session could not open them): Sun/Di/Cui 2015 (equations),
   Chrono SCM docs (bulldozing + plastic memory parameters), Sumner 1998 (erosion step) - for exact formulas.
3. Then in game behind a switch inside CarveFloor; MP: disp hash lines must still match.

## Sources (READ via search results unless marked)
- Chrono SCM / real-time ground vehicles on deformable terrain: https://www.researchgate.net/publication/359619160_Real-time_simulation_of_ground_vehicles_on_deformable_terrain ;
  https://asmedigitalcollection.asme.org/computationalnonlinear/article/18/8/081007/1156640/Real-Time-Simulation-of-Ground-Vehicles-on
- SCM vs DEM timing: https://arxiv.org/html/2407.18903v1 ; Chrono terrain sync: https://api.projectchrono.org/terrain_synchronization.html
- Krenn (DLR) SCM workshop: https://ewh.ieee.org/conf/icra/2010/workshops/PlanetaryRovers/03-Krenn-Locomotion/Krenn-Locomotion.pdf
- Road / soft-soil models (SCM grid): https://link.springer.com/article/10.1007/s11044-024-09980-6
- Compaction-based terrain model: https://www.researchgate.net/publication/286821241_Compaction-Based_Deformable_Terrain_Model_as_an_Interface_for_Real-Time_Vehicle_Dynamics_Simulations
- Tracked vehicle on deformable soils (lookup tables): https://journals.sagepub.com/doi/10.1177/1687814016647889
- Sun, Di, Cui 2015: https://atlantis-press.com/proceedings/meita-15/25838612
- Xiao Chen 2013 dissertation: https://scholarworks.gsu.edu/cs_diss/79
- Madsen et al. SAE 2012: https://saemobilus.sae.org/articles/a-physics-based-vehicle-terrain-interaction-model-soft-soil-off-road-vehicle-simulations-2012-01-0767
- Bekker parameters: https://journals.ekb.eg/article_201333.html ; https://mjae.journals.ekb.eg/article_102153.html
- Helexa et al. multi-pass ruts: https://www.citedrive.com/en/discovery/rut-formation-soil-strength-changes-and-energy-demand-under-repeated-pneumatic-wheel-passes-in-a-soil-channel/
- Repeated wheeling stress: https://publish.csiro.au/sr/SR17093 ; forwarder multipassing: https://www.bib.irb.hr/1233074
- Sumner et al.: https://arxiv.org/pdf/2302.08683 ; https://diglib.eg.org/items/a18f4810-e22f-4f61-971f-e33e29d70ee0
- Gilardi et al. 2016: https://diglib.eg.org/xmlui/handle/10.2312/egsh20161011
- Alvarado et al. 2022: https://dx.doi.org/10.3389/frvir.2022.801856
- Dvořák & Drahanský 2007: https://www.fit.vut.cz/research/result/c28812/
- Zhang et al. deformation models: https://www.springerprofessional.de/en/deformation-models-for-dynamic-terrain-visualization/3794328
- Aquilio 2006: https://www.springerprofessional.de/en/real-time-gpu-based-simulation-of-dynamic-terrain/1274856
- Spintires / MudRunner: https://80.lv/articles/breakdown-mud-and-water-of-spintires/ ;
  https://gamedev.net/tutorials/programming/general-and-gameplay-programming/rendering-and-simulation-in-an-off-road-driving-game-r3216
- Batman AO snow: https://gdcvault.com/play/1020177/Deformable-Snow-Rendering-in-Batman
- DEM: https://arxiv.org/pdf/2307.03445 ; https://arxiv.org/pdf/2306.01369 ; https://WWW.HPC.MsState.Edu/publications/docs/2019/07/16194ISTVS2019-preprint.pdf
- Hybrid sand: https://nccastaff.bournemouth.ac.uk/jmacey/MastersProject/MSc12/Papadimitriou/Master_Thesis_Report.pdf ;
  https://doi.org/10.1109/tvcg.2019.2944172
