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

The camera tunables are in `src/presentation/input/CameraController.h` (`CameraControlConfig`),
the look in `src/presentation/rendering/LightingStyle.h` and `DepthShading.h`.
