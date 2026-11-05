#include "domains/natural/biology/flora/TreeSkeleton.h"

#include <algorithm>
#include <cmath>

#include "domains/natural/biology/flora/FloraRandom.h"

namespace Infinitesia
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kGoldenAngle = 2.399963229728653;

enum Stage : std::uint64_t
{
	kSize = 1,
	kTrunk,
	kBranch,
	kRoot,
	kFrond
};

double Radians(double degrees)
{
	return degrees * kPi / 180.0;
}

double Lerp(double a, double b, double t)
{
	return a + (b - a) * t;
}

// Unit vector at `azimuth` around the vertical, `fromVertical` radians off
// straight up.
Vec3 Direction(double azimuth, double fromVertical)
{
	const double sideways = std::sin(fromVertical);
	return {sideways * std::cos(azimuth), std::cos(fromVertical), sideways * std::sin(azimuth)};
}

class Grower
{
public:
	Grower(const Species &species, std::uint64_t seed) : m_species(species), m_seed(seed)
	{
	}

	TreeSkeleton Grow()
	{
		ChooseSize();
		GrowTrunk();
		if (m_species.form == PlantForm::Palm)
			GrowFronds();
		else
			GrowBranches();
		GrowCrownTop();
		GrowRoots();
		return std::move(m_skeleton);
	}

private:
	double U(Stage stage, std::uint64_t entity, std::uint64_t index = 0) const
	{
		return FloraRandom::Unit(FloraRandom::Stream(m_seed, stage, entity, index));
	}

	bool Conifer() const
	{
		return m_species.form == PlantForm::Conifer;
	}
	bool Palm() const
	{
		return m_species.form == PlantForm::Palm;
	}

	// Bigger plants are also thicker and wider; each trait still has a share of
	// its own.
	void ChooseSize()
	{
		const double size = U(kSize, 0);
		m_height = Lerp(m_species.heightMeters.min, m_species.heightMeters.max, size);
		m_baseRadius =
		    Lerp(m_species.trunkRadiusMeters.min, m_species.trunkRadiusMeters.max, 0.7 * size + 0.3 * U(kSize, 1));
		m_crownRadius =
		    Lerp(m_species.crownRadiusMeters.min, m_species.crownRadiusMeters.max, 0.6 * size + 0.4 * U(kSize, 2));
		if (Conifer())
			m_leafRadius = 1.4;
		else if (Palm())
			m_leafRadius = 1.2;
		else
			m_leafRadius = std::clamp(m_crownRadius * 0.45, 1.2, 3.2);
		// The foliage on top adds to the height of the trunk.
		m_trunkTop = Conifer() || Palm() ? m_height - 1.0 : m_height - m_leafRadius * 0.8;
		m_trunkTop = std::max(m_trunkTop, 2.0);
	}

	void GrowTrunk()
	{
		const double leanAzimuth = 2.0 * kPi * U(kTrunk, 0);
		const double leanReach = (Conifer() ? 0.015 : 0.04 + 0.06 * U(kTrunk, 1)) * m_height;
		const int pieces = std::max(1, static_cast<int>(std::lround(m_trunkTop / 3.0)));
		m_trunk.push_back({0.0, 0.0, 0.0});
		for (int i = 1; i <= pieces; ++i)
		{
			const double y = m_trunkTop * i / pieces;
			const double lean = leanReach * (y / m_trunkTop) * (y / m_trunkTop);
			Vec3 point{lean * std::cos(leanAzimuth), y, lean * std::sin(leanAzimuth)};
			if (!Conifer() && !Palm() && i < pieces)
			{
				point.x += (U(kTrunk, i, 1) - 0.5) * 0.7;
				point.z += (U(kTrunk, i, 2) - 0.5) * 0.7;
			}
			m_trunk.push_back(point);
			m_skeleton.segments.push_back({m_trunk[m_trunk.size() - 2], point,
			                               TrunkRadius(m_trunk[m_trunk.size() - 2].y), TrunkRadius(y),
			                               TreePart::Trunk});
		}
	}

	double TrunkRadius(double y) const
	{
		return std::max(0.5, m_baseRadius * (1.0 - 0.65 * y / m_trunkTop));
	}

	// The point of the trunk's axis at the height (the trunk is a polyline).
	Vec3 PointOnTrunk(double y) const
	{
		for (std::size_t i = 1; i < m_trunk.size(); ++i)
		{
			if (y <= m_trunk[i].y || i + 1 == m_trunk.size())
			{
				const double span = m_trunk[i].y - m_trunk[i - 1].y;
				const double t = span > 0.0 ? std::clamp((y - m_trunk[i - 1].y) / span, 0.0, 1.0) : 0.0;
				return {Lerp(m_trunk[i - 1].x, m_trunk[i].x, t), Lerp(m_trunk[i - 1].y, m_trunk[i].y, t),
				        Lerp(m_trunk[i - 1].z, m_trunk[i].z, t)};
			}
		}
		return m_trunk.back();
	}

	// How long a branch is, as a share of the crown radius, `u` of the way up the
	// crown (0..1).
	double LengthFactor(double u) const
	{
		switch (m_species.crownShape)
		{
		case CrownShape::Sphere:
			return std::sin(kPi * (0.12 + 0.76 * u));
		case CrownShape::Cone:
			return std::max(0.15, 1.05 - 0.9 * u);
		case CrownShape::Column:
			return 0.55;
		case CrownShape::Umbrella:
			return 0.4 + 0.6 * u;
		}
		return 1.0;
	}

	void AddCluster(Vec3 center, double horizontal, double vertical)
	{
		m_skeleton.clusters.push_back({center, horizontal, vertical});
	}

	void GrowBranches()
	{
		const IntRange range = m_species.branchCount;
		const int count =
		    std::min(range.max, range.min + static_cast<int>(U(kBranch, 0) * (range.max - range.min + 1)));
		if (count <= 0)
			return;
		const double top = m_trunkTop * 0.97;
		double bottom = m_species.crownStartFraction * m_height;
		if (bottom > top * 0.9)
			bottom = top * 0.5;
		const double spacing = (top - bottom) / count;

		for (int i = 0; i < count; ++i)
		{
			const double u = (i + 0.5) / count;
			const double y =
			    std::clamp(bottom + (top - bottom) * u + (U(kBranch, i + 1, 0) - 0.5) * spacing * 0.8, bottom, top);
			const double azimuth = i * kGoldenAngle + (U(kBranch, i + 1, 1) - 0.5) * 0.6;
			double angle = Radians(m_species.branchAngleDegrees + (U(kBranch, i + 1, 2) - 0.5) * 16.0);
			angle = std::min(angle + (1.0 - u) * Radians(12.0), Radians(88.0));
			const double length = std::max(1.5, m_crownRadius * LengthFactor(u));

			const Vec3 start = PointOnTrunk(y);
			const double thick = std::max(0.5, TrunkRadius(y) * 0.5);
			const Vec3 middle = start + Direction(azimuth, angle) * (length * 0.5);
			const double endAngle = std::max(angle - Radians(25.0), Radians(10.0));
			const Vec3 end = middle + Direction(azimuth, endAngle) * (length * 0.5);
			m_skeleton.segments.push_back({start, middle, thick, std::max(0.5, thick * 0.6), TreePart::Branch});
			m_skeleton.segments.push_back({middle, end, std::max(0.5, thick * 0.6), 0.5, TreePart::Branch});

			if (Conifer())
			{
				// A flat tier of needles around the trunk at the height of the branch,
				// and one at its tip.
				AddCluster(start + Vec3{0.0, -0.3, 0.0}, std::max(1.6, length * 0.9), 1.3);
				AddCluster(end, std::clamp(0.6 * length, 1.2, 2.2), 1.1);
				continue;
			}
			AddCluster(end, m_leafRadius * 0.8, m_leafRadius * 0.7);
			const double twigLength = std::max(1.5, length * 0.3);
			const double twigAngle = std::min(endAngle + Radians(20.0), Radians(88.0));
			for (const double turn : {0.8, -0.8})
			{
				const Vec3 tip = end + Direction(azimuth + turn, twigAngle) * twigLength;
				m_skeleton.segments.push_back({end, tip, 0.5, 0.5, TreePart::Twig});
				AddCluster(tip, m_leafRadius, m_leafRadius * 0.8);
			}
		}
	}

	// Palm fronds are leaves from the start: they spring from the top of the
	// trunk, rise, and droop.
	void GrowFronds()
	{
		const IntRange range = m_species.branchCount;
		const int count =
		    std::max(3, std::min(range.max, range.min + static_cast<int>(U(kFrond, 0) * (range.max - range.min + 1))));
		const Vec3 top = m_trunk.back();
		for (int i = 0; i < count; ++i)
		{
			const double azimuth = 2.0 * kPi * (i + 0.5 * U(kFrond, i + 1)) / count;
			const double rise = Radians(m_species.branchAngleDegrees);
			const Vec3 middle = top + Direction(azimuth, rise) * (m_crownRadius * 0.5);
			const Vec3 end = middle + Direction(azimuth, Radians(120.0)) * (m_crownRadius * 0.5);
			m_skeleton.segments.push_back({top, middle, 0.5, 0.5, TreePart::Leaves});
			m_skeleton.segments.push_back({middle, end, 0.5, 0.5, TreePart::Leaves});
			AddCluster(middle, 1.7, 1.0);
			AddCluster(end, 1.5, 1.0);
		}
	}

	void GrowCrownTop()
	{
		const Vec3 top = m_trunk.back();
		if (Palm())
			AddCluster(top + Vec3{0.0, 0.5, 0.0}, m_leafRadius, 1.0);
		else if (Conifer())
			AddCluster(top + Vec3{0.0, 1.0, 0.0}, 1.0, 2.0);
		else
			AddCluster(top + Vec3{0.0, m_leafRadius * 0.4, 0.0}, m_leafRadius * 1.1, m_leafRadius * 0.9);
	}

	// A tap root straight down and a few roots spreading just under the surface.
	void GrowRoots()
	{
		const int depth = m_species.rootDepthMeters;
		if (depth <= 0)
			return;
		m_skeleton.segments.push_back(
		    {{0.0, -1.0, 0.0}, {0.0, -static_cast<double>(depth), 0.0}, 0.5, 0.5, TreePart::Root});
		const int count = 3 + static_cast<int>(U(kRoot, 0) * 3.0);
		const double reach = std::max(1.5, m_baseRadius * 1.5 + depth);
		for (int i = 0; i < count; ++i)
		{
			const double azimuth = 2.0 * kPi * (i + U(kRoot, i + 1)) / count;
			const Vec3 end{reach * std::cos(azimuth), -static_cast<double>(depth), reach * std::sin(azimuth)};
			m_skeleton.segments.push_back({{0.0, -1.0, 0.0}, end, 0.5, 0.5, TreePart::Root});
		}
	}

	const Species &m_species;
	std::uint64_t m_seed;
	TreeSkeleton m_skeleton;
	std::vector<Vec3> m_trunk;
	double m_height = 0.0;
	double m_baseRadius = 0.5;
	double m_crownRadius = 1.0;
	double m_leafRadius = 1.0;
	double m_trunkTop = 2.0;
};
} // namespace

TreeSkeleton GrowSkeleton(const Species &species, std::uint64_t seed)
{
	return Grower(species, seed).Grow();
}
} // namespace Infinitesia
