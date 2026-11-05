# MVP: отдельная жидкость (`FluidField`), генерация и простое течение

**Статус:** задача для следующего AI-агента
**Проект:** Infinitesia, C++20, raylib
**Цель:** убрать воду из блоков и сделать изменяемый слой жидкости, как в Dwarf Fortress:
клетка хранит грунт/блок, а жидкость — отдельно (тип + объём). Затем научить её простому
течению и минимально отрисовать.

---

## 0. Что прочитать перед началом

1. `docs/06_BLOCKS.md` — материи, теги, блоки, текущая модель.
2. `docs/02_REGIONS.md` — регион, сетка, `SettlementTerrain`, `PlanetTerrainSource`.
3. `src/world/Fluid.h` — уже добавленный `FluidColumn`.
4. `src/domains/natural/planetary/region/FluidOrigin.h` — источник жидкости.
5. `src/world/PlanetTerrainSource.*` — генерация колонок и `FluidAt`.
6. `src/world/VoxelWorld.*` — чанки блоков.
7. `src/world/TerrainSource.h` — интерфейс источника.
8. `tests/WorldTests.cpp` — текущие тесты мира.

---

## 1. Что уже сделано (не переделывать без причины)

Уже есть:

- `FluidOrigin { None, Sea, Lake, River, Groundwater, Subsurface, Volcanic }`.
- `FluidColumn`:
  ```cpp
  struct FluidColumn {
    MatterId matter = kAirMatter;
    FluidOrigin origin = FluidOrigin::None;
    double groundMeters = 0.0;
    double surfaceMeters = 0.0;
    double depthMeters = 0.0;
    bool HasFluid() const noexcept;
  };
  ```
- `TerrainSource::FluidAt(int x, int z)` с реализацией в `PlanetTerrainSource`.
- `SurfaceColumn` хранит `FluidOrigin fluid` и `double fluidLevelMeters`.
- `PlanetTerrainSource::Column` хранит `fluid`, `fluidLevelMeters`, `fluidTopBlocks`.
- `PlanetTerrainSource::FluidAt` возвращает жидкость в метрах.
- `water` и `seawater` объединены в одну материю `water`.
- `hasFluid` и костыль «вода не режется» удалены.
- 204 теста проходят в `cmake-build-release`.

**Важно:** сейчас `PlanetTerrainSource::StateOf` всё ещё кладёт воду блоками. Это надо убрать.

---

## 2. Границы задачи

### Входит в MVP

1. `FluidField` — изменяемое поле жидкости по клеткам.
2. Инициализация поля из `TerrainSource::FluidAt`.
3. Удаление воды из `PlanetTerrainSource::StateOf` (блоки — только грунт/растения/воздух).
4. Простое течение: вниз, затем в стороны; без давления.
5. Море — бесконечный источник, озеро — конечный объём.
6. Взаимодействие с копанием/постановкой блоков: убрал блок — вода затекает;
   поставил блок в воду — вода вытесняется/исчезает по простому правилу.
7. Минимальный рендер воды из `FluidField` (плоские верхние квады, прозрачность).
8. Тесты на всё перечисленное.

### Не входит

- трубы, насосы, машины, сети;
- двери, комнаты, зоны;
- несколько жидкостей в одной клетке и смешивание;
- давление, U-образные колена, «телепортация» воды;
- испарение, замерзание, дождь;
- пещеры/руды (пока генерация колонок);
- сохранение/загрузка;
- LOD и красивая вода;
- мультиблоки и block entities.

---

## 3. Архитектура MVP

### 3.1. Блок — только геометрия

`BlockState` остаётся:

```cpp
struct BlockState {
  MatterId matter;  // камень, земля, дерево, воздух...
  ShapeId shape;    // пока только полный куб
};
```

`StateAt`/`StateOf` **не возвращают воду**. Вода — не блок.

### 3.2. Жидкость — отдельное поле

Новый тип в `src/world/Fluid.h`:

```cpp
// 0..1000 mB; 1000 mB = 1 m3 = полная клетка 1x1x1.
struct FluidCell {
  MatterId matter = kAirMatter;
  std::uint16_t amountMb = 0;
  FluidOrigin origin = FluidOrigin::None;
};
```

- `matter` — вода, нефть, магма и т.д. (любая материя класса `fluid`).
- `amountMb` — объём в клетке; для MVP 0 или 1000, позже дробный.
- `origin` — откуда жидкость: `Sea`, `Lake`, `None` для перетёкшей.
- Одна клетка — один тип жидкости. Разные типы не смешиваются.

### 3.3. `FluidField` — чанковое хранилище

Новый класс `src/world/FluidField.h/.cpp`:

```cpp
class FluidField {
 public:
  const FluidCell* At(GridCell cell) const;
  FluidCell* At(GridCell cell);
  void Set(GridCell cell, FluidCell value);

  void InitializeChunk(const WorldChunk& chunk, const TerrainSource& terrain);
  void OnBlockChanged(GridCell cell, const VoxelWorld& world);
  void Step(const VoxelWorld& world);

  std::uint64_t Revision() const noexcept;

 private:
  // Только чанки, где есть жидкость.
  std::map<ChunkCoordinate, FluidChunk> m_chunks;
  std::set<GridCell> m_active;  // клетки, которым нужен тик
  std::uint64_t m_revision = 0;
};
```

`FluidChunk`:

```cpp
struct FluidChunk {
  ChunkCoordinate coordinate;
  int width = 0, height = 0, depth = 0;
  std::vector<FluidCell> cells;  // индексация как у WorldChunk::blocks
};
```

Не обязательно хранить `FluidChunk` для сухих чанков. Если жидкости нет, чанка в map нет.

`GridCell` сейчас не имеет `operator<`. Для `std::set<GridCell>` добавь в `src/world/Grid.h`:

```cpp
auto operator<=>(const GridCell&) const = default;
```

Либо храни активные клетки как упакованный ключ (`std::uint64_t`) и распаковывай его.

### 3.4. Кто чем владеет

- `VoxelWorld` владеет `FluidField`.
- `VoxelWorld::EnsureChunkGenerated` заполняет и блоки, и жидкость.
- `VoxelWorld` даёт доступ:
  ```cpp
  const FluidField& Fluids() const;
  FluidField& Fluids();
  const FluidCell* GetFluid(GridCell) const;
  ```
- `TerrainSource` остаётся неизменяемым источником начального состояния.

---

## 4. Пошаговый план

### Шаг 1. Расширить `FluidColumn` индексами блоков

`FluidColumn` в метрах неудобен для заполнения клеток. Добавить:

```cpp
struct FluidColumn {
  MatterId matter = kAirMatter;
  FluidOrigin origin = FluidOrigin::None;
  int groundBlock = -1;  // верхний твёрдый блок
  int topBlock = -1;     // верхний блок жидкости; -1 если сухо
  double groundMeters = 0.0;
  double surfaceMeters = 0.0;
  double depthMeters = 0.0;
  bool HasFluid() const noexcept;
};
```

`PlanetTerrainSource::FluidAt` заполняет `groundBlock = column.surface`,
`topBlock = column.fluidTopBlocks`.

Проверить, что для сухой колонки `topBlock == -1`, для моря/озера `topBlock > groundBlock`.

### Шаг 2. Добавить `FluidCell` и `FluidField`

Создать `src/world/FluidField.h/.cpp`.

Минимум:

- `InitializeChunk`: для каждой колонки чанка вызвать `terrain.FluidAt(x, z)`.
  Если жидкости нет — пропустить. Иначе заполнить клетки от `groundBlock + 1` до `topBlock`:
  - для всех, кроме верхней, `amountMb = 1000`;
  - верхняя: `amountMb = clamp((surfaceMeters - cellBottomMeters) * 1000, 0, 1000)`,
    где `cellBottomMeters = groundMeters + (y - (groundBlock + 1))`;
  - `matter = fluid.matter`, `origin = fluid.origin`.
- Если в чанке появилась хотя бы одна жидкость — создать `FluidChunk`.
- Добавить все неполные/граничные клетки в `m_active`. Полные клетки в глубине моря
  не надо тикать: они не меняются. Но их надо активировать, если у них появился
  пустой сосед (см. `OnBlockChanged`).

Простой вариант для MVP: активировать все клетки с `amountMb > 0` в только что
созданном чанке. Это дороже, но проще. Позже сузить до граничных. Главное — не
сканировать весь мир каждый тик.

### Шаг 3. Подключить `FluidField` к `VoxelWorld`

В `VoxelWorld`:

- добавить `FluidField m_fluids;`
- в `EnsureChunkGenerated` после заполнения блоков вызвать
  `m_fluids.InitializeChunk(chunk, *m_terrain);`
- добавить `GetFluid(GridCell)` и `StepFluids()`.

`StepFluids()` вызывать в фиксированном тике игры (`LocalGame::AdvanceFrame` /
`Simulation`), а не в каждом кадре. Иначе скорость воды будет зависеть от FPS.

В `VoxelWorld::SetBlock`:

- после изменения блока вызвать `m_fluids.OnBlockChanged(cell, *this);`
- при постановке твёрдого блока в клетку с жидкостью:
  - попытаться поднять жидкость в клетку выше, если там пусто;
  - иначе разлить в соседние клетки;
  - если места нет — удалить (для MVP это допустимо, задокументировать).
- при удалении блока — разбудить клетку и соседей, чтобы жидкость затекла.

### Шаг 4. Убрать воду из генерации блоков

В `PlanetTerrainSource::StateOf`:

```cpp
if (cell.y > column.surface) {
  return {plant != nullptr ? plant->matter : kAirMatter, kShapeFull};
}
```

Больше никакого `m_matter.water` в блоках.

`PlanetTerrainSource::Column` всё ещё хранит `fluidTopBlocks` — он нужен флоре
(`Ground::ColumnAt::growable`), это оставить.

`m_matter.water` остаётся: он используется в `FluidAt`, чтобы выбрать материю жидкости.

**Порядок миграции, чтобы не остаться без воды на экране:**

1. Добавить `FluidField` и тесты, вода пока ещё блоками.
2. Сделать рендер воды из `FluidField`.
3. Только после этого убрать воду из `StateOf`.
4. Удалить/переписать тесты, которые ищут воду в блоках.

Если рендер откладывается, шаг 3 всё равно обязателен для этой задачи; временно
можно оставить отладочный вывод глубины в `DrawDebugOverlay`, но не возвращать
воду в блоки.

### Шаг 5. Простое течение

`FluidField::Step(const VoxelWorld& world)`:

- обрабатывать только `m_active`, а не весь мир;
- за один шаг: скопировать `m_active` в локальный список, **очистить `m_active`**,
  обработать список и снова добавить туда только те клетки, которые изменились или
  соседствуют с изменившимися. Иначе полное море останется «активным» навсегда;
- порядок детерминированный: `std::set<GridCell>` или сортировка;
- не течь в клетки незагруженных чанков: `world.GetBlock(cell) == nullptr` означает
  «чанк не загружен» (или клетка вне мира) — такую клетку не трогать;
- для каждой клетки:
  1. **вниз**: если клетка под ней не занята твёрдым блоком и принимает жидкость
     того же типа (пустая или тот же `matter`), перелить туда максимум;
  2. **в стороны**: 4 соседа; выровнять уровни, переливая часть разницы;
  3. не переливать в твёрдый блок;
  4. не смешивать разные `matter`;
  5. `origin` у перетёкшей жидкости становится `None`;
  6. все затронутые клетки добавить в `m_active`.
- источник: если `origin == Sea` и `amountMb < 1000`, восстановить до 1000.
  Озеро (`Lake`) не восстанавливается: оно конечное.
- `m_revision` увеличивать при любом изменении, чтобы рендер знал, что меш устарел.

Для MVP достаточно 1–2 итераций за тик. Не делать BFS по всему морю.

Проверка «твёрдый ли блок»:

```cpp
const Block* block = world.GetBlock(cell);
if (block != nullptr && world.Registry().Occupied(block->state)) {
  // твёрдо, жидкость не входит
}
```

Незагруженный блок считать твёрдым или не трогать клетку: не должно быть утечек
в незагруженные чанки. В MVP лучше не течь в клетки, чей чанк не загружен.

### Шаг 6. Минимальный рендер воды

Не возвращать воду в блоки. Сделать отдельный меш по чанку:

- для каждой колонки найти верхнюю клетку с `amountMb > 0`;
- поверхность: `bottomY + amountMb / 1000 * cellSize`;
- добавить один верхний quad;
- материал: синий, alpha ~150, `MATERIAL_MAP_DIFFUSE`, world shader;
- рисовать после непрозрачной земли: `rlDisableDepthMask()` → вода → `rlEnableDepthMask()`;
- в `CutAtLevel` клампить `surfaceY = min(surfaceY, cutTop)`;
- если `groundTop >= surfaceY` — воды на срезе нет;
- кэш мешей по чанку и `FluidField::Revision()`, чтобы не строить каждый кадр;
- боковые грани на берегу — не обязательны для MVP, но если дёшево, добавить.

Если проще: отдельный `FluidMeshBuilder` и отдельный кэш в `WorldRenderer`, по аналогии
с `TerrainMeshBuilder`/`ChunkMeshCache`. Не смешивать воду и блоки в одном меше.

### Шаг 7. Обновить тесты

Удалить/переписать тесты, которые считают воду блоками:

- `ColumnsAreGroundWithSoilAndRockBelowAndAirAbove`
- `TheSeaFillsLowGroundOnlyBelowSeaLevel`
- `ALakeFillsWaterAboveSeaLevel`
- `TheFluidLayerMatchesTheBlocks`

Новые тесты (имена ориентировочные):

1. `TerrainBlocksContainNoFluid`
   - для моря, озера и суши: `StateAt` никогда не возвращает материю с `Tag::Fluid`.

2. `TheSeaIsStoredInTheFluidField`
   - у моря есть `FluidCell` с `matter == water`, `origin == Sea`, `amountMb > 0`.

3. `ALakeIsFiniteAndTheSeaIsInfinite`
   - вычерпать озеро → объём уменьшается;
   - вычерпать клетку моря → после `Step` она снова полная.

4. `WaterFillsADugHole`
   - взять колонку под водой, убрать блок под водой, вызвать `Step` N раз;
   - клетка заполнилась водой.

5. `WaterDoesNotPassThroughSolidBlocks`
   - выкопать яму, отрезанную камнем; вода не появляется с другой стороны.

6. `FluidFlowIsDeterministic`
   - два одинаковых мира после одинакового числа тиков дают одинаковые `FluidField`.

7. `FluidDoesNotMixDifferentMaterials`
   - вода не перетекает в клетку с нефтью/другой жидкостью (если добавишь тестовую материю).

8. Если сделан рендер: тест на построение меша воды (число квадов/вершин) или
   хотя бы smoke-тест, что для моря меш не пустой.

Старые тесты `StartingCells`, `HasSkyAccess`, `TreeFelling` должны продолжать проходить.
Вода больше не блок, поэтому `StartingCells` будет ставить рабочего на дно под водой;
это существующее поведение, не чинить в этой задаче.

---

## 5. Критерии готовности

- [ ] `PlanetTerrainSource::StateAt` не возвращает жидкости.
- [ ] `FluidField` инициализируется из `FluidAt` для загруженных чанков.
- [ ] Море бесконечное, озеро конечное.
- [ ] Вода течёт вниз и в стороны, не проходит сквозь твёрдые блоки.
- [ ] Копание под водой заполняет яму за несколько тиков.
- [ ] Постановка блока в воду вытесняет/удаляет воду.
- [ ] Вода видна в 3D хотя бы как полупрозрачная плоскость.
- [ ] `ctest --output-on-failure` — 100% passed.
- [ ] `python tools/check_dependencies.py` — `Layering OK`.
- [ ] Нет `hasFluid`-костыля и воды в `BlockState` генерации.

---

## 6. Файлы, которые вероятно придётся менять

Новые:

- `src/world/FluidField.h`
- `src/world/FluidField.cpp`
- `src/presentation/rendering/terrain/FluidMeshBuilder.h/.cpp` (если делаешь рендер)

Изменяемые:

- `src/world/Fluid.h` — `FluidCell`, при необходимости индексы в `FluidColumn`.
- `src/world/PlanetTerrainSource.h/.cpp` — `FluidAt`, `StateOf`.
- `src/world/VoxelWorld.h/.cpp`, `VoxelWorldChunks.cpp`, `VoxelWorldTerrain.cpp`.
- `src/world/Chunk.h` — возможно, ничего, если `FluidField` отдельный.
- `src/presentation/rendering/WorldRenderer.*`, `WorldTerrainRenderer.cpp`.
- `src/app/ApplicationUpdate.cpp` — вызов `StepFluids()` на тике.
- `tests/WorldTests.cpp`, при необходимости `tests/LocalGameTests.cpp`.
- `docs/06_BLOCKS.md` — обновить раздел про источники.

CMake трогать не нужно: `file(GLOB_RECURSE ...)` подхватит новые `.cpp`.

---

## 7. Подводные камни

1. **Два источника истины.** Если оставить воду в `StateOf` и одновременно в `FluidField`,
   они разойдутся. После задачи вода должна быть только в `FluidField`.
2. **Производительность.** Не сканировать весь мир и не тикать все полные клетки моря.
   Только `m_active` и только загруженные чанки.
3. **Незагруженные чанки.** Не переливать жидкость в клетки незагруженных чанков:
   это создаст «утечки в пустоту».
4. **Детерминизм.** `std::unordered_set` с указателями/порядком может дать разный
   результат. Использовать `std::set`/сортировку или устойчивый порядок обхода.
5. **Источники.** `origin` перетёкшей жидкости должен стать `None`, иначе озеро
   станет бесконечным, а море будет размножаться.
6. **Смешивание.** Одна клетка — один `matter`. Не переливать воду в нефть.
7. **Поверхность.** Верхняя клетка может быть заполнена не полностью; `amountMb`
   должен быть дробным (0..1000), а не только 0/1000, иначе вода будет «прыгать»
   на метр.
8. **Срез.** В `CutAtLevel` вода не должна висеть над срезом; клампить к `cutTop`.
9. **Старые тесты.** Тесты, которые ищут `water` в блоках, надо переписать, а не
   подгонять под старое поведение.
10. **Debug-сборка.** В `cmake-build-debug` сейчас не подтянут `pugixml`; для проверки
    использовать `cmake-build-release` или сначала починить конфигурацию debug.
11. **Циклический include.** `VoxelWorld` хранит `FluidField` по значению, а `FluidField::Step`
    принимает `const VoxelWorld&`. В `FluidField.h` вперёд объяви `class VoxelWorld;`,
    а `VoxelWorld.h` подключай только в `FluidField.cpp`.

---

## 8. Команды проверки

```bash
cmake --build cmake-build-release -j 8
cd cmake-build-release && ctest --output-on-failure
python ../tools/check_dependencies.py
```

Ожидаемо:

- сборка без ошибок;
- все тесты проходят;
- `Layering OK`.

---

## 9. Что делать после MVP

Не в этой задаче, но следующим этапам:

1. Трубы и `FluidNetwork`.
2. Насос: обмен `mB` между `FluidField` и `FluidTank`.
3. Машины как block entities.
4. Двери как блоки с состоянием.
5. Зоны/комнаты — ручная разметка игроком.
6. Несколько жидкостей, испарение/замерзание.
7. Пещеры и руды.

## 10. Обновление: давление как в Dwarf Fortress

После разбора потока DF давление добавлено: полная вода служит «трубой», по которой поток
телепортируется через полные клетки вниз, в стороны и вверх, но не выше первого полного блока
источника. Так вода поднимается в U-образном колене до уровня источника, но не переливается выше.
Поиск ограничен локальным радиусом (2048 клеток), включается при изменении блока, восстановлении
моря и внешней записи жидкости; спокойное море каждый тик не сканируется. Диагонали давление не
проводят, разные материи не смешиваются. Уровень воды в `FluidCell` остаётся дробным (0..1000 mB),
а давление переносит одну седьмую клетки за тик — как 1/7 в DF.

Заодно исправлен `TerrainSnapshot`: поверхность воды теперь всегда на одном уровне, а не
`ground + 60`; для глубокого океана используйте `--vexag`/`--scale`, чтобы картинка не была
слишком высокой.
