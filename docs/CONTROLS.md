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
| Click the map / globe | Select a planet cell and re-centre the theater window |
| W | Toggle the 250 × 250 Theater Map and the planet surface map |
| Click a theater tile | Focus that site |
| R | Theater Map → site map of the focused tile; site map → Theater Map |
| WASD / arrows | Site map: move the focus one tile at a time |
| Mouse wheel | Site map: switch between 3 × 3, 5 × 5, 9 × 9 and 17 × 17 tiles |
| Enter | Found the settlement on the nearest dry bank inside the focused tile (ocean/all-water tiles are rejected) |
| M | Return to the settlement |
| Backspace | Theater Map → surface map; site map → Theater Map |

The Theater Map is the main selection screen: 250 × 250 tiles of 1024 m. The site
map is its zoomed-in view, and the globe shows where the theater window lies on
the planet.

The camera tunables are in `src/presentation/input/CameraController.h` (`CameraControlConfig`),
the look in `src/presentation/rendering/LightingStyle.h` and `DepthShading.h`.
