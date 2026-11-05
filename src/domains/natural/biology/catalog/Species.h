#pragma once

#include <string>

#include "domains/natural/biology/niche/Niche.h"
#include "matter/MatterId.h"

namespace Infinitesia
{
// The generator that grows a plant of the species (docs/07_BIOLOGY.md, section
// 4).
enum class PlantForm : unsigned char
{
	Broadleaf,
	Conifer,
	Palm,
	Shrub
};

enum class CrownShape : unsigned char
{
	Sphere,
	Cone,
	Umbrella,
	Column
};

struct IntRange
{
	int min = 0;
	int max = 0;
};

struct DoubleRange
{
	double min = 0.0;
	double max = 0.0;
};

// Matter of the parts of a plant (class Plant of the matter catalog).
struct PlantMatter
{
	MatterId trunk = kUnspecifiedMatter;
	MatterId branch = kUnspecifiedMatter;
	MatterId twig = kUnspecifiedMatter;
	MatterId leaves = kUnspecifiedMatter;
	MatterId root = kUnspecifiedMatter;
};

struct Species
{
	std::string key;  // "species.oak"
	std::string name; // English only
	PlantForm form = PlantForm::Broadleaf;
	Niche niche;

	IntRange heightMeters;         // of a grown plant
	DoubleRange trunkRadiusMeters; // at the base
	CrownShape crownShape = CrownShape::Sphere;
	DoubleRange crownRadiusMeters;
	double crownStartFraction = 0.5; // share of the height where the crown begins
	IntRange branchCount;
	double branchAngleDegrees = 45.0;
	int rootDepthMeters = 1;
	double shadeTolerance = 0.5; // 0..1: can it grow under a canopy

	PlantMatter matter;
	double logsPerCubicMeter = 1.0;
};
} // namespace Infinitesia
