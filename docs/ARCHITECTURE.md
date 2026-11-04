# Architecture

The game is one executable built from layers. A layer may include only layers below it;
`tools/check_dependencies.py` (CTest: `LayeringCheck`) fails the build otherwise.

```
app            main loop, screens, wiring                      (exe)
presentation   rendering, ui, input, assets - raylib           (exe)
gameplay       GameWorld, entities, items, work, commands      (lib, no raylib)
simulation     system scheduler on the fixed tick              (lib, no raylib)
world          grid, chunks, VoxelWorld, terrain sources       (lib, no raylib)
blocks         shapes of a cell, dense tables over materials   (lib)
geology        what the ground is made of: province columns    (lib)
materials      every material of the game; JSON catalog        (lib)
planet         planet generation pipeline                      (lib)
chemistry      elements, compositions, reactions               (lib)
foundation     time, simulation clock, load progress, coords   (lib)
```

Exact allowed edges are the `ALLOWED` table in `tools/check_dependencies.py`; the
libraries are declared with `infinitesia_add_module(...)` in `CMakeLists.txt`. Keep both
in step.

## Rules

- Dependencies go down only. Domains never reach sideways into each other's internals.
- Pure models (chemistry, planet, geology) and the block terrain know nothing about raylib.
- Terrain (`VoxelWorld`) and things standing on it (`EntityStore`) are separate; `GameWorld`
  bundles them for systems that need both. Take the narrowest half you can.
- Content is data in `assets/`, loaded into registries with stable string ids
  (`foundation/ContentRegistry.h`). Materials are one list, `assets/materials/materials.json`
  (`MaterialCatalog`): air, fluids, rocks, soils, minerals, built materials alike, one `MaterialDef`
  each; what only some have (a mineral's formula, a rock's modal composition) is a side table by
  the same id. A world cell is `BlockState{material, shape}`; every rule about it is a
  `BlockRegistry` query (docs/06_BLOCKS.md). Gameplay items and object
  kinds are in `assets/gameplay/{items,objects}.json` (`GameContent`). Code asks an object
  for its *abilities* (worker, pickable), never for a particular kind. Adding a
  kind with a known look is JSON only; a new look is a new `ObjectVisual` plus a draw routine.

## Adding a domain

1. `src/<domain>/` with a public header surface and `internal/` for the rest.
2. A line in `MODULE_OF` and `ALLOWED` (`tools/check_dependencies.py`) and an
   `infinitesia_add_module` in `CMakeLists.txt`.
3. A test executable in `tests/`, registered with `doctest_discover_tests`.
4. A section in `docs/`.

## Simulation runtime

`Simulation` (foundation) owns the one clock: a fixed timestep that counts ticks. `SimulationRuntime`
(`src/simulation/`) listens to it and, on every tick, runs the registered `SimulationSystem`s. It
adds no clock of its own: `GameCalendar` turns the tick number into game seconds, days and months
with integer arithmetic.

- `api/` is what a system sees: `SystemId`, `SystemPhase` (PreUpdate, Update, PostUpdate),
  `Cadence` (every tick, N ticks, game seconds, days, months), `SystemDescriptor`, `SystemContext`.
- `runtime/`: `SystemRegistry` (unique ids), `SystemScheduler` (phase, then `order`, then
  registration order; skips disabled and not-due systems), `SystemExecutor` (context, error
  policy `Disable`/`Continue`/`Rethrow`, tick and phase sequence checks).
- Register through `LocalGame::GetRuntime().Registry()`; `LocalGame` registers `WorkSystem` there.

## Planet generation

`PlanetMap` is built by a pipeline of stages that declare what they need and provide; new
domains add stages and layers instead of editing it. See `docs/05_PLANET_PIPELINE.md`.

## Planned (not done yet)

- Nothing at the moment. `WorkSystem` runs on the scheduler (`system.work`, Update phase, every
  tick); later systems register the same way.
