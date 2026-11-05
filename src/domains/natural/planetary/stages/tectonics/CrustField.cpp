#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

#include "domains/natural/planetary/stages/tectonics/TectonicsInternal.h"

namespace Infinitesia::TectonicsDetail
{
using namespace GenerationDetail;

CrustField BuildCrustField(const PlanetBuild &build, std::uint32_t seed)
{
	const int latB = build.LatBands();
	const int lonS = build.LonSegs();
	// Continental lithosphere is spatially continuous and independent of plate
	// boundaries. The 40% target approximates Earth's continental-crust area;
	// it is a calibration, not a prediction from bulk composition.
	// -----------------------------------------------------------------------
	constexpr double kContinentalCrustAreaFraction = 0.40;
	const std::size_t cellCount = static_cast<std::size_t>(latB) * lonS;
	std::vector<double> continentalField(cellCount);
	std::vector<std::pair<double, double>> areaRankedField;
	areaRankedField.reserve(cellCount);
	double totalAreaWeight = 0.0;
	for (int la = 0; la < latB; ++la)
	{
		for (int lo = 0; lo < lonS; ++lo)
		{
			const std::size_t index = static_cast<std::size_t>(la) * lonS + lo;
			const PlanetRegion &cell = build.Cell(la, lo);
			const double field = SphericalFbm(cell.latitudeDegrees, cell.longitudeDegrees, seed ^ 0x71a1d001u, 3, 5);
			const double areaWeight = std::max(0.0, std::cos(cell.latitudeDegrees * std::numbers::pi / 180.0));
			continentalField[index] = field;
			areaRankedField.emplace_back(field, areaWeight);
			totalAreaWeight += areaWeight;
		}
	}

	std::sort(areaRankedField.begin(), areaRankedField.end(),
	          [](const auto &first, const auto &second) { return first.first < second.first; });
	const double targetOceanArea = totalAreaWeight * (1.0 - kContinentalCrustAreaFraction);
	double rankedArea = 0.0;
	double continentalThreshold = 0.5;
	for (const auto &[field, areaWeight] : areaRankedField)
	{
		rankedArea += areaWeight;
		continentalThreshold = field;
		if (rankedArea >= targetOceanArea)
			break;
	}
	std::vector<bool> isContinentalCrust(cellCount, false);
	for (std::size_t index = 0; index < cellCount; ++index)
		isContinentalCrust[index] = continentalField[index] > continentalThreshold;

	CrustField crust;
	crust.field = std::move(continentalField);
	crust.isContinental = std::move(isContinentalCrust);
	crust.threshold = continentalThreshold;
	return crust;
}
} // namespace Infinitesia::TectonicsDetail
