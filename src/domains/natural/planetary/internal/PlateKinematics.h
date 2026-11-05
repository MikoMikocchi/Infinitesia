#pragma once

#include <utility>

#include "domains/natural/planetary/internal/GenerationMath.h"

// Plates, their motion, the boundaries between them and the crust's isostasy.
namespace Infinitesia::GenerationDetail
{
struct EulerPole
{
	Vec3d axis{};
	double omegaRadPerMy = 0.0;
};

struct PlateKinematics
{
	EulerPole euler{};
};

struct TectonicPlate
{
	int id = 0;
	double centerLat = 0.0;
	double centerLon = 0.0;
	bool isOceanic = false;
	double crustDensity = 2900.0;
	double crustThicknessKm = 7.0;
	PlateKinematics kinematics{};
};

enum class BoundaryType
{
	Divergent,
	Convergent,
	Transform
};

enum class BoundarySubtype
{
	MidOceanRidge,
	ContinentalRift,
	OceanicSubduction,
	ContinentalSubduction,
	ContinentalCollision,
	TransformFault
};

std::pair<double, double> EulerVelocityDegPerMy(const EulerPole &pole, double latitudeDegrees, double longitudeDegrees);
BoundaryType ClassifyBoundaryFromVelocity(double velocityALatitude, double velocityALongitude, double velocityBLatitude,
                                          double velocityBLongitude, double normalLatitude, double normalLongitude);
BoundarySubtype DetailedBoundary(BoundaryType boundaryType, bool plateAOceanic, bool plateBOceanic);

extern const double kOceanicRho;
extern const double kContinentalRho;
double AiryIsostasyKm(double crustThicknessKm, double crustDensity);
double OceanicBathymetryKm(double crustAgeNormalized, double maximumAgeMyr);
} // namespace Infinitesia::GenerationDetail
