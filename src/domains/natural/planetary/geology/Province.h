#pragma once

#include <cstdint>
#include <string_view>

namespace Infinitesia
{
// Index of a Province in PlanetMap::GetProvinces() plus one. Stable for one
// planet and geology data version: ids follow a row-major scan of the grid.
using ProvinceId = std::uint32_t;
inline constexpr ProvinceId kNoProvince = 0;

// Geological province classes, in classification priority order
// (docs/03_GEOLOGY.md, section 6.2).
enum class ProvinceType : std::uint8_t
{
	OceanicRidge,         // young oceanic crust around spreading centres
	AbyssalPlain,         // older oceanic crust
	Trench,               // trench and accretionary prism on the down-going plate
	IslandArc,            // volcanic arc on oceanic crust
	ContinentalArc,       // volcanic arc on a continental margin
	CollisionOrogen,      // active continent-continent collision belt
	AncientOrogen,        // eroded fold-and-thrust belt far from active boundaries
	RiftSystem,           // continental rift
	LargeIgneousProvince, // mantle-plume flood volcanism and its intrusions
	Craton,               // old stable shield with a thin cover
	Platform,             // cratonic basement under a sedimentary cover
	SedimentaryBasin,     // thick sediment fill
	PassiveMargin,        // continental margin and shelf away from plate boundaries
	MetamorphicComplex,   // exposed deep crust outside active belts
};
inline constexpr int kProvinceTypeCount = 14;

// Stable lowercase identifier, e.g. "craton". Used in data files and logs.
std::string_view ProvinceTypeName(ProvinceType type) noexcept;

// How deep the present surface cuts below the original top of the province's
// main-event rocks.
enum class ErosionLevel : std::uint8_t
{
	Surface,      // < 1 km: volcanic and sedimentary cover intact
	Shallow,      // 1-5 km: upper intrusions and porphyry level exposed
	Intermediate, // 5-15 km: metamorphic complexes and plutons
	Deep,         // > 15 km: high-grade basement
};

ErosionLevel ClassifyErosionDepth(double depthKm) noexcept;
std::string_view ErosionLevelName(ErosionLevel level) noexcept;

// One connected region of one province type. The ages and the erosion level are
// province-wide estimates; later stages refine them per place.
struct Province
{
	ProvinceId id = kNoProvince;
	ProvinceType type = ProvinceType::AbyssalPlain;
	int cellCount = 0;
	double areaKm2 = 0.0;
	double centroidLatitudeDegrees = 0.0;
	double centroidLongitudeDegrees = 0.0;
	double landFraction = 0.0; // share of the area above sea level
	double meanCrustAgeMyr = 0.0;
	double meanSedimentThicknessKm = 0.0;
	// Age of the main geological event (onset of arc magmatism, collision,
	// rifting, plume eruption, cover deposition, ...), million years before the
	// present.
	double eventAgeMyr = 0.0;
	double erosionDepthKm = 0.0;
	ErosionLevel erosionLevel = ErosionLevel::Surface;
	// Axis of the plate motion across the province, degrees clockwise from north
	// in [0, 180): the direction rock layers are compressed along, so folds run
	// across it.
	double compressionAzimuthDegrees = 0.0;
};
} // namespace Infinitesia
