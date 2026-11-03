# Planet Generation Model — Version 6

## Scope and scientific status

Version 6 generates a deterministic, scientifically motivated *snapshot* of one
rocky planet. It is an interactive-game model, not an N-body accretion code,
mantle-convection solver, climate model, or geological time integrator. Values
labelled as calibrated or heuristic below must not be interpreted as physical
predictions.

The default surface grid is 360 latitude bands by 720 longitude segments
(0.5 degrees, about 55 km per cell). It is
an equirectangular grid, not an equal-area mesh. Area-weighted operations use
the spherical cell-area factor proportional to `cos(latitude)`.

## System formation and orbital state

All quantities use SI units internally unless explicitly stated.

### Star and disk

For the narrow near-solar mass range used by the current preset:

- Main-sequence lifetime:
  `t_MS = 10 Gyr * (M_star / M_sun)^(-2.5)`.
- Luminosity approximation:
  `L_star / L_sun = (M_star / M_sun)^3.5 * (1 + 0.4 * age / t_MS)`.
- Disk mass:
  `M_disk = f_disk * M_star`, with `f_disk` sampled from 0.005 to 0.02.
- Solid inventory:
  `M_solid = 0.01 * M_disk * 10^[Fe/H]`.

The luminosity power law is a local approximation, not a stellar-evolution
track. A one-percent solar-nebula dust-to-gas ratio and the disk/solid fractions
are model priors; the generated system does not solve disk transport,
fragmentation, or planetesimal dynamics.

### Snow line and orbit

The adopted passive-disk temperature profile is:

`T(r) = 280 K * (L_star / L_sun)^(1/4) * (r / AU)^(-1/2)`.

For the model's water-condensation temperature `T_cond = 170 K`, this gives:

`r_snow = AU * (T(1 AU) / T_cond)^2`.

The target orbit is sampled by mean incident flux, then its semimajor axis is
derived using the orbit-averaged flux:

`<S> / S_earth = (L_star / L_sun) / ((a / AU)^2 * sqrt(1 - e^2))`.

Orbital period follows the two-body form of Kepler's third law:

`P = 2*pi*sqrt(a^3 / (G * (M_star + M_planet)))`.

The implementation neglects planet mass in this period calculation, which is
an excellent approximation for the generated planet-to-star mass ratio.

### Planet mass and bulk properties

The default rocky-temperate scenario targets near-Earth masses and caps the
planet at `1.25 M_earth` (custom scenarios may raise the cap). The current
one-planet budget samples:

`M_planet = min(f_planet * M_solid, 1.25 M_earth)`,

where `f_planet` is sampled from 0.10 to 0.22. The default stellar, disk, and
metallicity priors keep this near roughly 0.8–1.25 Earth masses; it remains a
procedural budget, not a solution of core accretion or migration. The
scenario's explicit maximum mass remains available for non-default worlds.

Bulk density is currently calibrated from the sampled iron mass fraction:

`rho_bulk = 4400 + 3500 * X_Fe` kg/m^3.

Radius and surface gravity then obey:

`R = cubert(M_planet / ((4/3)*pi*rho_bulk))`,
`g = G*M_planet/R^2`.

The density relation is a simple interpolation, not an equation of state or a
layered iron-silicate interior calculation. Combined with the default mass and
iron-fraction ranges, it yields surface gravity close to Earth's (roughly
0.9–1.1 `g_earth`), rather than the multi-g regime possible under the former
`3 M_earth` default cap.

Atmospheric pressure is not inferred linearly from total volatile mass. The
current Earth-like preset uses a reference pressure near `101.325 kPa`,
modified by a bounded retention factor based on gravity, stellar radiation,
and volatile availability:

`p = 101.325 kPa * clamp(0.95 + 0.1*(sqrt(g/g_earth)-1) - 0.08*R, 0.8, 1.05) * clamp(sqrt(X_vol/0.015), 1, 1.15)`,

where `R` is the normalized radiation-environment value. This is a coarse
degassing/escape proxy, not an atmospheric escape calculation.

## Surface and geological snapshot

### Continental crust versus tectonic plates

Continental crust is generated from a continuous 3D noise field evaluated on
unit-sphere coordinates. A quantile weighted by `cos(latitude)` selects a
40-percent continental-crust area fraction. This target is an Earth-calibrated
procedural prior; it is deliberately not inferred from iron abundance or water
inventory. It prevents continent outlines from being identical to Voronoi plate
boundaries, but it does not simulate continent assembly or breakup.

The binary continental/oceanic classification is retained for geology, while
crustal thickness and elevation blend smoothly across a field-value margin.
This represents broad shelves and transitional crust instead of a one-cell
step at every continent edge. Convergent margins create broad continental
uplift belts or oceanic trenches; divergent margins create subdued ridges or
rifts. Their broad influence is `w = smootherstep(clamp(1 - delta / 4.5, 0, 1))`,
where `delta` is the difference between the nearest and second-nearest plate
center distances in degrees. The calibrated relief is proportional to `w^2`
(up to +4.5 km for continental convergence, -3.2 km for oceanic convergence,
and smaller divergent/transform relief). The signed relief field and resulting
terrain are spatially smoothed across several cells so changing boundary
subtypes cannot create one-cell cliffs. Boundary geology can still be sharp
in the data, but its rendered surface expression is a mountain/trench belt.
These are procedural relief fields, not time-evolved mountain belts.

Plate centers are distributed approximately uniformly using a Fibonacci
spiral. For each plate, an Euler pole and angular velocity define the local
kinematic velocity:

`v = omega x r`.

The reference angular-rate interval is 0.09–0.9 degrees per million years,
equivalent to roughly 1–10 cm/year at Earth's radius before the model's
mass/age `heatScale` multiplier. Motion is classified by the
normal/tangential components of relative velocity. This is a kinematic
procedural construction: plate velocities are independently sampled and do not
obey global force or torque balance.

### Oceanic crust age and bathymetry

Oceanic spreading cells seed a multi-source shortest-path calculation on the
spherical grid. This replaces the previous all-pairs search. The resulting
grid distance is normalized over 80 angular degrees:

`age_norm = clamp(distance_from_ridge / 80 degrees, 0, 1)`.

The bathymetry approximation is:

`depth_km = -2.6 - 0.35 * sqrt(age_Myr)`,
`age_Myr = 180 * age_norm`.

The square-root dependence is motivated by half-space conductive cooling of
young oceanic lithosphere. The coefficient is an empirical Earth calibration;
the generated age field is not an integrated spreading history, and capped
ages do not represent the oldest oceanic crust on every world. If the grid has
no oceanic spreading seeds, a plate-center distance fallback is used instead.

### Isostasy and elevation

The Airy-style crustal elevation relative to a reference level is:

`h_km = T_crust * (rho_mantle - rho_crust) / rho_mantle - h_0`.

The current calibration uses `rho_mantle = 3300 kg/m^3`,
`rho_oceanic = 2900 kg/m^3`, `rho_continental = 2750 kg/m^3`, and
`h_0 = 4.35 km`. This places nominal 7 km oceanic crust near -3.5 km and
35 km continental crust near +1.5 km. It omits flexure, dynamic topography,
sediment loading, and realistic lithospheric strength.

Terrain detail, erosion, trenches, and foreland basins are game-scale
approximations. Multi-scale spherical noise adds subdued local roughness;
ridged detail is amplified only where broad positive tectonic relief indicates
mountain belts. A smoothed positive-relief neighborhood produces low foreland
basins, while old granitic shields and sedimentary basins receive subtle broad
elevation offsets. Spherical 3D noise avoids a longitude seam and latitude /
longitude noise distortion at the poles. The simulation runs on the
360 x 720 default grid (0.5 degrees, about 55 km per cell). Bilinear texture filtering smooths the
map display. A lake covers only `lakeFraction` of its cell, and the viewer
tints the cell toward water by that fraction. The globe renderer
exaggerates radial relief eightfold for visibility and uses elevation tint and
hill shading; the flat map uses the same generated heights.

## Geological context (geology data version 1)

These fields are added to each cell after the terrain stage and feed no earlier
field; the pre-existing planet is unchanged.

- Boundary context: the distance to the nearest plate boundary is half the gap
  between the two nearest plate centres (great-circle degrees) times the arc length
  of one degree; beyond 1000 km the boundary kind is `None`. Plate velocity is the
  Euler-pole velocity converted at 11.1 cm/yr per deg/Myr (Earth radius).
- Crust age: oceanic `crustAge * 180 Myr`; continental ages are assigned by the rank
  of the coherent age field so that 28% of the continental area is older than two
  thirds of the planet's age (gameplay calibration).
- Heat flow, oceanic: `q = 473 / sqrt(t_Myr)` mW/m2, bounded to 48-300 (half-space
  cooling with the old-plate floor). Continental: global means by crust age, 41
  (Archean) to 66 (Meso-Cenozoic) mW/m2, plus magmatic belts and plumes, all scaled
  by the same mass/age heat budget as the plate count.
- Sediment cover: gameplay-calibrated, 0.02-14 km (abyssal pelagic rain growing
  with age, terrigenous fans fading over ~400 km, trench fill, continental lowland
  cover, foreland basins, rifts, passive-margin wedges).
- Provinces: smooth-window scores per type, majority filter, connected components,
  small components merged into their longest-bordering neighbour.

References: Parsons & Sclater (1977); Stein & Stein (1992); Pollack, Hurter &
Johnson (1993), "Heat flow from the Earth's interior".

## Climate

The global effective temperature starts from:

`T_eff = 255 K * (<S>/S_earth)^(1/4)`.

The greenhouse increment is an Earth-calibrated pressure proxy:

`DeltaT_G = 33 K * sqrt(p / 101.325 kPa)`.

At 101.325 kPa this yields 33 K. Bulk volatile inventory is not treated as
greenhouse-gas abundance because the model does not resolve atmospheric
composition. The square-root pressure relation is a tunable approximation, not
radiative transfer.

The zonal temperature profile preserves the global radiative baseline and uses
an area-weighted annual insolation anomaly:

`T_surface(lat) = T_eff + DeltaT_G + 55 K * (I(lat) - mean_area(I)) - lapse_rate`.

The 55 K redistribution coefficient is a gameplay calibration. Ocean thermal
inertia affects seasonal range rather than lowering the annual-mean surface
temperature. Elevation cooling uses the Earth environmental reference lapse
rate, `6.5 K/km`; it is not scaled by gravity because atmospheric composition
and heat capacity are not modeled well enough to justify extrapolating that
Earth-calibrated environmental rate. Circulation zones and precipitation are
simplified.
The year is also sampled at 12 month midpoints. Each month's solar declination
is used to calculate daily-mean insolation, normalized against that month's
area-weighted global mean. The local monthly anomaly is

`DeltaT_month = clamp(35 K * (I_month - I_annual) * thermal_damping, -18 K, 18 K)`.

The damping proxy is 0.28 over ocean and `0.48 + 0.45 * continentality` over
land, approximating ocean thermal inertia and stronger seasonal extremes
farther inland. Annual mean temperature remains the average of the monthly
temperatures. This is a seasonal energy-balance approximation, not a transient
heat-capacity model.

Latitude insolation is computed from the daily-mean solar zenith integral,
averaged at 64 seasonal longitudes and normalized by the spherical mean flux.
This restores the equatorial-to-polar contrast; the former heuristic did not
reproduce the annual-mean insolation profile.
For latitude `phi`, obliquity `epsilon`, seasonal longitude `lambda`, solar
declination is `delta = asin(sin(epsilon)*sin(lambda))`; the sunset hour angle
is `H0 = acos(-tan(phi)*tan(delta))`, clipped to polar night/day. The
daily-mean integral is
`Q = H0*sin(phi)*sin(delta) + cos(phi)*cos(delta)*sin(H0)`, and normalized
insolation is `I = 4*mean(Q)/pi`.

Precipitation is computed monthly from a circulation/moisture proxy and a
Clausius-Clapeyron-like temperature factor:

`P_annual = 3200 mm * RH_proxy * sqrt(p / 101.325 kPa) * exp(0.055 * (T_annual - 15 C))`

`P_month = P_annual * W_month * (sat(T_month) / mean_month(sat(T))) * O_month / 12`,

where `W_month` is the seasonal circulation weight normalized to an annual
mean of one, `sat(T) = clamp(exp(0.055 * (T - 15 C)), 0.15, 2.5)`, and `O_month`
is a bounded orographic moisture factor. The ITCZ wet belt follows the
subsolar latitude; subtropical subsidence and mid-latitude storm activity also
shift seasonally. The number of dry-season months is counted from monthly
precipitation, rather than inferred from one annual wetness estimate.

Near-surface winds use simplified rotating circulation belts: easterly trades,
mid-latitude westerlies, and polar easterlies. Seasonal shifts follow the
subsolar latitude, and rotation period shifts the belt widths. For land cells,
the model samples upwind ocean fetch over a bounded 1800 km path, increases
precipitation on rising terrain, and reduces it in the lee of upstream high
terrain. This creates a first-order rain-shadow effect without solving
three-dimensional atmospheric flow. The coefficients are gameplay
calibrations, not a local weather forecast.

## Performance and determinism

The expensive ocean-crust distance search is now a multi-source Dijkstra pass:
`O(N log N)` in grid cells instead of comparing every ocean cell with every
candidate ridge cell. The 360 x 720 default map is checked against a 2-second
generation budget by `PlanetGenerationTests`. Repeated generation with the
same parameters and seed is tested for deterministic geological output.

## References

1. Murray, C. D. & Dermott, S. F. *Solar System Dynamics*. Cambridge
   University Press, 2000. Keplerian two-body dynamics.
   [doi:10.1017/CBO9781139174817](https://doi.org/10.1017/CBO9781139174817)
2. Williams, D. M. & Pollard, D. “Earth-like worlds on eccentric orbits:
   excursions beyond the habitable zone.” *International Journal of
   Astrobiology* 1(1), 61–69, 2002. Orbit-averaged flux for eccentric orbits.
   [doi:10.1017/S1473550402001064](https://doi.org/10.1017/S1473550402001064)
3. Hayashi, C. “Structure of the Solar Nebula, Growth and Decay of Magnetic
   Fields and Effects of Magnetic and Turbulent Viscosities on the Nebula.”
   *Progress of Theoretical Physics Supplement* 70, 35–53, 1981. Passive nebula
   temperature profile and condensation front.
   [doi:10.1143/PTPS.70.35](https://doi.org/10.1143/PTPS.70.35)
4. Kious, W. J. & Tilling, R. I. *This Dynamic Earth: The Story of Plate
   Tectonics*. U.S. Geological Survey, 1996. Observed plate-motion rates.
   [USGS](https://pubs.usgs.gov/gip/dynamic/understanding.html)
5. Parsons, B. & Sclater, J. G. “An analysis of the variation of ocean floor
   bathymetry and heat flow with age.” *Journal of Geophysical Research* 82(5),
   803–827, 1977. Ocean-floor cooling and square-root-of-age bathymetry.
   [doi:10.1029/JB082i005p00803](https://doi.org/10.1029/JB082i005p00803)
6. Turcotte, D. L. & Schubert, G. *Geodynamics*, 3rd ed. Cambridge University
   Press, 2014. Isostatic compensation and the Airy model.
   [doi:10.1017/CBO9780511843877](https://doi.org/10.1017/CBO9780511843877)
