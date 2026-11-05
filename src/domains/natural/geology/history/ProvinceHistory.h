#pragma once

#include <cstdint>
#include <vector>

#include "domains/natural/geology/history/GeologicEvent.h"
#include "matter/MatterTypes.h"

namespace Infinitesia
{
// One formation of a province's column before erosion.
struct HistoryFormation
{
	MatterId rock = kUnspecifiedMatter;
	RockClass rockClass = RockClass::SedimentaryClastic;
	double thicknessMeters = 0.0; // real thickness, before the game scale
	bool erased = false;          // stripped off by erosion
};

// What the history of a province is built from.
struct HistoryInput
{
	std::uint64_t seed = 0;
	ProvinceId province = kNoProvince;
	ProvinceType type = ProvinceType::AbyssalPlain;
	double planetAgeMyr = 4540.0;
	double eventAgeMyr = 0.0;                         // the province's main event
	double crustAgeMyr = 0.0;                         // age of the basement crust
	std::vector<HistoryFormation> formationsBottomUp; // the basement first
	double dipDegrees = 0.0;
	int foldCount = 0;
	double maxFaultSlipMeters = 0.0;
	double erosionDepthKm = 0.0;
};

// The events that made the province, oldest first: the basement, each formation
// in the order of superposition, the deformation that folded and broke them,
// and the erosion that cut the present surface. Every event but the basement
// has a parent that is older.
struct ProvinceHistory
{
	std::vector<GeologicEvent> events;
	std::vector<int> formationEvents; // index into `events`, per formation bottom-up
	int foldingEvent = -1;
	int faultingEvent = -1;
	int erosionEvent = -1;

	const GeologicEvent *Find(EventId id) const noexcept;
};

// How the main event of a province type relates to its formations.
enum class MainEventRole : std::uint8_t
{
	Deposition,  // the formations are a cover laid down up to the main event
	Magmatism,   // volcanic and intrusive activity builds the top formations
	Deformation, // the formations are older and the main event folds and
	             // metamorphoses them
};
MainEventRole MainEventRoleOf(ProvinceType type) noexcept;

// The span of time one formation was laid down in.
struct TimeSpan
{
	double startMyr = 0.0;
	double endMyr = 0.0;
};

// When the `covers` formations above the basement of a province formed, oldest
// first. A cover is laid down in the last few hundred million years before the
// end of the cover (the main event for a depositional or magmatic province, a
// little before the main event for a deformed one), not spread over the whole
// age of the crust; `basementAgeMyr` and `coverEndMyr` return the age of the
// basement and the end of the cover.
std::vector<TimeSpan> CoverTimeline(double planetAgeMyr, double eventAgeMyr, double crustAgeMyr, ProvinceType type,
                                    std::size_t covers, double *basementAgeMyr = nullptr,
                                    double *coverEndMyr = nullptr);

ProvinceHistory BuildProvinceHistory(const HistoryInput &input);
} // namespace Infinitesia
