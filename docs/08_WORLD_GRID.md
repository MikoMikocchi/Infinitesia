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

Это первый шаг: мир пока не имеет собственной гидрологии и климатической
модели, он уточняет планетарные поля. Локальные реки, озёра и осадки появятся
отдельным этапом.

```cpp
PlanetMap planet(parameters);
WorldGenerator generator(planet);
WorldMap world = generator.Generate(GeographicCoordinates{45.0, 30.0});

const RegionTile &centre = world.At(0, 0);
const RegionTile &east = world.At(1, 0);
```

## 4. Что ещё не сделано

- `LocalGrid` всё ещё описывает регион 4 км; его нужно привести к
  `kRegionTileSizeMeters = 768` и согласовать `RegionData`.
- Планетарная сетка всё ещё 360 × 720 (55 км); её нужно уменьшить до 1° или
  2° и перестать использовать как игровую.
- `WorldMap` пока не используется игрой, историей и просмотрщиком.
- Нет локальной гидрологии на 257 × 257: реки и озёра — только планетарный
  прокси по `drainageAccumulation`.
- Нет сайтов, цивилизаций и истории на `WorldMap`.

## 5. Порядок внедрения

1. `WorldMap` и `WorldGenerator` (сделано).
2. История и сайты на `WorldMap`.
3. `LocalGrid` → 768 м, `RegionData` → 48 × 48 чанков.
4. Локальная гидрология мира.
5. Планетарная сетка → 1° или 2°.
