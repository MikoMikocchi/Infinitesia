# Переход на SDL3 + OpenGL 3.3

Статус: этап 1 выполнен (каркас SDL3 + smoke-окно). Остальные этапы запланированы.

## 0. Прогресс

- [x] Этап 0. Базовая линия: план и структура зафиксированы.
- [x] Этап 1. Каркас SDL3: SDL3 3.4.20 через FetchContent, OpenGL 3.3 Core,
  загрузчик функций, smoke-окно.
- [ ] Этап 2. Math, Input, Clock.
- [ ] Этап 3. GPU-объекты.
- [ ] Этап 4. 2D и текст.
- [ ] Этап 5. 3D-мир.
- [ ] Этап 6. Ввод, камера, picking.
- [ ] Этап 7. Жизненный цикл приложения.
- [ ] Этап 8. Удаление raylib.
- [ ] Этап 9. Валидация.

Текущая проверка этапа 1:

```
InfinitesiaSdl3Smoke.exe --frames 3

OpenGL: 3.3.0 NVIDIA 610.88
Renderer: NVIDIA GeForce RTX 4060 Ti/PCIe/SSE2
SDL3 smoke finished cleanly after 3 frame(s).
```

Старый raylib-таргет `Infinitesia` продолжает собираться в этой же ветке.

## 1. Цель

Убрать raylib из исполняемого файла и заменить его на:

- **SDL3** — окно, события, ввод, таймеры, файловые пути, скриншоты;
- **OpenGL 3.3 Core** — вся отрисовка, как и сейчас;
- **stb_image / stb_image_write** — загрузка PNG и сохранение скриншотов;
- **stb_truetype** — текст вместо `DrawText`/`MeasureText` raylib;
- **свой небольшой gfx-слой** — текстуры, шейдеры, меши, материалы, 2D- и 3D-примитивы.

Игровая логика (`core`, `content`, `domains`, `world`, `simulation`, `gameplay`) не меняется:
raylib там не используется.

Не цель этого этапа: переход на SDL_GPU, Vulkan, D3D12 или Metal. Это отдельный,
гораздо более крупный проект.

## 2. Что есть сейчас

- raylib 5.5 подключается через `FetchContent` в `cmake/Dependencies.cmake`.
- Исполняемый файл линкуется с `raylib` в `CMakeLists.txt`.
- 40 файлов включают `raylib.h`, `rlgl.h` или `raymath.h`, всего около 4.9 тыс. строк.
- raylib изолирован в `src/app` и `src/presentation`; доменные библиотеки от него не зависят.

| Каталог | Файлов | Строк |
|---|---:|---:|
| `src/app` | 5 | 717 |
| `src/presentation/assets` | 1 | 27 |
| `src/presentation/input` | 5 | 416 |
| `src/presentation/rendering` | 14 | 1847 |
| `src/presentation/ui` | 15 | 1883 |

На что именно опирается текущий код:

- `Mesh`, `Material`, `Shader`, `DrawMesh`, `UploadMesh`, `UpdateMeshBuffer`;
- прямой `rlgl`: VAO/VBO, `rlBegin`/`rlEnd`, clip planes, depth mask;
- `Texture2D`, `Image`, `LoadImage`, `GenImageColor`, `LoadTextureFromImage`;
- `Camera3D`, `GetMouseRay`, `GetRayCollisionSphere`, `GetRayCollisionBox`;
- `DrawText`, `MeasureText`, `DrawRectangle*`, `DrawLine*`, `DrawLine3D`, `DrawCube`,
  `DrawSphere`, `DrawPlane`, `DrawBoundingBox`;
- `raymath`: векторы, матрицы, кватернионы;
- `InitWindow`, `BeginDrawing`, `EndDrawing`, `IsKeyPressed`, `GetMousePosition`,
  `GetScreenWidth`, `GetFrameTime`, `TakeScreenshot`, `GetApplicationDirectory`.

Ключевые шейдеры: `assets/shaders/world_lighting.vs`, `world_lighting.fs`,
`blob_shadow.fs`. Они уже написаны под GLSL 330 и могут быть сохранены почти без
изменений.

## 3. Целевая структура

```
src/presentation/
  platform/
    Window.h/.cpp      SDL3: окно, GL-контекст, события, resize, DPI, base path,
                       скриншот, завершение
    Input.h/.cpp       клавиатура, мышь, колесо, фокус, pressed/down/released
    Clock.h/.cpp       delta time, общее время, FPS
  math/
    Types.h            Vector2/3/4, Matrix, Quaternion, Color, Rectangle, Ray,
                       Camera3D — совместимые по полям с raylib
    Math.h/.cpp        нужное подмножество raymath
  gfx/
    Gl.h/.cpp          загрузка функций OpenGL 3.3 Core
    Image.h/.cpp       CPU-изображение RGBA8, stb_image
    Texture.h/.cpp     GPU-текстура
    Shader.h/.cpp      компиляция, линковка, uniform-ы
    Mesh.h/.cpp        VAO/VBO/EBO, upload/update/draw
    Material.h/.cpp    shader + texture + tint
    Renderer.h/.cpp    кадр, viewport, scissor, depth, blend, cull, clear
    Painter2D.h/.cpp   батчинг 2D-примитивов
    Painter3D.h/.cpp   линии, кубы, сферы, плоскости, bounding box
    Text.h/.cpp        stb_truetype, атлас глифов, DrawText/MeasureText
```

`src/app` переходит на `platform`, `gfx` и `math` вместо raylib. Доменные модули
не трогаем.

## 4. Ключевые решения

### 4.1. Математика

Чтобы не переписывать сотни обращений к `Vector3`, `Matrix` и `Quaternion`,
делаем совместимый слой:

- те же имена типов и полей (`Vector3{x,y,z}`, `Matrix{m0..m15}`, `Color{r,g,b,a}`);
- те же функции, которые уже используются: `Vector3Subtract`, `MatrixMultiply`,
  `QuaternionToMatrix`, `MatrixInvert`, `Fade`, `Clamp` и т.д.;
- те же цветовые константы (`WHITE`, `BLACK`, `RAYWHITE`, `GRAY`, `SKYBLUE` и др.);
- та же раскладка матриц (column-major, как в raymath), чтобы не менять шейдеры
  и `ViewFrustum`.

Это не «свой движок математики», а тонкая замена `raymath.h`. Если позже захочется
GLM, это можно сделать отдельным рефакторингом, но не в этой миграции.

### 4.2. Шейдеры

Оставляем текущий ABI, чтобы не переписывать шейдеры и `SetShaderValue`:

- атрибуты: `vertexPosition`, `vertexTexCoord`, `vertexNormal`, `vertexColor`;
- uniform-ы: `mvp`, `matModel`, `matNormal`, `texture0`, `colDiffuse`;
- плюс текущие uniform-ы мира: `lightDirection`, `sunColor`, `ambientSky`,
  `ambientGround`, `fogColor`, `fogParams`, `cameraPosition`, `depthShadeA`,
  `depthShadeB`.

`gfx::Shader` при линковке привязывает атрибуты к фиксированным location
(`glBindAttribLocation`), а `gfx::Mesh::Draw` сам считает и выставляет
`mvp`, `matModel`, `matNormal`. Для 2D и текста будет отдельный простой
шейдер с теми же именами атрибутов.

### 4.3. Текст

Используем `stb_truetype` и один TTF-шрифт в `assets/fonts/` (например,
DejaVu Sans или Inter). `gfx::Text`:

- кэширует атлас глифов по размеру шрифта;
- поддерживает UTF-8;
- предоставляет `DrawText(text, x, y, size, color)` и `MeasureText(text, size)`;
- рисует через `Painter2D` с белой текстурой атласа.

Важно: текущий raylib-шрифт — пиксельный bitmap. TTF даст немного другую
метрику и сглаживание, поэтому текстовые экраны не будут пиксель-в-пиксель
совпадать со старыми скриншотами. Это ожидаемое изменение; проверяем, чтобы
текст не вылезал за панели.

### 4.4. Изображения и текстуры

- `stb_image` читает PNG/JPG/BMP/TGA/PSD/GIF/HDR — практически тот же набор,
  что и raylib.
- `gfx::Image` хранит RGBA8. `BlockTextureAtlas` собирает атлас на CPU и
  загружает одной текстурой, как сейчас.
- `TextureAssetManager`, `UniqueTexture`, `RegionMapView`, `WorldMapView`,
  `PlanetViewer` переводятся на `gfx::Texture`.
- Загрузка GPU-ресурсов остаётся на главном потоке: генерация изображений может
  идти в worker-потоке, но `glTexImage2D`/`glTexSubImage2D` — только в `Update`/`Draw`.

### 4.5. Окно и ввод

`platform::Window`:

- `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD)`;
- OpenGL 3.3 Core, double buffer, depth 24, stencil 8, vsync;
- `SDL_GL_CreateContext` / `SDL_GL_SwapWindow`;
- `SDL_GetWindowSizeInPixels` для viewport, `SDL_GetWindowSize` для UI —
  нужно явно решить политику HiDPI;
- `SDL_GetBasePath` вместо `GetApplicationDirectory`;
- скриншот: `glReadPixels` + `stbi_write_png` с переворотом по Y.

`platform::Input` хранит состояние на кадр:

- `IsKeyDown`, `IsKeyPressed`, `IsKeyReleased`, `IsKeyPressedRepeat`;
- `IsMouseButtonDown/Pressed/Released`, `GetMousePosition`, `GetMouseDelta`,
  `GetMouseWheelMove`;
- `IsWindowFocused`.

Внутри — `SDL_Scancode`; снаружи — enum с именами используемых клавиш
(`KEY_W`, `KEY_PAGE_UP`, `KEY_ESCAPE`, ...), чтобы код читался как раньше.

### 4.6. OpenGL loader

Сделан собственный небольшой загрузчик в `src/presentation/gfx/Gl.cpp`:
нужные функции GL 3.3 Core резолвятся через `SDL_GL_GetProcAddress`, а
GL 1.1 (`glClear`, `glViewport`, `glBindTexture`, `glDrawElements`, ...)
линкуется из системной библиотеки. Это убирает зависимость от Python/glad;
список загружаемых функций можно расширять по мере порта.

Изначальный план предполагал вендорить glad, но для текущего набора вызовов
собственный загрузчик проще и прозрачнее. Если позже понадобится много
extension-функций, glad можно добавить отдельным шагом.

### 4.7. Зависимости

Добавить в `cmake/Dependencies.cmake`:

- SDL3, тег `release-3.4.20` (или актуальный стабильный), через `FetchContent`
  с `FIND_PACKAGE_ARGS`;
- `stb` (header-only, можно просто положить `stb_image.h`,
  `stb_image_write.h`, `stb_truetype.h` в `src/third_party/stb/`);
- glad (вендор).

raylib остаётся в зависимостях до этапа 8, чтобы можно было сверять поведение
и собирать старую версию из `main`.

## 5. Этапы

Работаем в отдельном worktree/ветке. `main` с raylib-версией остаётся эталоном
для скриншотов и сравнения.

### Этап 0. Базовая линия

- Собрать текущую версию, записать FPS и несколько скриншотов через
  `INFINITESIA_DEV_SCREENSHOT`.
- Зафиксировать сцены: загрузка, планета, театр, сайт, контекстное меню,
  профиль персонажа.
- Создать worktree `sdl3`.

**Готово, когда:** есть набор эталонных PNG и известны базовые тайминги.

### Этап 1. Каркас SDL3 — выполнен

- SDL3 3.4.20 добавлен через `FetchContent` с минимальным набором подсистем.
- Добавлены `platform::Window` (`src/presentation/platform`) и `gfx::Gl`
  (`src/presentation/gfx`).
- Сделан отдельный таргет `InfinitesiaSdl3Smoke` (`src/sdl3/SmokeMain.cpp`):
  окно, GL 3.3 Core контекст, очистка экрана, выход по Esc или после N кадров.
- Проверены resize, vsync, завершение и вывод GL-версии/renderer string.
- Старый `Infinitesia` (raylib) продолжает собираться.

**Готово, когда:** smoke-окно стабильно работает на Windows (и, по возможности,
Linux/macOS), GL-версия и renderer string корректны. — выполнено на Windows.

### Этап 2. Math, Input, Clock

- Перенести типы и функции `raymath`, которые используются.
- Реализовать `platform::Input` и `platform::Clock`.
- Добавить `TestPattern`: движущийся квадрат по WASD, колесо, скриншот.

**Готово, когда:** ввод и время работают без raylib, скриншот сохраняется.

### Этап 3. GPU-объекты

- `gfx::Image`, `gfx::Texture`, `gfx::Shader`, `gfx::Mesh`, `gfx::Material`.
- Порт `BlockTextureAtlas`, `TextureAssetManager`, `UniqueTexture`.
- Нарисовать текстурированный куб текущим `world_lighting`-шейдером.

**Готово, когда:** атлас блоков и текстуры предметов загружаются, куб
освещается, ошибок GL нет.

### Этап 4. 2D и текст

- `gfx::Painter2D`: прямоугольники, линии, рамки, скруглённые рамки,
  треугольники, текстуры.
- `gfx::Text` на stb_truetype.
- Порт `DebugOverlay`, `WorldContextMenu`, `CharacterProfilePanel`,
  `Application::DrawLoadingScreen`, HUD в `ApplicationRender`, 2D-части
  `PlanetViewerDraw`, `RegionMapView`, `WorldMapView`.

**Готово, когда:** все 2D-экраны рисуются, текст читается, панели не разъезжаются.

### Этап 5. 3D-мир

- `gfx::Camera`, `gfx::Renderer` с view/projection, clip planes, depth/blend/cull.
- `gfx::Painter3D`: `DrawLine3D`, `DrawPlane`, `DrawCube`, `DrawSphere`,
  `DrawBoundingBox`, квад для blob shadow.
- Порт `WorldRenderer`, `WorldTerrainRenderer`, `WorldObjectRenderer`,
  `WorldOverlayRenderer`, `FluidMeshBuilder`, `TerrainMeshBuilder`,
  `ViewFrustum`, `PlanetViewerGlobe`, `PlanetGlobeGeometry`.

**Готово, когда:** terrain, вода, тени, подсветка объектов и глобус выглядят
как в эталоне (с поправкой на шрифт).

### Этап 6. Ввод, камера, picking

- Порт `CameraController`, `GridPointer`, `WorldInteractionController`,
  `WorldObjectPicker`, `PlanetViewerInput`.
- Реализовать `GetMouseRay` через обратную view-projection.
- Реализовать `CheckCollisionPointRec`, `GetRayCollisionSphere`,
  `GetRayCollisionBox` (или заменить их на собственные ray-AABB/ray-sphere).

**Готово, когда:** камера, picking, контекстное меню и клики по глобусу работают.

### Этап 7. Жизненный цикл приложения

- `ApplicationRun`: `platform::Window` + `platform::Clock`, event loop,
  `SDL_Quit`.
- `ApplicationUpdate`/`ApplicationRender`: `BeginFrame`/`EndFrame`, размеры
  экрана, `TextFormat` → `snprintf`, `TakeScreenshot` → `platform::SaveScreenshot`.
- `GetApplicationDirectory` → `platform::BasePath`.
- Проверить dev-hooks и `loading_profile.txt`.

**Готово, когда:** полный игровой цикл работает без raylib.

### Этап 8. Удаление raylib

- Убрать `raylib` из `cmake/Dependencies.cmake` и `CMakeLists.txt`.
- Удалить include-ы `raylib.h`, `rlgl.h`, `raymath.h`.
- Проверить `grep` по проекту: raylib не осталось.
- Обновить `docs/ARCHITECTURE.md` и упоминания стека.

**Готово, когда:** конфигурация и сборка проходят без raylib.

### Этап 9. Валидация

- Сравнить скриншоты «до/после» по сценам.
- Пройти ручной чек-лист: планета, театр, сайт, камера, вода, тени, меню,
  профиль, загрузка, скриншот.
- Прогнать тесты (они доменные и не должны зависеть от рендера).
- Проверить Debug/Release, Windows x64; по возможности Linux/macOS.
- Замерить frame time и время загрузки текстур/атласа.

**Готово, когда:** нет известных визуальных регрессий, тесты зелёные,
производительность не хуже базовой.

## 6. Карта замен

| raylib | Новый слой |
|---|---|
| `InitWindow` / `CloseWindow` | `platform::Window` |
| `WindowShouldClose` | событие `Window::ShouldClose` |
| `BeginDrawing` / `EndDrawing` | `gfx::Renderer::BeginFrame` / `EndFrame` |
| `ClearBackground` | `gfx::Renderer::Clear` |
| `Camera3D` | `math::Camera3D` |
| `Mesh`, `UploadMesh`, `UpdateMeshBuffer` | `gfx::Mesh` |
| `Material` | `gfx::Material` |
| `Shader`, `LoadShader`, `SetShaderValue` | `gfx::Shader` |
| `Texture2D`, `LoadTexture` | `gfx::Texture` |
| `Image`, `LoadImage`, `GenImageColor` | `gfx::Image` + stb_image |
| `DrawText`, `MeasureText` | `gfx::Text` |
| `DrawRectangle*`, `DrawLine*`, `DrawTriangle` | `gfx::Painter2D` |
| `DrawLine3D`, `DrawCube`, `DrawSphere`, `DrawPlane`, `DrawBoundingBox` | `gfx::Painter3D` |
| `rlBegin`, `rlVertex3f`, `rlEnd` | `gfx::Painter3D` / immediate mesh |
| `rlSetClipPlanes`, `rlDisableDepthMask` | `gfx::Renderer` state |
| `GetMouseRay`, `GetRayCollision*`, `CheckCollisionPointRec`, `RectangleContains` | `math::Ray` / `math::Rectangle` helpers |
| `IsKey*` (включая `IsKeyPressedRepeat`), `IsMouse*`, `GetMouse*` | `platform::Input` |
| `GetScreenWidth/Height` | `platform::Window` |
| `GetFrameTime`, `GetTime`, `GetFPS` | `platform::Clock` |
| `TakeScreenshot` | `platform::SaveScreenshot` |
| `GetApplicationDirectory` | `platform::BasePath` |
| `TextFormat` | `std::snprintf` |

## 7. Риски

1. **Матрицы и шейдеры.** Несовпадение layout/порядка умножения даст неверную
   проекцию или освещение. Смягчение: сохранить raymath-совместимый `Matrix`,
   проверить куб и frustum на этапе 3.
2. **Текст.** Другая метрика шрифта → сдвиги в UI. Смягчение: TTF с явными
   размерами, проверка панелей, запас по ширине.
3. **Глубина и clip planes.** Текущий код специально ставит near/far и
   depth mask. Смягчение: повторить state в `gfx::Renderer`, проверить
   coplanar-тени и линии сетки.
4. **HiDPI.** `GetScreenWidth` в raylib и SDL3 могут означать разное.
   Смягчение: явно разделить logical size и pixel size, viewport — в пикселях,
   UI — в logical.
5. **Многопоточная генерация.** Изображения считаются в worker-потоке, а
   текстуры грузятся на главном. Смягчение: не вызывать GL из worker-потоков,
   как и сейчас.
6. **Производительность мешей.** Сейчас `UpdateMeshBuffer` переиспользует
   буферы. Смягчение: сохранить динамический update и capacity в `gfx::Mesh`.
7. **Сборка SDL3.** На Windows статическая сборка SDL3 добавляет время
   конфигурации/сборки. Смягчение: `FIND_PACKAGE_ARGS` + `FetchContent` fallback.

## 8. Оценка

Для одного разработчика, без учёта неожиданных регрессий:

| Этап | Оценка |
|---|---:|
| 0. Базовая линия | 0.5 дня |
| 1. Каркас SDL3 | 2–3 дня |
| 2. Math/Input/Clock | 2–3 дня |
| 3. GPU-объекты | 4–6 дней |
| 4. 2D и текст | 5–7 дней |
| 5. 3D-мир | 8–12 дней |
| 6. Ввод/камера/picking | 4–6 дней |
| 7. Жизненный цикл | 2–3 дня |
| 8. Удаление raylib | 1–2 дня |
| 9. Валидация | 3–5 дней |
| **Итого** | **~31–47 рабочих дней** |

Это ориентир на паритет по функциям и visuals. Если делать только «чтобы
запускалось», можно уложиться быстрее, но тогда останутся регрессии в тенях,
тексте и UI.

## 9. Definition of Done

- В проекте нет `raylib.h`, `rlgl.h`, `raymath.h` и линковки с raylib.
- Все экраны и dev-hooks работают.
- Скриншоты ключевых сцен совпадают с эталоном (кроме ожидаемого текста).
- `ctest` проходит.
- Сборка Debug/Release на Windows; по возможности Linux/macOS.
- Нет известных утечек GL-ресурсов и ошибок `glGetError`.
