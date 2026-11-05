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

`RegionTile` — сводка на 32 байта, а не второй `PlanetRegion`:

```cpp
struct RegionTile
{
    float elevationMeters;
    float temperatureCelsius;
    float precipitationMm;
    float drySeasonMonths;
    float soilFertility;
    float riverDischarge; // 0..1
    Biome biome;
    std::uint8_t flags;   // ocean, lake, coast, river
    ProvinceId province;
};
```

`WorldMap` хранит `257 × 257` таких записей — **66 049** ячеек, около
**2,1 МиБ** — и `LocalGrid`, задающий положение окна на сфере. `RegionTileId`
это `{i, j}` от центрального тайла `(0, 0)`, диапазон `-128 .. 128`.

## 3. Генерация

`WorldGenerator` берёт готовый `PlanetMap` и `RegionTerrain`:

1. для каждого region tile считать точку на сфере через `LocalGrid`;
2. взять планетарные поля (`PlanetSurface`) и крупный рельеф;
3. добавить детали рельефа до длины волны `2 × 768 = 1536 м`;
4. пересчитать температуру по локальной высоте (lapse rate `6.5 K/км`);
5. классифицировать биом, записать провинцию и нормализованный сток;
6. выставить флаги `Ocean`, `Lake`, `River`, а после прохода — `Coast`.

Затем `WorldHydrology` (`world/WorldHydrology.{h,cpp}`) считает локальную воду
на 257 × 257:

1. priority-flood заполняет замкнутые впадины до уровня перелива;
2. каждая суша направляет сток к самому низкому соседу (8 направлений);
3. водосбор накапливается вниз по течению с весом по осадкам;
4. тайлы с эффективным водосбором ≥ `kRiverAreaKm2` становятся реками;
5. впадины глубже `kLakeDepthMeters` становятся озёрами.

Планетарный `drainageAccumulation` больше не используется как прокси: это
граничное условие, а реки и озёра теперь свои у мира.

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
- `WorldMap` пока не используется игрой и просмотрщиком.
- `RegionGenerator` пока берёт климат и провинции из 55-километровой планеты,
  а не из `WorldMap`; это следующий шаг связывания.

## 5. Порядок внедрения

1. `WorldMap` и `WorldGenerator` (сделано).
2. `LocalGrid` → 768 м, `RegionData` → 48 × 48 чанков (сделано).
3. Локальная гидрология мира: реки и озёра на 257 × 257 (сделано).
4. `RegionGenerator` читает климат/провинции/высоту из `WorldMap`.
5. Планетарная сетка → 1° или 2°.
6. Показ `WorldMap` в просмотрщике и утилите снимка.
