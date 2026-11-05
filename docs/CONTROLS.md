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

Planet, world and region map:

| Input | Action |
|---|---|
| Click the map / globe | Select a planet cell and re-centre the world window |
| W | Toggle the 257 × 257 World Map and the planet surface map |
| Click a world tile | Focus that region |
| R | World Map → region map of the focused tile; region map → World Map |
| WASD / arrows | Region map: move the focus one tile at a time |
| Mouse wheel | Region map: switch between 3 × 3, 5 × 5, 9 × 9 and 17 × 17 tiles |
| Enter | Found the settlement in the focused region (dry land only) |
| M | Return to the settlement |
| Backspace | World Map → surface map; region map → World Map |

The World Map is the main selection screen: 257 × 257 tiles of 768 m. The region
map is its zoomed-in view, and the globe shows where the world window lies on the
planet.

The camera tunables are in `src/presentation/input/CameraController.h` (`CameraControlConfig`),
the look in `src/presentation/rendering/LightingStyle.h` and `DepthShading.h`.
