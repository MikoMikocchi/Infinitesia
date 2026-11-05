#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "domains/natural/planetary/PlanetMap.h"

// Golden test: pins the exact output of planet generation.
//
// Every field of every PlanetRegion, every Province, the PlanetWaterBalance and
// the number of geological epochs are folded into one 64-bit FNV-1a hash per
// planet. Any change to a generated value, however small, changes the hash.
//
// When the generation result changes ON PURPOSE: recompute the hashes (run this
// test, copy the "actual" values into the constants below) and raise
// PlanetStage::version of every stage whose output changed. An unintended hash
// change is a regression.

namespace
{
using namespace Infinitesia;

class Hasher
{
public:
	void Bytes(const void *data, std::size_t size)
	{
		const auto *bytes = static_cast<const unsigned char *>(data);
		for (std::size_t i = 0; i < size; ++i)
		{
			m_hash ^= bytes[i];
			m_hash *= 0x100000001b3ULL;
		}
	}
	// Reals are hashed rounded to 7 significant digits: the last bits of a double
	// depend on the compiler, optimisation level and FMA contraction, so a
	// bit-exact hash would only match on the machine that produced it.
	void Real(double value)
	{
		char text[32];
		const int length = std::snprintf(text, sizeof text, "%.6e", value);
		Bytes(text, static_cast<std::size_t>(length));
	}
	// Every integral, bool or enum value is hashed as 8 bytes.
	template <typename T> void Int(T value)
	{
		const auto wide = static_cast<std::uint64_t>(static_cast<std::int64_t>(value));
		Bytes(&wide, sizeof wide);
	}
	std::uint64_t Value() const
	{
		return m_hash;
	}

private:
	std::uint64_t m_hash = 0xcbf29ce484222325ULL;
};

void Add(Hasher &h, const PlanetRegion &r)
{
	h.Int(r.latitudeIndex);
	h.Int(r.longitudeIndex);
	h.Int(r.seed);
	h.Real(r.latitudeDegrees);
	h.Real(r.longitudeDegrees);
	h.Int(r.tectonicPlateId);
	h.Real(r.tectonicStress);
	h.Int(r.isVolcanicZone);
	h.Real(r.tectonicReliefKm);
	h.Real(r.continentalCrustFraction);
	h.Real(r.crustThickness);
	h.Real(r.isostaticCrustThicknessKm);
	h.Real(r.crustAge);
	h.Int(r.surfaceRock);
	h.Int(r.boundarySubtype);
	h.Real(r.distanceToBoundaryKm);
	h.Int(r.isSubductingPlate);
	h.Real(r.plateVelocityEastCmPerYear);
	h.Real(r.plateVelocityNorthCmPerYear);
	h.Real(r.boundaryRelativeVelocityEastCmPerYear);
	h.Real(r.boundaryRelativeVelocityNorthCmPerYear);
	h.Real(r.crustAgeMyr);
	h.Real(r.sedimentThicknessKm);
	h.Real(r.heatFlowMwPerM2);
	h.Real(r.hotspotInfluence);
	h.Real(r.paleoLatitudeDegrees);
	h.Int(r.provinceId);
	h.Real(r.elevationNormalized);
	h.Real(r.slopeNormalized);
	h.Int(r.isOcean);
	h.Int(r.hasRiver);
	h.Real(r.riverDischarge);
	h.Real(r.riverDischargeM3PerSecond);
	h.Real(r.annualRunoffVolumeKm3);
	h.Int(r.isLake);
	h.Real(r.drainageAccumulation);
	h.Real(r.lakeFraction);
	h.Real(r.lakeSurfaceAreaKm2);
	h.Real(r.lakeWaterVolumeKm3);
	h.Real(r.lakeAnnualInflowKm3);
	h.Real(r.lakeAnnualEvaporationKm3);
	h.Real(r.lakeAnnualInfiltrationKm3);
	h.Real(r.lakeAnnualSpillKm3);
	h.Real(r.lakeWaterLevelKm);
	h.Real(r.lakeSpillElevationKm);
	h.Real(r.lakeSpillHeightKm);
	h.Real(r.soilFertility);
	h.Real(r.soilDepthNorm);
	h.Real(r.temperatureCelsius);
	h.Real(r.temperatureAnnualRangeC);
	h.Real(r.precipitationMm);
	for (double v : r.monthlyTemperatureCelsius)
	{
		h.Real(v);
	}
	for (double v : r.monthlyPrecipitationMm)
	{
		h.Real(v);
	}
	h.Real(r.drySeasonMonths);
	h.Real(r.humidityNormalized);
	h.Real(r.soilMoisture);
	h.Real(r.snowDays);
	h.Real(r.continentality);
	h.Real(r.windMoistureFactor);
	h.Int(r.biome);
}

void Add(Hasher &h, const Province &p)
{
	h.Int(p.id);
	h.Int(p.type);
	h.Int(p.cellCount);
	h.Real(p.areaKm2);
	h.Real(p.centroidLatitudeDegrees);
	h.Real(p.centroidLongitudeDegrees);
	h.Real(p.landFraction);
	h.Real(p.meanCrustAgeMyr);
	h.Real(p.meanSedimentThicknessKm);
	h.Real(p.eventAgeMyr);
	h.Real(p.erosionDepthKm);
	h.Int(p.erosionLevel);
	h.Real(p.compressionAzimuthDegrees);
}

void Add(Hasher &h, const PlanetWaterBalance &w)
{
	h.Real(w.landRunoffKm3);
	h.Real(w.lakePrecipitationGainKm3);
	h.Real(w.lakeLossKm3);
	h.Real(w.drySinkLossKm3);
	h.Real(w.oceanDischargeKm3);
	h.Real(w.landSinkKm3);
}

std::uint64_t HashPlanet(std::uint32_t seed, double axialTiltDegrees = 23.44, double planetAgeYears = 4.5e9)
{
	PlanetParameters parameters;
	parameters.seed = seed;
	parameters.latitudeBands = 90;
	parameters.longitudeSegments = 180;
	ClimateInputs climate;
	climate.axialTiltDegrees = axialTiltDegrees;
	climate.planetAgeYears = planetAgeYears;
	const PlanetMap map(parameters, climate);

	Hasher h;
	for (int lat = 0; lat < map.GetLatitudeBandCount(); ++lat)
	{
		for (int lon = 0; lon < map.GetLongitudeSegmentCount(); ++lon)
		{
			Add(h, map.GetRegion(lat, lon));
		}
	}
	for (const Province &province : map.GetProvinces())
	{
		Add(h, province);
	}
	Add(h, map.GetWaterBalance());
	h.Int(map.GetEpochs().Epochs().size());
	return h.Value();
}
} // namespace

TEST_CASE("GoldenPlanetSeed1")
{
	CHECK(HashPlanet(1) == 0x253167e90d169d22ULL);
}
TEST_CASE("GoldenPlanetSeed7")
{
	CHECK(HashPlanet(7, 40.0, 2.0e9) == 0xd7993c05cc79d666ULL);
}
TEST_CASE("GoldenPlanetSeed12345")
{
	CHECK(HashPlanet(12345) == 0x71074ef71b82cdd0ULL);
}
