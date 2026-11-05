# Сетка мира: 257 × 257 region tiles

Документ фиксирует масштаб мира, к которому проект приводится вслед за Dwarf
Fortress. Блок остаётся **1 м**; меняется только иерархия.

## 1. Иерархия

Dwarf Fortress: регион-тайл состоит из `16 × 16` local area blocks, каждый
local area block — из `48 × 48` тайлов. В Infinitesia один тайл — один блок
1 м, поэтому:

| Уровень | Формула | Размер | В чанках 16 м |
|---|---|---:|---:|
| Тайл / блок | `kBlockSizeMeters` | 1 м | — |
| Local area block | `48 × 48` тайлов | 48 м | `3 × 3` |
| Region tile | `16 × 16` local blocks | **768 м** | **`48 × 48`** |
| World | `257 × 257` region tiles | **197,376 км** | — |

Площадь мира — **38 957 км²**. Это не вся планета: мир — локальное окно на
сфере, как карта мира в DF. Планета остаётся отдельным, более грубым слоем.

Константы живут в `src/domains/natural/planetary/world/WorldMap.h`:

```cpp
inline constexpr int kTilesPerLocalBlock = 48;
inline constexpr int kLocalBlocksPerRegionTile = 16;
inline constexpr int kRegionTilesPerWorldSide = 257;

inline constexpr double kBlockSizeMeters = 1.0;
inline constexpr double kChunkSizeMeters = 16.0 * kBlockSizeMeters;
inline constexpr double kLocalBlockSizeMeters = kTilesPerLocalBlock * kBlockSizeMeters;
inline constexpr double kRegionTileSizeMeters = kLocalBlocksPerRegionTile * kLocalBlockSizeMeters;
inline constexpr double kWorldSizeMeters = kRegionTilesPerWorldSide * kRegionTileSizeMeters;
```

Иерархия масштабно-инвариантна: если когда-нибудь понадобится точный
физический масштаб DF (481 км), достаточно поднять `kBlockSizeMeters` до
`2.44`; числа `16 × 16`, `48 × 48` и `257 × 257` не меняются.

## 2. RegionTile

`RegionTile` — компактная сводка на один region tile, а не второй
`PlanetRegion`:

```cpp
struct RiverChannel
{
    float fromX, fromY;   // вход, нормализованные координаты плитки 0..1
    float toX, toY;       // выход (вниз по течению)
    float widthMeters;    // полная ширина водного зеркала
    float depthMeters;    // врезка в рельеф
    float bendX, bendY;   // контрольная точка Безье; <0 = прямой отрезок
};

struct RegionTile
{
    float elevationMeters;
    float temperatureCelsius;
    float precipitationMm;
    float drySeasonMonths;
    float soilFertility;
    float riverDischarge;      // 0..1
    float waterLevelMeters;    // уровень воды над морем
    float waterDepthMeters;    // глубина у центра плитки
    std::uint32_t riverFirst;  // срез WorldMap::Rivers()
    std::uint16_t riverCount;  // число сегментов русла в плитке
    std::int8_t flowEast;      // направление потока, -1/0/1
    std::int8_t flowNorth;
    Biome biome;
    std::uint8_t flags;        // ocean, lake, coast, river
    ProvinceId province;
};
```

Для моря `waterLevelMeters = 0`, а `waterDepthMeters` — глубина океана. Для
озера уровень — это уровень перелива из priority-flood, а глубина — насколько
впадина заполнена. Для реки уровень — поверхность воды в русле, а глубина —
врезка русла. У суши уровень совпадает с высотой, глубина равна нулю.

Русло плитки — это не один отрезок, а набор сегментов в общем массиве
`WorldMap::Rivers()`; `riverFirst`/`riverCount` задают срез для плитки. Если в
плитку впадает несколько притоков, каждый получает сегмент до общей точки
слияния в центре плитки, а из слияния выходит один общий сегмент вниз по
течению. Ширина притока считается по его водосбору, ширина общего сегмента —
по суммарному. Точки пересечения общей границы у соседних плиток совпадают,
поэтому сеть не рвётся на границе region tile. `flowEast`/`flowNorth` хранят
направление стока вниз по течению.

Внутри плитки сегмент — квадратичная кривая Безье: `bendX/bendY` задают
контрольную точку. Амплитуда изгиба — до 18 % длины сегмента и не больше
0,12 плитки, направление берётся из детерминированного хэша координат плитки и
номера сегмента. Поэтому русло изгибается, но остаётся внутри своего region
tile и не рвётся на границах. `WorldTerrain::WaterAt` и врезка рельефа считают
расстояние уже до кривой, а не до прямой.

`WorldMap` хранит `257 × 257` таких записей — **66 049** ячеек, около
**4 МиБ** — и `LocalGrid`, задающий положение окна на сфере. В просмотрщике
**World Map** — основной экран выбора: 257 × 257 тайлов целиком, с контуром
окна мира на глобусе. `W` переключает World Map и планетарную карту; `R`
открывает регион как приближение выбранного тайла. `RegionTileId`
это `{i, j}` от центрального тайла `(0, 0)`, диапазон `-128 .. 128`.

## 3. Генерация

`WorldGenerator` берёт готовый `PlanetMap` и `RegionTerrain`:

1. для каждого region tile считать точку на сфере через `LocalGrid`;
2. взять планетарные поля (`PlanetSurface`) и крупный рельеф;
3. добавить детали рельефа до длины волны `2 × 768 = 1536 м`;
4. пересчитать температуру по локальной высоте (lapse rate `6.5 K/км`);
5. применить локальный климат (`world/WorldClimate.{h,cpp}`): крупные
   водоёмы смягчают температуру побережья, континентальность даёт холодные
   зимы внутри материка, а преобладающие ветры переносят океаническую влагу,
   поднимают её на наветренных склонах и оставляют дождевую тень за хребтом;
6. выставить флаги `Ocean` и `Lake`.

Вода и климат считаются в два прохода: сначала `WorldHydrology` даёт локальные
озёра и реки, затем `WorldClimate` видит эти водоёмы и меняет температуру и
осадки, после чего `WorldHydrology` пересчитывает воду уже по локальным осадкам.
`WorldHydrology` (`world/WorldHydrology.{h,cpp}`) считает локальную воду
на 257 × 257:

1. priority-flood заполняет замкнутые впадины до уровня перелива;
2. каждая суша направляет сток к самому низкому соседу (8 направлений);
3. водосбор накапливается вниз по течению с весом по осадкам;
4. тайлы с эффективным водосбором ≥ `kRiverAreaKm2` становятся реками;
5. впадины глубже `kLakeDepthMeters` становятся озёрами;
6. для каждого озера записывается уровень перелива и глубина, для каждой реки —
   направление стока, ширина/глубина русла и отрезок русла внутри плитки.

После гидрологии биомы классифицируются заново по локальным температуре,
осадкам и водосбору, а `MarkCoasts` выставляет флаги `Coast`.

Локальные потребители читают мир через `WorldTerrain`
(`region/WorldTerrain.{h,cpp}`): внутри окна он подменяет базовую высоту,
температуру, осадки, сухой сезон, плодородие, провинцию и воду на значения
`RegionTile`, а мелкий рельеф по-прежнему добавляет `RegionTerrain`. Вне окна
он возвращается к планетарным полям. Непрерывные поля между центрами region
tiles смешиваются bilinear/smoothstep (`WorldMap::SampleAt`), поэтому на границе
тайлов нет ступеньки высоты и полосы в затенении; биом, провинция, флаги и
гидрология берутся из ближайшего тайла.

`WorldTerrain::WaterAt` отдаёт уровень, глубину, направление потока и врезку
русла в точке. `SettlementTerrain` заполняет водой локальные впадины озёр до
уровня перелива и речные русла — вода появляется только там, где дно ниже её
поверхности. `WorldTerrain::ElevationMeters` вычитает врезку русла из рельефа,
поэтому река лежит в настоящей ложбине с берегами, а не поверх земли.
`PlanetTerrainSource::FluidAt` переносит это в `FluidColumn`, а `FluidField`
инициализирует клетки водой с `FluidOrigin::River`/`Lake`/`Sea` и направлением
потока. `RegionMapView` тоже запрашивает `WaterAt` для каждого пикселя карты,
поэтому реки и озёра видны на карте региона как вода, а не только как
врезанные ложбины.

```cpp
PlanetMap planet(parameters);
WorldGenerator generator(planet);
WorldMap world = generator.Generate(GeographicCoordinates{45.0, 30.0});

const RegionTile &centre = world.At(0, 0);
const RegionTile &east = world.At(1, 0);
```

## 4. Что ещё не сделано

- `LocalGrid` уже приведён к region tile 768 м; `RegionData` теперь 49 × 49
  точек по 16 м.
- Планетарная сетка всё ещё 360 × 720 (55 км); её нужно уменьшить до 1° или
  2° и перестать использовать как игровую.
- `WorldMap` уже используется `RegionGenerator` и `SettlementTerrain`:
  внутри окна высота, климат, почва и провинция берутся из region tile.
- Показ самой карты мира в просмотрщике ещё не сделан.

## 5. Порядок внедрения

1. `WorldMap` и `WorldGenerator` (сделано).
2. `LocalGrid` → 768 м, `RegionData` → 48 × 48 чанков (сделано).
3. Локальная гидрология мира: реки и озёра на 257 × 257 (сделано).
4. `RegionGenerator` читает климат/провинции/высоту из `WorldMap` (сделано).
5. Локальный климат и дождевые тени на 257 × 257 (сделано).
6. Планетарная сетка → 1° или 2°.
7. Показ `WorldMap` в просмотрщике и утилите снимка.
