# Controls

Local (settlement) view:

| Input | Action |
|---|---|
| WASD / arrow keys | Pan the camera (speed scales with zoom distance) |
| Mouse wheel | Zoom toward the cursor; pitch tilts from 35 deg (close) to 75 deg (far) |
| Q / E | Rotate the view in 90 degree steps |
| Middle mouse drag | Rotate the view freely |
| F | Focus on the selected object (or the first worker) |
| PageUp / PageDown | Change the viewed level |
| V | Toggle slice view (only the viewed level and below) / show all |
| F3 | Toggle path debug |
| M | Switch between the local view and the planet map |
| Right-click | Context menu with actions for the cell or object |

Planet, theater and site map:

| Input | Action |
|---|---|
| Click the planet map / globe | Open the theater at that point |
| Drag the planet globe | Rotate it |
| Click a theater tile | Focus that tile |
| Double-click a theater tile | Open its site map |
| WASD / arrows | Site map: move the focus one tile at a time |
| Mouse wheel | Site map: zoom; planet/theater: zoom the globe |
| Enter | Found the settlement on the nearest dry bank inside the focused site tile (ocean/all-water tiles are rejected) |
| M | Return to the settlement |
| Right mouse button / Esc / Backspace | Go one level up: site → theater → planet |

The game starts on the planet: the left panel is the planet map, the globe
rotates, and there are no map-switch hotkeys. A click opens the 250 × 250
theater map (1024 m per tile); a double-click on a theater tile opens the site
map. The globe follows each step and zooms toward the selected point.

The camera tunables are in `src/presentation/input/CameraController.h` (`CameraControlConfig`),
the look in `src/presentation/rendering/LightingStyle.h` and `DepthShading.h`.
