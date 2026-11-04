# INFINITESIA
# EXTREME PLANETARY SIMULATION — MASTER DOCUMENTATION BLUEPRINT

> **Status: archived.** Moved to `docs/archive/` as long-term vision. It does not describe the current code and largely overlaps with "MASTER TECHNICAL DESIGN DOCUMENT". For the current picture see `docs/Systems.md`, `docs/DataDriven.md` and the numbered documents in `docs/`.

**Document type:** Master Documentation Architecture & Technical Specification  
**Project:** Infinitesia  
**Engine:** Existing custom C++ application based on C++20/C++23 and raylib  
**Primary paradigm:** Deterministic, data-driven, multi-scale planetary and industrial simulation  
**World representation:** Spherical planetary model, global 2D representation, local 3D environments  
**Document status:** Long-term architectural vision and implementation blueprint  
**Intended audience:** Developers, technical designers, simulation programmers, future AI coding agents

---

# PART I — THE GRAND VISION

## 1. What Infinitesia Is Supposed to Become

Infinitesia is intended to be an industrial, geological, ecological, economic, social, and political simulation operating on a procedurally generated planet.

The player does not merely place buildings on a map.

The player interacts with a world whose properties emerge from interconnected natural and artificial processes.

A mountain exists because of geological history.

A mineral deposit exists because of a chain of geological and geochemical events.

A river exists because of topography, precipitation, erosion, groundwater, and drainage.

A forest exists because of climate, soil, water availability, ecological succession, and biological interactions.

A mine exists because someone discovered a deposit, established infrastructure, organized workers, supplied machinery, and created an industrial operation.

A factory transforms materials according to physical, chemical, thermal, and mechanical processes.

A settlement develops according to employment, logistics, housing, resources, infrastructure, politics, public health, and social conditions.

An economy emerges from production, consumption, trade, scarcity, labor, investment, and institutions.

The central principle:

**The world should not be a collection of randomly assigned properties. It should be the result of a hierarchy of interacting processes.**

The simulation should be capable of answering questions such as:

- Why does this mountain exist?
- Why is this particular mineral present here?
- Why is the deposit shaped like a vein rather than a massive body?
- Why does this river flow through this valley?
- Why is this soil suitable for agriculture?
- Why does this factory produce a certain amount of waste?
- Why is a particular region economically important?
- Why does a settlement grow, stagnate, or decline?
- Why does a political movement become influential?
- Why does a particular industrial process become economically viable?

The answer should ideally be traceable through the simulation's underlying data and process history.

## 2. Three Levels of Ambition

The project must distinguish three different things.

### 2.1 Ultimate Vision

The complete theoretical scope of the simulation.

This includes planetary formation, tectonics, geology, chemistry, mineralogy, climate, hydrology, biology, materials science, industrial chemistry, metallurgy, economics, population dynamics, politics, and social systems.

The ultimate vision is allowed to be absurdly ambitious.

### 2.2 Scientific Model

The subset of the vision represented by explicit mathematical, physical, chemical, biological, or economic models.

Scientific models must document:

- Assumptions.
- Equations.
- Units.
- Input parameters.
- Output variables.
- Numerical methods.
- Approximation methods.
- Validity ranges.
- Known limitations.
- Validation references.

A model does not become scientifically accurate merely because its variables have scientific names.

### 2.3 Gameplay Model

The computationally affordable approximation actually used by the game.

A gameplay model may simplify reality considerably, provided that:

- Its behavior is understandable.
- Its assumptions are explicit.
- Its outputs are internally consistent.
- It preserves the intended gameplay relationships.
- It can be upgraded without rewriting the entire architecture.

Every system must state which level it belongs to.

---

# PART II — GLOBAL ARCHITECTURAL RULES

## 3. Fundamental Design Principles

Every subsystem must follow these principles.

### 3.1 Determinism

Given identical initial conditions, simulation configuration, seed, and event sequence, the system should produce identical results within its declared determinism guarantees.

Document:

- Random number generation.
- Seed derivation.
- Deterministic iteration.
- Event ordering.
- Floating-point limitations.
- Save/load consistency.
- Parallel execution restrictions.

### 3.2 Data-Driven Design

Where appropriate, materials, elements, minerals, reactions, machines, buildings, geological processes, and environmental parameters should be defined through external data.

Do not hardcode every mineral, machine, or chemical reaction into separate C++ classes.

The implementation should support data definitions with explicit schemas, validation, identifiers, and versioning.

### 3.3 Hierarchical Simulation

The planet cannot be simulated at maximum detail everywhere.

Use several levels:

1. Planetary background.
2. Continental and regional simulation.
3. Local geological simulation.
4. Active 3D world simulation.
5. Industrial process simulation.

Systems must specify what information is preserved when moving between levels.

### 3.4 Conservation and Accounting

Where relevant, processes must track:

- Mass.
- Energy.
- Water.
- Chemical species.
- Material quantities.
- Economic value.
- Labor.
- Waste.
- Information.

A process must not create or destroy quantities without an explicitly documented source, sink, or approximation.

### 3.5 Provenance

Generated world features should retain enough information to explain their origins.

Examples:

- A deposit records its geological formation process.
- A rock layer records its depositional or intrusive origin.
- A river records its drainage relationships.
- A manufactured alloy records its input materials and production route.

### 3.6 Uncertainty

The simulation must distinguish:

- What physically exists.
- What the simulation knows.
- What the player has discovered.
- What is estimated.
- What remains uncertain.

### 3.7 No Mandatory Maximum Complexity

A detailed scientific model must never be required merely to run the entire planet.

Every subsystem must offer an affordable approximation or a way to reduce its simulation frequency and resolution.

---

# PART III — DOCUMENTATION STRUCTURE

The following is the proposed documentation repository.

```text
docs/
│
├── 00_MASTER_VISION.md
│
├── 01_PLANETARY_FORMATION.md
├── 02_SPHERICAL_WORLD_MODEL.md
├── 03_TECTONIC_SYSTEMS.md
├── 04_GEOGRAPHY_AND_TERRAIN.md
├── 05_CLIMATE_SYSTEM.md
├── 06_HYDROLOGY_SYSTEM.md
├── 07_GEOLOGICAL_HISTORY.md
├── 08_PETROLOGY_AND_ROCKS.md
├── 09_MINERALOGY.md
├── 10_CHEMISTRY_PLATFORM.md
├── 11_GEOCHEMISTRY.md
├── 12_ORE_DEPOSIT_GENERATION.md
├── 13_PETROLEUM_SYSTEM.md
├── 14_GEOPHYSICS.md
├── 15_OCEANOLOGY.md
├── 16_SOILS_AND_ECOLOGY.md
├── 17_PALEONTOLOGY_AND_EVOLUTION.md
├── 18_MATERIALS_SCIENCE.md
│
├── 19_CHEMICAL_INDUSTRY.md
├── 20_MINING_AND_METALLURGY.md
│
├── 21_WORLD_GENERATION_PIPELINE.md
├── 22_CHUNK_AND_LOD_ARCHITECTURE.md
├── 23_SURVEY_AND_EXPLORATION.md
├── 24_UNIVERSAL_PROCESS_FRAMEWORK.md
├── 25_DATA_SCHEMAS_AND_CONTENT_FORMAT.md
├── 26_VALIDATION_AND_TESTING.md
├── 27_DEBUG_AND_VISUALIZATION_TOOLS.md
└── 28_IMPLEMENTATION_ROADMAP.md
```

The documents are not independent essays. Together they form a technical specification.

Every document must include:

1. Purpose.
2. Scope.
3. Terminology.
4. Scientific background.
5. Simulation objectives.
6. Mathematical and computational model.
7. Data structures.
8. Inputs and outputs.
9. Processes and state transitions.
10. Interactions with other systems.
11. Generation and simulation requirements.
12. Performance considerations.
13. Validation and testing.
14. Debug visualization.
15. Simplifications and known limitations.
16. Implementation phases.
17. Open questions.
18. References and further reading.

---

# PART IV — THE 29 DOCUMENTS

# 00_MASTER_VISION.md

## Purpose

Defines what Infinitesia is, what it attempts to simulate, and how the entire project is conceptually organized.

This is the highest-level design authority.

## Required contents

### 1. Project Identity

Describe:

- Genre.
- Core gameplay.
- Player role.
- Simulation philosophy.
- World scale.
- Intended experience.
- Non-goals.

### 2. World Concept

Explain the relationship between:

- Planet.
- Continents.
- Regions.
- Local environments.
- Underground environments.
- Settlements.
- Industrial infrastructure.

### 3. Simulation Philosophy

Describe:

- Emergent systems.
- Causal generation.
- Deterministic simulation.
- Data-driven architecture.
- Multi-scale modeling.
- Scientific approximations.
- Player interaction.

### 4. Simulation Domains

List all major domains:

- Planetary formation.
- Tectonics.
- Geography.
- Climate.
- Hydrology.
- Geology.
- Mineralogy.
- Chemistry.
- Geochemistry.
- Geophysics.
- Oceanology.
- Ecology.
- Evolution.
- Materials science.
- Mining.
- Industrial chemistry.
- Metallurgy.
- Logistics.
- Population.
- Economy.
- Politics.
- Research.

### 5. System Dependency Map

Describe which systems depend on which others.

For example:

```text
Planetary Formation
        |
        v
Planetary Structure
        |
        v
Tectonics
        |
        +------------------+
        |                  |
        v                  v
Geography              Geological History
        |                  |
        v                  v
Climate              Petrology
        |                  |
        v                  v
Hydrology           Mineralogy
        |                  |
        +--------+---------+
                 |
                 v
             Geochemistry
                 |
        +--------+---------+
        |                  |
        v                  v
   Ore Deposits         Petroleum
        |                  |
        v                  v
      Mining            Extraction
        |                  |
        +--------+---------+
                 |
                 v
             Processing
                 |
                 v
              Industry
```

### 6. Performance Philosophy

Explain the different simulation detail levels and their costs.

### 7. Development Philosophy

Establish that the project must evolve incrementally.

### 8. Definition of Done

Define what it means for a subsystem to be complete, tested, documented, and integrated.

---

# 01_PLANETARY_FORMATION.md

## Purpose

Describe how the planet itself originates and develops.

This document covers the planet's formation before ordinary geological history begins.

## Required contents

### 1. Protoplanetary Environment

Describe:

- Protoplanetary disks.
- Dust grains.
- Gas composition.
- Temperature gradients.
- Orbital dynamics.
- Chemical condensation.
- Radial material distribution.

### 2. Accretion

Describe:

- Dust aggregation.
- Planetesimals.
- Protoplanet formation.
- Gravitational accretion.
- Collisional growth.
- Orbital migration.

### 3. Giant Impacts

Model the conceptual consequences of major collisions:

- Mass redistribution.
- Heating.
- Melting.
- Volatile loss.
- Angular momentum changes.
- Satellite formation possibilities.

### 4. Planetary Differentiation

Describe:

- Core formation.
- Mantle formation.
- Crust formation.
- Density separation.
- Metal-silicate partitioning.
- Volatile redistribution.

### 5. Early Thermal Evolution

Include:

- Accretional heating.
- Radioactive heating.
- Gravitational differentiation.
- Magma oceans.
- Cooling.
- Solidification.
- Heat transport.

### 6. Primitive Atmosphere and Hydrosphere

Describe possible sources and evolution of:

- Water.
- Carbon dioxide.
- Nitrogen.
- Sulfur compounds.
- Noble gases.
- Other volatile substances.

### 7. Initial Planetary Conditions

Define the output of this document:

- Planet mass.
- Radius.
- Density profile.
- Core properties.
- Mantle properties.
- Crustal properties.
- Initial thermal state.
- Initial volatile inventory.
- Rotation.
- Initial magnetic-field assumptions.

### 8. Generation Modes

Provide a simplified gameplay generation mode and a more detailed scientific background mode.

### 9. Validation

Check that mass, density, energy assumptions, and compositional relationships are plausible.

---

# 02_SPHERICAL_WORLD_MODEL.md

## Purpose

Define the mathematical representation of the planet.

The global map is 2D, but the planet is geometrically spherical and local environments are fully 3D.

## Required contents

### 1. Coordinate Systems

Define:

- Planet-centered Cartesian coordinates.
- Latitude and longitude.
- Spherical coordinates.
- Local tangent-plane coordinates.
- Local 3D world coordinates.
- Geological depth coordinates.
- Surface-relative elevation.

### 2. Coordinate Conversion

Specify transformations between:

- Latitude/longitude and Cartesian positions.
- Planetary positions and local map positions.
- Global positions and local 3D coordinates.
- Surface positions and underground positions.

### 3. Planetary Geometry

Describe:

- Sphere and ellipsoid models.
- Planet radius.
- Surface distance.
- Great-circle distance.
- Local curvature.
- Surface normals.
- Gravity direction.

### 4. Global Map Representation

Define:

- Map projection.
- Distortion.
- Grid layout.
- Map resolution.
- Polar treatment.
- Longitude wrapping.
- Navigation.

### 5. Planetary Partitioning

Describe:

- Global cells.
- Regions.
- Tiles.
- Spatial indexing.
- Hierarchical subdivisions.

### 6. Local 3D Coordinate Frames

Define how a local environment is constructed from a planetary surface location.

### 7. Precision

Document numerical precision, coordinate ranges, floating-origin techniques, and precision limitations.

### 8. Spatial Queries

Specify:

- Nearest region.
- Surface elevation.
- Distance.
- Neighboring cells.
- Geological region.
- Biome.
- Climate zone.
- Resource occurrence.

### 9. Serialization

Define stable planetary location identifiers and coordinate serialization.

---

# 03_TECTONIC_SYSTEMS.md

## Purpose

Describe the movement and interaction of lithospheric plates.

Tectonics is one of the principal causal systems behind planetary geography and geology.

## Required contents

### 1. Lithospheric Structure

Describe:

- Lithosphere.
- Asthenosphere.
- Continental lithosphere.
- Oceanic lithosphere.
- Crustal thickness.
- Lithospheric age.
- Thermal state.

### 2. Plate Representation

Each plate should have:

- Stable identifier.
- Geometry.
- Area.
- Motion vector.
- Angular velocity.
- Crustal composition.
- Age distribution.
- Thermal characteristics.

### 3. Plate Motion

Describe:

- Relative movement.
- Angular velocity.
- Motion interpolation.
- Plate boundaries.
- Simplified mantle-driven motion.

### 4. Boundary Classification

Include:

- Divergent boundaries.
- Convergent boundaries.
- Transform boundaries.
- Subduction zones.
- Collision zones.
- Rift zones.
- Complex boundaries.

### 5. Geological Consequences

Model the creation of:

- Mountain belts.
- Ocean basins.
- Island arcs.
- Volcanic arcs.
- Rift valleys.
- Mid-ocean ridges.
- Accretionary complexes.
- Suture zones.

### 6. Crustal Evolution

Describe:

- Crustal growth.
- Crustal recycling.
- Crustal thickening.
- Crustal thinning.
- Continental collision.
- Terrane accretion.

### 7. Tectonic Stress

Represent:

- Compression.
- Extension.
- Shear.
- Stress orientation.
- Strain.
- Fault formation.

### 8. Seismicity

Provide an abstract earthquake-event model with magnitude, location, depth, and tectonic context.

The game does not need to reproduce every physical detail of earthquake rupture.

### 9. Volcanism

Connect tectonic conditions to magma generation and volcanic activity.

### 10. Geological Provenance

Every major tectonic feature should be traceable to its formation context.

---

# 04_GEOGRAPHY_AND_TERRAIN.md

## Purpose

Generate planetary surface geography from tectonics, geological history, erosion, climate, and hydrology.

## Required contents

### 1. Continental Configuration

Describe:

- Continental nuclei.
- Cratons.
- Shields.
- Continental margins.
- Continental collisions.
- Ocean basins.
- Island systems.

### 2. Elevation Model

Define:

- Elevation.
- Bathymetry.
- Relief.
- Slope.
- Aspect.
- Curvature.
- Terrain roughness.

### 3. Major Landforms

Include:

- Mountains.
- Plateaus.
- Plains.
- Basins.
- Valleys.
- Canyons.
- Deserts.
- Coastal shelves.
- Volcanic provinces.

### 4. Geomorphological Processes

Model:

- Weathering.
- Erosion.
- Sediment transport.
- Deposition.
- Mass wasting.
- Glacial erosion.
- River incision.
- Coastal erosion.

### 5. Sedimentary Basins

Define basin geometry, subsidence, sediment supply, and filling.

### 6. Terrain Hierarchy

Describe how large-scale terrain is translated into regional and local terrain.

### 7. Procedural Terrain Generation

Specify:

- Initial terrain.
- Tectonic contribution.
- Geological contribution.
- Erosion passes.
- Hydrological correction.
- Detail generation.

### 8. Terrain Materials

Map geological materials to surface materials without treating them as interchangeable concepts.

### 9. Validation

Check drainage consistency, slope continuity, realistic elevation ranges, and plausible geological structures.

---

# 05_CLIMATE_SYSTEM.md

## Purpose

Simulate planetary atmosphere and climate.

## Required contents

### 1. Atmospheric Composition

Track major atmospheric constituents and relevant trace gases.

### 2. Atmospheric Structure

Describe:

- Pressure.
- Density.
- Temperature.
- Vertical layers.
- Humidity.
- Atmospheric mass.

### 3. Planetary Energy Balance

Include:

- Stellar radiation.
- Albedo.
- Absorption.
- Emission.
- Greenhouse effects.
- Heat storage.
- Surface-atmosphere exchange.

### 4. Atmospheric Circulation

Represent:

- Hadley-like circulation.
- Mid-latitude circulation.
- Polar circulation.
- Prevailing winds.
- Pressure systems.
- Seasonal shifts.

### 5. Moisture Cycle

Track:

- Evaporation.
- Condensation.
- Precipitation.
- Atmospheric transport.
- Snow accumulation.
- Sublimation.

### 6. Climate Classification

Generate climate zones from temperature and moisture statistics.

### 7. Seasonality

Account for:

- Axial tilt.
- Orbital parameters.
- Seasonal solar radiation.
- Day length.
- Seasonal precipitation.

### 8. Long-Term Climate

Support abstract climate changes driven by:

- Atmospheric composition.
- Volcanism.
- Continental arrangement.
- Orbital variations.
- Ocean circulation.
- Ice coverage.

### 9. Extreme Events

Represent storms, droughts, heat waves, cold waves, and other relevant events at a gameplay-appropriate level.

### 10. Climate Outputs

Provide regional temperature, precipitation, wind, humidity, seasonality, and climate classification.

---

# 06_HYDROLOGY_SYSTEM.md

## Purpose

Model water movement across and beneath the planet.

## Required contents

### 1. Global Water Inventory

Define:

- Ocean water.
- Surface water.
- Groundwater.
- Ice.
- Atmospheric water.
- Soil moisture.

### 2. Water Cycle

Model:

- Evaporation.
- Transpiration.
- Condensation.
- Precipitation.
- Infiltration.
- Runoff.
- Groundwater recharge.
- Discharge.

### 3. Drainage Networks

Describe:

- Watersheds.
- Catchments.
- Drainage basins.
- River networks.
- Tributaries.
- River orders.
- Endorheic basins.

### 4. Rivers

Track:

- Discharge.
- Flow direction.
- Channel slope.
- Sediment load.
- Erosion.
- Deposition.
- Flooding.
- Seasonal variation.

### 5. Lakes and Wetlands

Describe:

- Lake formation.
- Water balance.
- Sedimentation.
- Salinity.
- Wetland hydrology.

### 6. Groundwater

Include:

- Aquifers.
- Aquitards.
- Porosity.
- Permeability.
- Hydraulic head.
- Recharge.
- Discharge.
- Groundwater flow.

### 7. Hydrochemistry

Track dissolved substances, salinity, pH, hardness, and other useful chemical characteristics.

### 8. Glaciers and Ice

Describe accumulation, melting, ice movement, and glacial water contribution.

### 9. Water Resource Outputs

Provide water availability, quality, seasonal reliability, and extraction constraints.

---

# 07_GEOLOGICAL_HISTORY.md

## Purpose

Provide the temporal framework that explains how geological structures and materials formed.

## Required contents

### 1. Geological Time

Define:

- Eons.
- Eras.
- Periods.
- Epochs.
- Geological events.
- Relative dating.
- Absolute dating.

### 2. Geological Event Model

Each event should have:

- Identifier.
- Start time.
- End time.
- Location.
- Event type.
- Intensity or magnitude.
- Parent events.
- Resulting structures.
- Material transformations.

### 3. Event Types

Include:

- Volcanic episodes.
- Intrusions.
- Metamorphic events.
- Sedimentation.
- Uplift.
- Subsidence.
- Folding.
- Faulting.
- Erosion.
- Glaciation.
- Hydrothermal alteration.
- Mineralization.

### 4. Stratigraphic Relationships

Describe:

- Superposition.
- Cross-cutting relationships.
- Unconformities.
- Intrusive relationships.
- Metamorphic overprinting.

### 5. Dating Systems

Distinguish relative dating from numerical age estimates.

### 6. Geological Time Machine

Provide a system capable of reconstructing selected historical states.

Possible outputs:

- Ancient continental arrangement.
- Former mountain belts.
- Ancient coastlines.
- Buried sedimentary basins.
- Eroded volcanic systems.
- Historical mineralization environments.

### 7. Geological Provenance Graph

Connect present-day features to their historical causes.

### 8. Historical Compression

Specify how long geological periods are summarized without simulating every instant.

---

# 08_PETROLOGY_AND_ROCKS.md

## Purpose

Define the rock system: how rocks are classified, formed, transformed, and distributed.

## Required contents

### 1. Rock Classification

Include:

- Igneous rocks.
- Sedimentary rocks.
- Metamorphic rocks.

### 2. Igneous Petrology

Describe:

- Magma composition.
- Crystallization.
- Intrusive rocks.
- Extrusive rocks.
- Cooling rate.
- Texture.
- Mineral assemblages.

### 3. Sedimentary Petrology

Include:

- Clastic rocks.
- Chemical sedimentary rocks.
- Biogenic sedimentary rocks.
- Grain size.
- Sorting.
- Rounding.
- Cementation.
- Compaction.

### 4. Metamorphic Petrology

Describe:

- Pressure.
- Temperature.
- Fluids.
- Protolith.
- Metamorphic grade.
- Facies.
- Recrystallization.

### 5. Rock Composition

Represent rocks using mineral proportions and relevant chemical composition.

### 6. Rock Textures

Include grain size, fabric, porosity, layering, foliation, and other structural properties.

### 7. Rock Transformations

Describe:

- Weathering.
- Burial.
- Melting.
- Metamorphism.
- Recrystallization.
- Alteration.
- Erosion.

### 8. Mechanical Properties

Where needed, represent density, hardness, strength, brittleness, porosity, and permeability.

### 9. Rock Data Definitions

Specify reusable rock templates and generated rock instances.

---

# 09_MINERALOGY.md

## Purpose

Define minerals as structured geological materials rather than simple inventory categories.

## Required contents

### 1. Mineral Definition

Distinguish minerals, mineraloids, amorphous materials, and mineral-like industrial substances.

### 2. Chemical Composition

Define:

- Chemical formula.
- Elemental composition.
- Substitution.
- Solid solutions.
- Trace elements.

### 3. Crystal Structure

Describe:

- Crystal systems.
- Lattice parameters.
- Unit cells.
- Symmetry.
- Crystal habits.
- Polymorphism.
- Crystal defects.

### 4. Mineral Properties

Include:

- Density.
- Hardness.
- Cleavage.
- Fracture.
- Luster.
- Color.
- Streak.
- Magnetism.
- Electrical properties.
- Thermal properties.

### 5. Mineral Formation

Connect mineral formation to:

- Magmatic processes.
- Hydrothermal systems.
- Sedimentary processes.
- Metamorphism.
- Weathering.
- Evaporation.
- Biological processes.

### 6. Mineral Assemblages

Describe mineral associations and equilibrium or non-equilibrium assemblages.

### 7. Mineral Zoning

Represent compositional zoning and alteration when appropriate.

### 8. Mineral Occurrences

Distinguish:

- Mineral grains.
- Disseminations.
- Veins.
- Massive bodies.
- Layers.
- Placers.
- Replacement bodies.

### 9. Mineral Data Registry

Define stable identifiers and structured mineral records.

### 10. Computational Levels

Support simple mineral categories and advanced crystal-chemical descriptions without requiring full crystal simulation.

---

# 10_CHEMISTRY_PLATFORM.md

## Purpose

Provide a common chemical data and reaction framework for geology, ecology, water, materials science, and industry.

This is one of the most important foundational documents.

## Required contents

### 1. Periodic Table

Support all 118 currently recognized chemical elements.

For each element, define relevant properties:

- Atomic number.
- Symbol.
- Name.
- Standard atomic weight where applicable.
- Electron configuration.
- Common oxidation states.
- Relevant isotopes.
- Physical properties.
- Chemical classification.

Do not assume that every property must be simulated in every game process.

### 2. Isotopes

Represent:

- Mass number.
- Abundance.
- Stability.
- Radioactive decay.
- Half-life.
- Decay products.

### 3. Chemical Species

Distinguish:

- Elements.
- Ions.
- Molecules.
- Radicals.
- Complexes.
- Aqueous species.
- Gaseous species.
- Solid compounds.

### 4. Chemical Bonds and Structures

Describe:

- Ionic bonding.
- Covalent bonding.
- Metallic bonding.
- Molecular geometry.
- Coordination.
- Crystal chemistry.

### 5. Compounds

Support compounds with:

- Formula.
- Composition.
- Molecular or structural representation where needed.
- Physical state.
- Relevant properties.

### 6. Chemical Reactions

Define:

- Reactants.
- Products.
- Stoichiometry.
- Reaction conditions.
- Reaction rates.
- Reaction enthalpy.
- Catalysts.
- Side products.
- Reversibility.

### 7. Thermodynamics

Include:

- Temperature.
- Pressure.
- Enthalpy.
- Entropy.
- Gibbs free energy.
- Heat capacity.
- Phase stability.

### 8. Chemical Kinetics

Describe:

- Reaction rates.
- Activation energy.
- Concentration dependence.
- Temperature dependence.
- Catalysis.
- Rate-limiting steps.

### 9. Equilibrium

Include:

- Equilibrium constants.
- Mass action.
- Chemical potential.
- Phase equilibrium.
- Solubility equilibrium.

### 10. Aqueous Chemistry

Support:

- Dissolution.
- Precipitation.
- Hydration.
- Complexation.
- Hydrolysis.
- Acid-base behavior.
- pH.
- Buffering.
- Ionic strength.

### 11. Redox Chemistry

Track oxidation states, electron transfer, and oxidation-reduction reactions.

### 12. Electrochemistry

Describe electrode potentials, electrolysis, galvanic processes, and relevant industrial applications.

### 13. Phase Transitions

Include melting, freezing, boiling, condensation, sublimation, and solid-state transformations.

### 14. Reaction Networks

Allow multiple reactions to interact through shared species and changing conditions.

### 15. Numerical Solvers

Document:

- Stoichiometric matrices.
- Mass-balance equations.
- Equilibrium solvers.
- Kinetic integration.
- Stability.
- Convergence.
- Simplified lookup-based alternatives.

### 16. Chemical Accuracy Levels

Define:

- Category-level chemistry.
- Stoichiometric chemistry.
- Thermochemical chemistry.
- Detailed reaction-network chemistry.

### 17. Data Validation

Check elemental balance, charge balance, units, valid formulas, and reaction consistency.

---

# 11_GEOCHEMISTRY.md

## Purpose

Connect chemical processes to geological materials and environments.

## Required contents

### 1. Element Distribution

Describe:

- Crustal abundance.
- Mantle abundance.
- Core partitioning.
- Mineral-hosted elements.
- Trace-element behavior.

### 2. Geochemical Reservoirs

Define reservoirs in:

- Atmosphere.
- Hydrosphere.
- Lithosphere.
- Biosphere.
- Mantle.
- Core.

### 3. Element Mobility

Model how elements move through geological processes.

### 4. Partitioning

Describe element distribution between:

- Melt and crystals.
- Rock and fluid.
- Minerals and aqueous solutions.
- Different mineral phases.

### 5. Geochemical Signatures

Represent:

- Major elements.
- Trace elements.
- Rare earth elements.
- Isotopic ratios.
- Geochemical anomalies.

### 6. Weathering Chemistry

Describe chemical weathering, dissolution, oxidation, hydration, and secondary mineral formation.

### 7. Hydrothermal Geochemistry

Include fluid-rock interactions, solute transport, precipitation, and alteration.

### 8. Geochemical Cycles

Cover carbon, sulfur, phosphorus, nitrogen, iron, and other important cycles.

### 9. Geochemical Mapping

Generate spatial layers for elemental abundance, anomalies, alteration, and mineral potential.

### 10. Provenance

Explain how geochemical signatures originate from source rocks, fluids, and later transformations.

---

# 12_ORE_DEPOSIT_GENERATION.md

## Purpose

Generate mineral deposits as geological bodies with meaningful origin, geometry, composition, and economic properties.

The objective is not to scatter random ore blocks across a map.

## Required contents

### 1. Deposit Classification

Include major deposit families:

- Magmatic.
- Hydrothermal.
- Porphyry.
- Epithermal.
- Volcanogenic massive sulfide.
- Sedimentary-exhalative.
- Skarn.
- Greisen.
- Pegmatite.
- Orogenic gold.
- Iron formation.
- Lateritic.
- Placer.
- Sedimentary chemical.
- Residual.
- Metamorphic-related.

### 2. Deposit Formation Conditions

Describe the relevant geological conditions for each deposit family:

- Tectonic setting.
- Source materials.
- Temperature.
- Pressure.
- Fluids.
- Chemical environment.
- Host rock.
- Structural controls.
- Timing.

### 3. Deposit Geometry

Represent:

- Veins.
- Stockworks.
- Disseminated bodies.
- Massive sulfide bodies.
- Stratiform layers.
- Replacement bodies.
- Pipes.
- Breccia bodies.
- Placer accumulations.

### 4. Composition

Track:

- Main minerals.
- Gangue minerals.
- Valuable elements.
- Deleterious elements.
- Alteration minerals.
- Grade distribution.

### 5. Spatial Structure

Include:

- Boundaries.
- Thickness.
- Continuity.
- Depth.
- Orientation.
- Zoning.
- Internal heterogeneity.

### 6. Resource Quantification

Define:

- Total material.
- Valuable material.
- Grade.
- Tonnage.
- Recoverability.
- Dilution.
- Uncertainty.

### 7. Economic Classification

Distinguish geological occurrence from economically recoverable resource.

Economic viability depends on technology, prices, infrastructure, regulations, and operating costs.

### 8. Exploration Clues

Generate observable indicators:

- Surface alteration.
- Geochemical anomalies.
- Magnetic anomalies.
- Structural patterns.
- Rock associations.
- Geophysical signatures.

### 9. Deposit History

Store formation and modification events.

### 10. Mining Interface

Expose deposits to mining systems without requiring mining code to understand the full geological model.

---

# 13_PETROLEUM_SYSTEM.md

## Purpose

Model petroleum generation, migration, accumulation, and extraction.

## Required contents

### 1. Organic Matter

Describe organic-rich sedimentary materials and their preservation conditions.

### 2. Source Rocks

Track:

- Organic richness.
- Kerogen type.
- Depositional environment.
- Thermal maturity.
- Burial history.

### 3. Burial and Thermal Evolution

Describe:

- Burial depth.
- Temperature history.
- Heating duration.
- Maturation.
- Generation windows.

### 4. Petroleum Generation

Represent transformation of organic matter into hydrocarbons using an appropriate level of abstraction.

### 5. Migration

Describe:

- Primary migration.
- Secondary migration.
- Carrier beds.
- Permeability.
- Buoyancy.
- Fluid pathways.

### 6. Reservoirs

Track:

- Porosity.
- Permeability.
- Pore structure.
- Fluid saturation.
- Reservoir geometry.

### 7. Seals and Traps

Include:

- Structural traps.
- Stratigraphic traps.
- Combination traps.
- Cap rocks.
- Fault sealing.

### 8. Petroleum Composition

Distinguish crude oil, natural gas, condensates, and associated fluids.

### 9. Reservoir Properties

Include pressure, temperature, fluid properties, saturation, and estimated recoverability.

### 10. Extraction Interface

Connect geological resources to drilling, production, processing, storage, and transport.

### 11. Uncertainty

Distinguish discovered accumulations from probabilistic exploration targets.

---

# 14_GEOPHYSICS.md

## Purpose

Describe the physical fields and measurements used to infer planetary and subsurface structure.

## Required contents

### 1. Gravity

Represent:

- Gravitational acceleration.
- Density anomalies.
- Regional gravity variation.
- Gravity observations.

### 2. Magnetic Field

Describe:

- Planetary magnetic field.
- Crustal magnetic anomalies.
- Magnetic minerals.
- Field measurements.

### 3. Seismic Properties

Include:

- P-wave velocity.
- S-wave velocity.
- Density.
- Attenuation.
- Acoustic impedance.
- Simplified seismic propagation.

### 4. Thermal Properties

Track:

- Geothermal gradient.
- Heat flow.
- Conductivity.
- Heat capacity.
- Radiogenic heat production.

### 5. Electrical Properties

Describe resistivity and conductivity as functions of material, fluid, temperature, and composition.

### 6. Geophysical Observations

Represent measurements with location, instrument, uncertainty, and resolution.

### 7. Inversion

Explain how observations can be used to estimate hidden geological structures.

### 8. Tomographic Models

Support abstract 2D and 3D reconstructions.

### 9. Exploration Integration

Connect geophysical signals to survey and prospecting systems.

### 10. Performance

Avoid globally solving expensive physical fields at unnecessary resolution.

---

# 15_OCEANOLOGY.md

## Purpose

Model oceans as physical, chemical, geological, and ecological environments.

## Required contents

### 1. Ocean Geometry

Include:

- Ocean basins.
- Continental shelves.
- Slopes.
- Abyssal plains.
- Trenches.
- Mid-ocean ridges.

### 2. Physical Oceanography

Represent:

- Temperature.
- Salinity.
- Density.
- Pressure.
- Currents.
- Mixing.
- Stratification.

### 3. Ocean Circulation

Describe large-scale circulation, regional currents, upwelling, and deep-water formation.

### 4. Ocean Chemistry

Track:

- Dissolved gases.
- Salinity.
- Carbonate chemistry.
- Nutrients.
- pH.
- Dissolved minerals.

### 5. Marine Sedimentation

Include:

- Terrigenous sediment.
- Biogenic sediment.
- Chemical sediment.
- Turbidity deposits.
- Pelagic deposits.

### 6. Seafloor Geology

Connect ocean floors to tectonics, volcanism, hydrothermal systems, and sedimentation.

### 7. Hydrothermal Vents

Model their geological and chemical influence.

### 8. Marine Ecosystems

Connect temperature, light, nutrients, depth, chemistry, and habitat.

### 9. Coastal Systems

Describe deltas, estuaries, reefs, beaches, lagoons, and coastal erosion.

### 10. Marine Resources

Support fisheries, minerals, hydrocarbons, and other relevant resources.

---

# 16_SOILS_AND_ECOLOGY.md

## Purpose

Connect surface geology, climate, hydrology, biology, and ecosystems.

## Required contents

### 1. Soil Formation

Include:

- Parent material.
- Climate.
- Organisms.
- Relief.
- Time.
- Weathering.

### 2. Soil Horizons

Describe soil layers and their properties.

### 3. Soil Properties

Track:

- Texture.
- Structure.
- Organic matter.
- Moisture.
- pH.
- Salinity.
- Nutrients.
- Porosity.
- Permeability.

### 4. Soil Chemistry

Represent nutrient availability, mineral transformations, and relevant chemical processes.

### 5. Ecosystem Classification

Include forests, grasslands, deserts, tundra, wetlands, and aquatic ecosystems.

### 6. Species and Functional Groups

Support plants, animals, fungi, microorganisms, and ecological roles.

### 7. Population Dynamics

Describe:

- Birth.
- Death.
- Migration.
- Growth.
- Competition.
- Predation.
- Resource limitation.

### 8. Ecological Succession

Model ecosystem changes after disturbances and environmental changes.

### 9. Biogeochemical Cycles

Connect organisms to carbon, nitrogen, phosphorus, sulfur, and water cycles.

### 10. Human Impact

Include land use, pollution, mining, agriculture, deforestation, and habitat fragmentation.

### 11. Ecological Services

Represent soil fertility, water regulation, biomass production, and other useful outputs.

---

# 17_PALEONTOLOGY_AND_EVOLUTION.md

## Purpose

Describe biological history and the preservation of evidence of past life.

## Required contents

### 1. Evolutionary Framework

Represent:

- Lineages.
- Speciation.
- Extinction.
- Adaptation.
- Diversification.
- Migration.

### 2. Biological Time

Connect biological history to geological time.

### 3. Fossilization

Describe:

- Burial.
- Preservation.
- Mineral replacement.
- Compression.
- Impressions.
- Trace fossils.
- Exceptional preservation.

### 4. Fossil Distribution

Connect fossil occurrences to depositional environments and later geological transformations.

### 5. Paleoecology

Reconstruct ancient ecosystems from fossil and geological evidence.

### 6. Mass Extinctions

Represent major extinction events and ecological consequences at an appropriate abstraction level.

### 7. Ancient Climate

Connect fossil evidence to historical climate reconstruction.

### 8. Geological Interpretation

Distinguish actual historical biology from what can be inferred from preserved evidence.

### 9. Player Interaction

Potential gameplay uses:

- Fossil discovery.
- Scientific research.
- Stratigraphic dating.
- Resource exploration.
- Natural history collections.

---

# 18_MATERIALS_SCIENCE.md

## Purpose

Describe how material structure determines industrially relevant properties.

This document bridges chemistry, geology, engineering, and manufacturing.

## Required contents

### 1. Material Categories

Include:

- Pure elements.
- Minerals.
- Ceramics.
- Metals.
- Alloys.
- Polymers.
- Glasses.
- Composites.
- Refractory materials.
- Construction materials.

### 2. Material Structure

Represent:

- Crystal phases.
- Amorphous phases.
- Grain structure.
- Porosity.
- Defects.
- Inclusions.
- Interfaces.

### 3. Phase Diagrams

Describe:

- Pure substance phase transitions.
- Binary systems.
- Multicomponent systems.
- Solid solutions.
- Eutectics.
- Intermetallic compounds.

### 4. Mechanical Properties

Include:

- Density.
- Elastic modulus.
- Yield strength.
- Tensile strength.
- Hardness.
- Toughness.
- Ductility.
- Brittleness.
- Fatigue behavior.

### 5. Thermal Properties

Include:

- Thermal conductivity.
- Heat capacity.
- Expansion.
- Melting range.
- Thermal shock resistance.

### 6. Electrical and Magnetic Properties

Represent conductivity, resistivity, magnetic response, and relevant functional properties.

### 7. Corrosion and Degradation

Describe corrosion, oxidation, chemical attack, wear, creep, and fatigue.

### 8. Material Processing

Connect processing history to final material properties.

### 9. Quality and Defects

Track contamination, porosity, inclusions, cracks, and other quality factors.

### 10. Industrial Material Registry

Define material identifiers, compositions, properties, and processing histories.

---

# 19_CHEMICAL_INDUSTRY.md

## Purpose

Define the industrial transformation of chemical substances.

This document must use the common Chemistry Platform and Universal Process Framework.

## Required contents

### 1. Industrial Process Taxonomy

Include:

- Mixing.
- Dissolution.
- Leaching.
- Precipitation.
- Crystallization.
- Distillation.
- Evaporation.
- Filtration.
- Drying.
- Calcination.
- Roasting.
- Oxidation.
- Reduction.
- Electrolysis.
- Chemical synthesis.
- Purification.
- Gas treatment.
- Waste treatment.

### 2. Process Units

Define reusable equipment categories:

- Reactors.
- Vessels.
- Separators.
- Columns.
- Filters.
- Heat exchangers.
- Pumps.
- Compressors.
- Furnaces.
- Electrochemical cells.

### 3. Process Conditions

Each process may depend on:

- Temperature.
- Pressure.
- Residence time.
- Concentration.
- Flow rate.
- pH.
- Energy.
- Catalyst.
- Mixing.
- Equipment condition.

### 4. Input and Output Accounting

Every process must specify:

- Inputs.
- Outputs.
- Byproducts.
- Waste.
- Energy consumption.
- Water consumption.
- Losses.
- Efficiency.

### 5. Process Networks

Describe how individual process units form production chains.

### 6. Plant Architecture

Include:

- Material storage.
- Transfer.
- Process units.
- Utilities.
- Cooling.
- Heating.
- Waste handling.
- Product storage.

### 7. Industrial Utilities

Model water, electricity, steam, cooling, compressed gases, and other relevant utilities.

### 8. Process Control

Support operating conditions, setpoints, throughput constraints, and process stability at a gameplay-appropriate level.

### 9. Environmental Impact

Track emissions, wastewater, solid waste, thermal discharge, and resource consumption.

### 10. Maintenance

Represent equipment wear, downtime, replacement parts, and maintenance requirements.

### 11. Process Documentation

Each industrial process should have a documented input-output specification and validation test.

---

# 20_MINING_AND_METALLURGY.md

## Purpose

Connect geological resource extraction to industrial mineral processing and metal production.

## Required contents

### 1. Mining Methods

Describe relevant surface and underground mining categories.

### 2. Mine Geometry

Include:

- Orebody boundaries.
- Excavation volumes.
- Waste rock.
- Access infrastructure.
- Geological contacts.
- Ground conditions.

### 3. Extraction

Represent:

- Excavation.
- Drilling.
- Blasting as an abstract production process.
- Loading.
- Hauling.
- Dewatering.
- Ventilation.
- Ground support.

### 4. Mining Operations

Track:

- Equipment.
- Labor.
- Energy.
- Water.
- Throughput.
- Recovery.
- Dilution.
- Waste.
- Operating costs.

### 5. Mineral Processing

Include:

- Crushing.
- Grinding.
- Screening.
- Classification.
- Gravity separation.
- Magnetic separation.
- Flotation.
- Leaching.
- Thickening.
- Filtration.
- Drying.

### 6. Metallurgy

Include:

- Smelting.
- Converting.
- Roasting.
- Reduction.
- Refining.
- Electrorefining.
- Electrowinning.
- Alloy production.

### 7. Metal Products

Define grades, purity, impurities, and product specifications.

### 8. Tailings and Waste

Track tailings, slag, waste rock, process residues, and disposal requirements.

### 9. Resource Depletion

Connect extraction to remaining geological resources and changing mine conditions.

### 10. Mine Economics

Calculate production costs, recovery, revenue, investment, and operational risk.

### 11. Geological Feedback

Extraction must modify the actual local geological environment rather than simply decrementing an abstract resource counter.

---

# 21_WORLD_GENERATION_PIPELINE.md

## Purpose

Specify the complete procedural generation sequence.

This document explains how all planetary systems combine into one coherent world.

## Required contents

### 1. Generation Inputs

Define:

- Planet seed.
- Planet parameters.
- Generation configuration.
- Geological parameters.
- Climate parameters.
- Content registry versions.

### 2. Generation Stages

Proposed order:

1. Generate planetary parameters.
2. Construct initial planetary structure.
3. Establish geological background.
4. Generate tectonic plates.
5. Simulate broad tectonic evolution.
6. Generate continental and oceanic configuration.
7. Construct major geological provinces.
8. Generate geological history.
9. Generate major rock units.
10. Generate mineralogical composition.
11. Generate geological structures.
12. Generate mineralization potential.
13. Generate major deposits.
14. Generate elevation and bathymetry.
15. Generate climate.
16. Generate ocean circulation.
17. Generate hydrological networks.
18. Generate soil properties.
19. Generate ecosystems.
20. Generate resource and exploration metadata.
21. Build local 3D generation parameters.
22. Validate the resulting world.
23. Serialize generation metadata.

The exact dependency order must be justified. Some stages may require iteration.

### 3. Intermediate Representations

Define what each generation stage consumes and produces.

### 4. Deterministic Generation

Specify stable seed derivation per subsystem and spatial region.

### 5. Lazy Generation

Describe how detailed information is generated only when required.

### 6. Generation Caching

Define cache keys, invalidation, persistence, and versioning.

### 7. Generation Validation

Check cross-system consistency.

### 8. Regeneration

Define what happens when generation rules or content data change.

### 9. Generation Diagnostics

Record generation statistics and anomalies.

---

# 22_CHUNK_AND_LOD_ARCHITECTURE.md

## Purpose

Define spatial partitioning, streaming, and detail management.

Chunks are an implementation and streaming mechanism. They are not the fundamental unit of geological origin.

## Required contents

### 1. Spatial Hierarchy

Describe:

- Planet.
- Region.
- Tile.
- Chunk.
- Local environment.
- Subsurface volume.
- Industrial site.

### 2. Chunk Responsibilities

Specify:

- Loading.
- Unloading.
- Serialization.
- Rendering.
- Local simulation.
- Spatial queries.
- Entity storage.

### 3. LOD Levels

Define multiple representations of:

- Terrain.
- Geological structure.
- Climate.
- Hydrology.
- Ecosystems.
- Resource distribution.
- Industrial simulation.

### 4. LOD Transitions

Describe how detail is added or removed while preserving state.

### 5. Simulation Frequency

Specify different update frequencies for different levels.

### 6. Background Simulation

Describe how distant regions continue evolving without full local simulation.

### 7. Memory Management

Define budgets, caches, eviction, and resource ownership.

### 8. Streaming

Specify asynchronous loading and generation.

### 9. Persistence

Define how chunk data and non-chunk planetary data are saved.

### 10. Spatial Consistency

Ensure borders, rivers, geological bodies, and other continuous structures remain coherent across chunk boundaries.

---

# 23_SURVEY_AND_EXPLORATION.md

## Purpose

Define how the player discovers the world and gradually builds knowledge about its resources and structure.

The player should not automatically know everything the generation system knows.

## Required contents

### 1. Knowledge Model

Distinguish:

- True world state.
- Observed information.
- Estimated information.
- Inferred information.
- Unknown information.

### 2. Exploration Methods

Include:

- Surface inspection.
- Geological mapping.
- Sampling.
- Drilling.
- Geochemical surveys.
- Geophysical surveys.
- Remote observation.
- Laboratory analysis.

### 3. Survey Data

Every observation should record:

- Location.
- Time.
- Method.
- Instrument.
- Resolution.
- Measurement.
- Uncertainty.
- Provenance.

### 4. Geological Mapping

Support geological units, boundaries, structures, and confidence estimates.

### 5. Resource Exploration

Describe prospect generation, target ranking, sampling, and resource estimation.

### 6. Scientific Interpretation

Allow the player to infer hidden geological structures from observations.

### 7. Information Quality

Represent measurement errors, incomplete coverage, and competing interpretations.

### 8. Exploration Economics

Track equipment, time, labor, energy, and cost.

### 9. Knowledge Visualization

Define maps and overlays for known and unknown information.

### 10. Research Integration

Connect discoveries to scientific knowledge and technological development.

---

# 24_UNIVERSAL_PROCESS_FRAMEWORK.md

## Purpose

Create one shared conceptual and technical framework for processes across the entire simulation.

This is arguably the most important integration document.

Geology, chemistry, hydrology, ecology, industry, and social systems all transform state over time.

The framework must allow them to share infrastructure without pretending they obey identical physical laws.

## 1. Universal Process Concept

A process is a transformation of one or more states under specified conditions.

Conceptually:

```text
Initial State
     |
     v
Input Evaluation
     |
     v
Condition Evaluation
     |
     v
Transformation Model
     |
     v
Output Calculation
     |
     v
Conservation and Validation
     |
     v
State Update
     |
     v
Event and History Recording
```

## 2. Process Categories

Define categories:

- Physical processes.
- Chemical processes.
- Geological processes.
- Thermal processes.
- Mechanical processes.
- Hydrological processes.
- Biological processes.
- Ecological processes.
- Industrial processes.
- Economic processes.
- Social processes.
- Political processes.

These categories may share interfaces, but must not be forced into one universal numerical solver.

## 3. Process Definition

A process definition should include:

- Stable identifier.
- Name.
- Category.
- Description.
- Inputs.
- Outputs.
- Required conditions.
- Parameters.
- Time model.
- Spatial scope.
- Resolution requirements.
- Conservation rules.
- Validation rules.
- Implementation model.
- Data version.

## 4. Process Instance

An active process instance should track:

- Process definition.
- Location.
- State.
- Progress.
- Inputs.
- Outputs.
- Start time.
- Last update.
- Current conditions.
- Completion state.
- Failure state.
- Relevant history.

## 5. State Transformation

Define how a process reads and modifies simulation state.

Specify:

- Read dependencies.
- Write dependencies.
- Transaction boundaries.
- Update ordering.
- Conflict resolution.
- Failure handling.

## 6. Time Models

Support:

- Instantaneous events.
- Fixed-duration processes.
- Continuous-rate approximations.
- Discrete timestep simulation.
- Event-driven updates.
- Long-term accumulated processes.

## 7. Spatial Models

Support:

- Point processes.
- Local volume processes.
- Surface processes.
- Regional processes.
- Global processes.
- Network processes.

## 8. Conservation

Where applicable, validate:

- Mass.
- Elemental composition.
- Charge.
- Energy.
- Water.
- Material quantities.

For economic and social processes, define the appropriate accounting rules separately.

## 9. Process Networks

Processes can be connected into dependency graphs.

Examples:

```text
Mining
  |
  v
Crushing
  |
  v
Grinding
  |
  v
Separation
  |
  v
Concentrate
  |
  v
Smelting
  |
  v
Refining
  |
  v
Metal Product
```

Another example:

```text
Precipitation
  |
  v
Surface Runoff
  |
  v
River Transport
  |
  v
Sediment Deposition
  |
  v
Burial
  |
  v
Compaction
  |
  v
Sedimentary Rock
```

## 10. Process Scheduling

Specify:

- Update frequency.
- Priority.
- Dependency ordering.
- Event queues.
- Simulation budgets.
- Deferred processing.

## 11. Process Accuracy Levels

Define simplified, standard, and detailed implementations for compatible processes.

A process must declare which state variables are lost or approximated at each level.

## 12. Process History

Allow important transformations to be recorded for inspection, debugging, scientific analysis, and player-facing history.

## 13. Process Validation

Every process must have:

- Unit tests.
- Input validation.
- Output validation.
- Conservation checks where applicable.
- Determinism checks.
- Performance measurements.

## 14. Failure Modes

Describe how the simulation handles:

- Invalid inputs.
- Missing materials.
- Impossible conditions.
- Numerical instability.
- Resource exhaustion.
- Conflicting state updates.

## 15. Integration Requirements

Every subsystem document must explain how its processes use this framework.

---

# 25_DATA_SCHEMAS_AND_CONTENT_FORMAT.md

## Purpose

Define the shared data model used by the simulation and external content definitions.

## Required contents

### 1. Identifier System

Specify stable IDs for:

- Elements.
- Isotopes.
- Compounds.
- Minerals.
- Rocks.
- Materials.
- Reactions.
- Processes.
- Machines.
- Buildings.
- Geological structures.
- Biomes.
- Species.
- Resources.

### 2. Units

Define a common unit system.

Use SI units internally where appropriate.

Specify:

- Mass.
- Length.
- Time.
- Temperature.
- Pressure.
- Energy.
- Power.
- Amount of substance.
- Concentration.
- Density.
- Flow.
- Currency.
- Labor.

### 3. Schema Format

Define the chosen data serialization formats.

Potential formats:

- JSON for general structured data.
- Dedicated binary formats for runtime or large datasets.
- CSV for simple tabular source data where appropriate.

Do not create multiple formats without a concrete reason.

### 4. Schema Versioning

Define:

- Version numbers.
- Migration.
- Compatibility.
- Deprecated fields.
- Validation.

### 5. Content Registries

Describe how content is loaded, validated, indexed, and accessed.

### 6. Composition Models

Define shared representations for elemental, chemical, mineralogical, and material compositions.

### 7. Reaction Schema

Define balanced chemical reactions and their conditions.

### 8. Process Schema

Define process metadata and execution configuration.

### 9. Geological Schema

Define rocks, minerals, geological units, structures, deposits, and events.

### 10. Industrial Schema

Define machines, equipment, production recipes, material flows, and utilities.

### 11. Localization

Separate player-facing names and descriptions from stable identifiers.

### 12. Example Definitions

Provide complete example files for:

- An element.
- A mineral.
- A rock.
- A chemical compound.
- A reaction.
- An ore deposit type.
- A machine.
- An industrial process.
- A geological event.

---

# 26_VALIDATION_AND_TESTING.md

## Purpose

Define how the simulation proves that its systems are functioning correctly.

## Required contents

### 1. Unit Testing

Test individual mathematical and transformation functions.

### 2. Integration Testing

Test interactions between systems.

### 3. Determinism Testing

Verify repeatable results for identical initial conditions.

### 4. Conservation Testing

Check mass, elemental, charge, energy, and other relevant balances.

### 5. Numerical Testing

Test solver convergence, stability, edge cases, and error accumulation.

### 6. Geological Validation

Check:

- Plausible rock associations.
- Geological age relationships.
- Tectonic consistency.
- Deposit formation context.
- Stratigraphic relationships.

### 7. Geographic Validation

Check:

- Terrain continuity.
- Drainage consistency.
- Slope validity.
- Coastline continuity.
- Spatial boundaries.

### 8. Climate Validation

Check energy and water accounting at the selected simulation level.

### 9. Industrial Validation

Check:

- Input-output balances.
- Throughput.
- Equipment constraints.
- Waste accounting.
- Process conditions.

### 10. Performance Testing

Measure:

- Generation time.
- Tick duration.
- Memory.
- Loading time.
- Serialization.
- Query performance.

### 11. Regression Testing

Preserve known test worlds and expected outputs.

### 12. Property-Based Testing

Test general invariants over many generated inputs.

### 13. Test Worlds

Define standardized small planets and geological scenarios for development.

### 14. Error Reporting

Ensure failures identify the relevant subsystem, process, data record, and simulation context.

---

# 27_DEBUG_AND_VISUALIZATION_TOOLS.md

## Purpose

Specify developer tools for inspecting the simulation.

The world must be observable internally.

Without visualization, complex procedural systems will become extremely difficult to debug.

## Required contents

### 1. Planet Viewer

Show the entire planet with selectable data layers.

### 2. Geological Viewer

Visualize:

- Geological provinces.
- Rock units.
- Faults.
- Folds.
- Intrusions.
- Sedimentary basins.
- Geological age.

### 3. Tectonic Viewer

Show plates, boundaries, movement, and tectonic history.

### 4. Mineral Viewer

Display mineral distribution and composition.

### 5. Ore Deposit Viewer

Display deposit boundaries, grade, depth, geometry, and uncertainty.

### 6. Geochemical Viewer

Show elemental concentrations and anomalies.

### 7. Geophysical Viewer

Show gravity, magnetic, seismic, thermal, and resistivity layers.

### 8. Climate Viewer

Display temperature, precipitation, pressure, wind, and climate zones.

### 9. Hydrology Viewer

Display watersheds, rivers, drainage networks, lakes, and groundwater.

### 10. Soil and Ecology Viewer

Show soil classes, vegetation, biomes, and ecosystem properties.

### 11. 3D Subsurface Viewer

Inspect geological layers and structures beneath the surface.

### 12. Process Inspector

Inspect:

- Active processes.
- Inputs.
- Outputs.
- Conditions.
- Progress.
- Failures.
- History.

### 13. Industrial Inspector

Inspect production chains, throughput, efficiency, material balance, utilities, and waste.

### 14. Data Inspector

Browse loaded content definitions and their validation state.

### 15. Simulation Profiler

Measure system update time, memory, event volume, and generation costs.

### 16. Seed Reproduction

Allow developers to reproduce a specific generation or simulation state using saved seeds and configuration.

### 17. Visual Comparison

Allow comparison of two generation configurations or simulation snapshots.

---

# 28_IMPLEMENTATION_ROADMAP.md

## Purpose

Translate the enormous vision into an achievable sequence of engineering work.

This document is not allowed to pretend that all systems can be developed simultaneously.

## Required contents

### Phase 0 — Repository Audit

Inspect the existing codebase.

Document:

- Current architecture.
- Existing systems.
- Current simulation loop.
- World representation.
- Chunk handling.
- Terrain generation.
- Pathfinding.
- Work system.
- Rendering.
- Input.
- UI.
- Build configuration.
- Existing tests.

Identify what can be extended and what needs redesign.

### Phase 1 — Simulation Foundation

Implement:

- Deterministic random generation.
- Stable identifiers.
- Simulation time.
- Fixed timestep.
- Event infrastructure.
- Data loading.
- Basic serialization.
- Debug logging.

### Phase 2 — Planetary Geometry

Implement:

- Planet configuration.
- Spherical coordinate system.
- Global 2D map.
- Region indexing.
- Coordinate conversions.
- Basic planetary viewer.

### Phase 3 — Geological Background

Implement a simplified model of:

- Crust.
- Mantle.
- Core.
- Continental regions.
- Oceanic regions.
- Tectonic plates.
- Geological provinces.

### Phase 4 — Geological History

Implement:

- Geological time.
- Event records.
- Basic tectonic evolution.
- Major rock formation.
- Stratigraphic relationships.

### Phase 5 — Terrain and Hydrology

Implement:

- Elevation.
- Bathymetry.
- Terrain classification.
- Watersheds.
- Rivers.
- Lakes.
- Basic erosion.

### Phase 6 — Climate and Ecology

Implement:

- Temperature.
- Precipitation.
- Climate zones.
- Soil properties.
- Biome generation.
- Basic ecological simulation.

### Phase 7 — Chemistry and Mineralogy

Implement:

- Element registry.
- Compound registry.
- Mineral registry.
- Rock composition.
- Chemical formulas.
- Basic reaction framework.
- Material balance validation.

### Phase 8 — Mineral Deposits

Implement several representative deposit families before expanding the complete deposit catalog.

### Phase 9 — Exploration

Implement:

- Geological observations.
- Sampling.
- Resource maps.
- Uncertainty.
- Discovery progression.

### Phase 10 — Local 3D World Integration

Connect planetary geology and terrain to the existing local 3D world.

### Phase 11 — Mining

Implement extraction, mine geometry, transport, and resource depletion.

### Phase 12 — Processing and Metallurgy

Implement crushing, separation, smelting, refining, and material production.

### Phase 13 — Industrial Chemistry

Implement chemical production chains and industrial utilities.

### Phase 14 — Materials Science

Introduce advanced material properties and process-dependent material quality.

### Phase 15 — Advanced Planetary Science

Expand geophysics, oceanology, geological history, detailed climate, and advanced geochemistry.

### Phase 16 — Biological History and Advanced Ecology

Expand ecosystems, species, evolutionary history, and paleontology.

### Phase 17 — Society and Industry Integration

Connect the natural world to:

- Population.
- Labor.
- Logistics.
- Economy.
- Politics.
- Research.
- Infrastructure.

### Phase 18 — Advanced Simulation

Introduce more detailed solvers selectively, based on measured gameplay and performance requirements.

## Required Phase Format

Every implementation phase must contain:

- Objectives.
- Dependencies.
- Prerequisites.
- Tasks.
- Expected files.
- Data requirements.
- Acceptance criteria.
- Tests.
- Performance budget.
- Known limitations.
- Completion conditions.

---

# PART V — CROSS-SYSTEM SPECIAL SPECIFICATIONS

The 29 documents above describe the primary systems. The following concepts must also be covered across those documents.

They are not necessarily separate files yet, but they must not be forgotten.

# 29. THE GEOLOGICAL TIME MACHINE

## Concept

The world should retain enough geological history to explain its present state.

A geological viewer should be able to reconstruct selected historical snapshots.

Examples:

- A continent before collision.
- An ancient ocean basin.
- A volcanic arc before erosion.
- A sedimentary basin during deposition.
- An ore deposit during mineralization.
- A mountain belt before extensive erosion.

## Required architecture

- Geological event history.
- Time-indexed geological states.
- Historical tectonic configuration.
- Stratigraphic reconstruction.
- Historical terrain approximation.
- Event dependency graph.
- Historical visualization.

## Important limitation

Do not store a complete copy of the entire planet for every geological timestep.

Use event histories, checkpoints, compressed state representations, and selective reconstruction.

---

# 30. THE PLANETARY ENCYCLOPEDIA

## Concept

The game should contain an internally generated scientific encyclopedia.

It describes the actual generated world rather than a disconnected collection of generic descriptions.

## Required entries

- Planet.
- Continents.
- Oceans.
- Regions.
- Mountains.
- Rivers.
- Lakes.
- Geological provinces.
- Rock formations.
- Minerals.
- Deposits.
- Fossils.
- Biomes.
- Ecosystems.
- Industrial materials.
- Chemical compounds.

## Each entry should include

- Identifier.
- Name.
- Classification.
- Description.
- Location.
- Relevant properties.
- Origin.
- Related entities.
- Discovery state.
- Confidence.
- Historical events.

## Example

A discovered copper deposit could have an encyclopedia entry explaining its geological setting, associated minerals, host rocks, estimated grade, formation history, and exploration confidence.

The encyclopedia must derive information from the simulation database.

It must not invent facts that contradict the generated world.

---

# 31. THE SCIENTIFIC UNCERTAINTY SYSTEM

## Concept

The simulation internally knows more than the player.

However, even the simulation may use approximations rather than exact scientific knowledge.

These distinctions must be explicit.

## Required categories

- Known ground truth.
- Simulated estimate.
- Observed measurement.
- Inferred interpretation.
- Probabilistic prediction.
- Unknown information.

## Example

A mineral deposit might internally have a known generated geometry.

The player may only have:

- Two drill samples.
- A geochemical anomaly.
- An uncertain geophysical interpretation.

The player therefore sees an estimated deposit rather than its exact generated shape.

## Required functionality

- Measurement uncertainty.
- Sampling coverage.
- Confidence intervals where appropriate.
- Competing interpretations.
- Discovery history.
- Research-based uncertainty reduction.

---

# PART VI — SIMULATION DETAIL MODES

# 32. MULTI-RESOLUTION SIMULATION

The simulation must never attempt to calculate every physical process everywhere at maximum resolution.

Use three principal modes.

## 32.1 Geological Background Mode

Purpose: generate and maintain the broad planetary state.

Characteristics:

- Large spatial cells.
- Long time intervals.
- Statistical and procedural models.
- Precomputed geological history.
- Simplified climate.
- Simplified hydrology.
- Aggregated resource information.

Used for the entire planet.

## 32.2 Active Simulation Mode

Purpose: simulate regions relevant to exploration, settlements, and active gameplay.

Characteristics:

- Higher spatial resolution.
- More detailed geological structures.
- Better hydrology.
- Local environmental changes.
- Detailed resource estimates.
- More frequent updates.

Used for selected regions.

## 32.3 Industrial Simulation Mode

Purpose: model important active industrial sites.

Characteristics:

- Explicit material flows.
- Process networks.
- Chemical reactions where relevant.
- Energy accounting.
- Equipment constraints.
- Production losses.
- Waste streams.
- Quality control.

Used for active factories, mines, refineries, and other important facilities.

## 32.4 Mode Transitions

Every subsystem must specify:

- What data is preserved.
- What data is aggregated.
- What information is lost.
- How state is reconstructed.
- Whether results remain deterministic.
- How numerical inconsistencies are handled.

---

# PART VII — TECHNICAL INTEGRATION REQUIREMENTS

# 33. CONNECTION TO THE EXISTING CODEBASE

The documentation describes a future architecture, not permission to replace the existing project wholesale.

The implementation agent must first inspect the current repository.

The existing C++20/raylib/CMake foundation should be extended where practical.

Potential future technologies may include:

- EnTT for entity-component-system functionality.
- JSON libraries for structured content.
- SQLite for selected persistent databases.
- Dear ImGui for developer tools.
- Additional numerical libraries if justified.

These are options, not mandatory dependencies.

## 33.1 Architectural Separation

Maintain clear boundaries between:

- Simulation core.
- Planet model.
- World generation.
- Geological systems.
- Chemical systems.
- Industrial systems.
- Population and society.
- Rendering.
- UI.
- Input.
- Persistence.
- Debugging.

Rendering must not become the source of truth for simulation state.

The simulation should be capable of running without graphical rendering.

## 33.2 Headless Simulation

A headless mode should eventually support:

- World generation.
- Simulation stepping.
- Automated testing.
- Statistical validation.
- Performance profiling.
- Save/load testing.

## 33.3 Fixed Timestep

Maintain deterministic simulation scheduling where feasible.

Long-term geological processes should not necessarily execute every ordinary simulation tick.

Use appropriate update frequencies and accumulated process models.

## 33.4 Data Ownership

Every system must clearly specify which subsystem owns each piece of state.

Avoid multiple systems independently storing conflicting copies of the same authoritative value.

## 33.5 Public Interfaces

Every subsystem must document:

- Public data types.
- Public operations.
- Events emitted.
- Events consumed.
- Dependencies.
- Error handling.
- Serialization responsibilities.

---

# PART VIII — UNIVERSAL REQUIREMENTS FOR EVERY SYSTEM

# 34. MANDATORY SYSTEM DOCUMENT TEMPLATE

Every subsystem document must answer the following questions.

## A. Identity

1. What does the system simulate?
2. Why does it exist?
3. What is explicitly outside its scope?
4. What other systems depend on it?

## B. Scientific Foundation

1. What real-world phenomenon is represented?
2. What scientific principles are relevant?
3. Which parts are simplified?
4. What assumptions are made?
5. What is the intended accuracy?

## C. Simulation Model

1. What state variables exist?
2. What are the units?
3. What equations or algorithms are used?
4. What are the inputs?
5. What are the outputs?
6. What processes transform the state?
7. How is time represented?
8. How is space represented?

## D. Data Model

1. What entities exist?
2. What are their identifiers?
3. What properties do they contain?
4. How are they serialized?
5. How are they versioned?
6. What validation rules apply?

## E. Integration

1. Which systems provide inputs?
2. Which systems consume outputs?
3. Which events are emitted?
4. Which events are consumed?
5. What are the update dependencies?

## F. Performance

1. What is the expected spatial resolution?
2. What is the update frequency?
3. What is the memory cost?
4. Can calculations be cached?
5. Can calculations be deferred?
6. Can the system run at a lower detail level?

## G. Validation

1. What invariants must hold?
2. What tests are required?
3. What reference scenarios exist?
4. What numerical failure modes are possible?
5. How is determinism checked?

## H. Debugging

1. What must be visualized?
2. What data must be inspectable?
3. What statistics are useful?
4. How can a failure be reproduced?

## I. Implementation

1. What is the smallest working implementation?
2. What is the next level of sophistication?
3. What dependencies must be completed first?
4. What are the acceptance criteria?
5. What is explicitly postponed?

---

# PART IX — IMPLEMENTATION RULES FOR FUTURE AI CODING AGENTS

# 35. IMPORTANT INSTRUCTIONS

Any AI model working on Infinitesia must follow these rules.

## 35.1 Read Before Writing

Before implementing a system:

1. Read this master blueprint.
2. Read the relevant subsystem documents.
3. Inspect the actual repository.
4. Identify existing functionality.
5. Identify architectural constraints.
6. Prepare an implementation plan.

Do not assume that documentation perfectly describes the current code.

The repository is the source of truth for current implementation state.

## 35.2 Do Not Implement the Entire Vision

The documents intentionally describe a long-term and extremely ambitious project.

They are not a request to implement every feature immediately.

Always identify:

- Current development phase.
- Required prerequisites.
- Minimum viable implementation.
- Tests.
- Integration requirements.

## 35.3 Do Not Build Fake Scientific Complexity

Avoid systems that contain hundreds of scientific variables but do not use them coherently.

A smaller model with correct relationships is preferable to a huge model full of meaningless parameters.

## 35.4 Do Not Hardcode the Entire World

Use data-driven definitions where appropriate.

Avoid giant switch statements containing every mineral, rock, element, reaction, and machine.

## 35.5 Preserve Existing Functionality

Before changing existing code:

- Inspect call sites.
- Understand ownership.
- Identify dependent systems.
- Avoid unnecessary rewrites.
- Maintain build compatibility.

## 35.6 Write Tests Alongside Systems

Every significant system must include validation.

Do not postpone all testing until the project is large.

## 35.7 Keep Documentation Synchronized

When an implementation changes:

- Update its subsystem document.
- Update schemas.
- Update dependencies.
- Update limitations.
- Update the roadmap.

## 35.8 Report Actual Completion

An AI agent must distinguish:

- Implemented.
- Partially implemented.
- Stubbed.
- Planned.
- Documented but not implemented.
- Tested.
- Not tested.

Never claim a subsystem is complete merely because its classes compile.

---

# PART X — FIRST TASK FOR A NEW AI MODEL

# 36. INITIAL REPOSITORY AUDIT AND ARCHITECTURE PLAN

The first task is not to implement the entire planetary simulation.

The first task is to understand the existing project and establish a realistic foundation.

## Step 1 — Inspect the Repository

Inspect all existing source files and build configuration.

Pay special attention to:

- Application architecture.
- Simulation loop.
- World model.
- Chunk representation.
- Terrain generation.
- Work system.
- Pathfinding.
- Rendering.
- UI.
- Input.
- Existing data structures.
- Existing tests.

## Step 2 — Produce an Architecture Report

Document:

- What already exists.
- What works.
- What is incomplete.
- What is missing.
- What should be preserved.
- What should be extended.
- What should eventually be refactored.

## Step 3 — Map Existing Code to the New Architecture

Identify where existing modules fit into the long-term design.

Do not force every new system into the current folder structure prematurely.

## Step 4 — Identify Foundational Risks

Evaluate:

- Coordinate representation.
- Determinism.
- Simulation scheduling.
- Memory ownership.
- Serialization.
- Data loading.
- Chunk boundaries.
- Global versus local world state.

## Step 5 — Propose the First Implementation Milestone

The first milestone should establish a minimal planetary foundation.

Suggested scope:

- Planet configuration.
- Deterministic generation seed.
- Spherical coordinate utilities.
- Global 2D map representation.
- Basic region partitioning.
- Planet data model.
- Minimal debug visualization.
- Unit tests.

## Step 6 — Do Not Continue Without a Clear Architecture

Before large-scale implementation, present:

- Architecture report.
- Proposed module structure.
- Required data types.
- Public interfaces.
- Dependency graph.
- Implementation plan.
- Acceptance criteria.

The goal is to establish a foundation that can support future geological, chemical, and industrial complexity without requiring a complete rewrite.

---

# PART XI — FINAL DESIGN PHILOSOPHY

# 37. THE INFINTESIA PRINCIPLE

Infinitesia should aim to make its world understandable through causality.

The player should not merely encounter a resource.

The player should encounter the geological history that produced it.

The player should not merely construct a factory.

The player should construct a chain of processes that transforms materials, consumes energy, requires infrastructure, produces waste, and interacts with the surrounding economy.

The player should not merely occupy a settlement.

The player should participate in the development of a society with labor, institutions, social relationships, political conflicts, and material constraints.

The planet should not be a decorative background.

It should be the foundation of the entire simulation.

## 37.1 The Ultimate Goal

A world where:

- Geography has a history.
- Geology has causes.
- Resources have origins.
- Materials have properties.
- Chemistry has consequences.
- Industry has physical constraints.
- Ecology responds to environmental change.
- Infrastructure changes the landscape.
- Settlements depend on their environment.
- Economies emerge from production and scarcity.
- Political systems emerge from social and material conditions.
- Player actions become part of the world's history.

## 37.2 The Most Important Engineering Constraint

**The ambition of the simulation must never exceed the architecture's ability to manage complexity.**

Every subsystem should be independently understandable, testable, replaceable, and scalable.

The project should begin with a small number of coherent systems and gradually connect them into a much larger simulation.

The final result may be extraordinarily complex.

The codebase must not become extraordinarily incomprehensible.

---

# END OF MASTER DOCUMENTATION BLUEPRINT

**Recommended next action:** Create the `docs/` directory and populate the 29 documents with this structure. Begin with `00_MASTER_VISION.md`, `02_SPHERICAL_WORLD_MODEL.md`, `24_UNIVERSAL_PROCESS_FRAMEWORK.md`, `25_DATA_SCHEMAS_AND_CONTENT_FORMAT.md`, and `28_IMPLEMENTATION_ROADMAP.md`.

Do not implement all 29 systems at once. Establish the shared architecture first, then develop each system in dependency order.