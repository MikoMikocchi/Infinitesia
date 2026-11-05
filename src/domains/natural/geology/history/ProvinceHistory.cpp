#include "domains/natural/geology/history/ProvinceHistory.h"

#include <algorithm>
#include <cmath>

#include "domains/natural/geology/util/StableRandom.h"

namespace Infinitesia
{
namespace
{
constexpr std::uint64_t kEventStage = 0x6576'656e'0005ull; // "even"

EventType FormationEventType(RockClass rockClass)
{
	switch (rockClass)
	{
	case RockClass::IgneousIntrusive:
		return EventType::Intrusion;
	case RockClass::IgneousExtrusive:
	case RockClass::Volcaniclastic:
		return EventType::VolcanicEpisode;
	case RockClass::Metamorphic:
		return EventType::RegionalMetamorphism;
	case RockClass::SedimentaryClastic:
	case RockClass::SedimentaryChemical:
	case RockClass::SedimentaryBiogenic:
		return EventType::Sedimentation;
	}
	return EventType::Sedimentation;
}
} // namespace

std::string_view EventTypeName(EventType type) noexcept
{
	switch (type)
	{
	case EventType::BasementFormation:
		return "basement_formation";
	case EventType::Sedimentation:
		return "sedimentation";
	case EventType::VolcanicEpisode:
		return "volcanic_episode";
	case EventType::Intrusion:
		return "intrusion";
	case EventType::RegionalMetamorphism:
		return "regional_metamorphism";
	case EventType::Folding:
		return "folding";
	case EventType::Faulting:
		return "faulting";
	case EventType::Erosion:
		return "erosion";
	}
	return "unknown";
}

const GeologicEvent *ProvinceHistory::Find(EventId id) const noexcept
{
	for (const GeologicEvent &event : events)
	{
		if (event.id == id)
			return &event;
	}
	return nullptr;
}

MainEventRole MainEventRoleOf(ProvinceType type) noexcept
{
	switch (type)
	{
	case ProvinceType::OceanicRidge:
	case ProvinceType::IslandArc:
	case ProvinceType::ContinentalArc:
	case ProvinceType::LargeIgneousProvince:
		return MainEventRole::Magmatism;
	case ProvinceType::Trench:
	case ProvinceType::CollisionOrogen:
	case ProvinceType::AncientOrogen:
	case ProvinceType::Craton:
	case ProvinceType::MetamorphicComplex:
		return MainEventRole::Deformation;
	case ProvinceType::AbyssalPlain:
	case ProvinceType::RiftSystem:
	case ProvinceType::Platform:
	case ProvinceType::SedimentaryBasin:
	case ProvinceType::PassiveMargin:
		return MainEventRole::Deposition;
	}
	return MainEventRole::Deposition;
}

std::vector<TimeSpan> CoverTimeline(double planetAgeMyr, double eventAgeMyr, double crustAgeMyr, ProvinceType type,
                                    std::size_t covers, double *basementAgeMyr, double *coverEndMyr)
{
	const double planetAge = std::max(planetAgeMyr, 1.0);
	const double mainEvent = std::clamp(eventAgeMyr, 0.0, planetAge);
	// The basement is older than everything laid on it, and than the main event.
	const double basementAge = std::clamp(std::max(crustAgeMyr, mainEvent * 1.25 + 1.0), 0.0, planetAge);
	const double coverEnd = MainEventRoleOf(type) == MainEventRole::Deformation
	                            ? mainEvent + 0.25 * (basementAge - mainEvent) // folded afterwards
	                            : mainEvent;                                   // up to the main event
	// A cover builds up over a few hundred million years at most (Earth timeline,
	// scaled).
	const double span = std::min(basementAge - coverEnd, 500.0 * planetAge / 4540.0);
	if (basementAgeMyr != nullptr)
		*basementAgeMyr = basementAge;
	if (coverEndMyr != nullptr)
		*coverEndMyr = coverEnd;

	std::vector<TimeSpan> timeline;
	const double slot = covers > 0 ? std::max(span, 0.0) / static_cast<double>(covers) : 0.0;
	const double coverStart = coverEnd + std::max(span, 0.0);
	for (std::size_t k = 0; k < covers; ++k)
	{
		TimeSpan time;
		time.startMyr = coverStart - static_cast<double>(k) * slot - 0.02 * slot;
		time.endMyr = coverStart - static_cast<double>(k + 1) * slot + 0.1 * slot;
		if (k + 1 == covers)
		{
			time.endMyr = coverEnd; // the last layer ends exactly where the cover ends
		}
		timeline.push_back(time);
	}
	return timeline;
}

ProvinceHistory BuildProvinceHistory(const HistoryInput &input)
{
	ProvinceHistory history;
	if (input.formationsBottomUp.empty())
	{
		return history;
	}

	const double planetAge = std::max(input.planetAgeMyr, 1.0);
	const double mainEvent = std::clamp(input.eventAgeMyr, 0.0, planetAge);
	const MainEventRole role = MainEventRoleOf(input.type);

	const std::size_t count = input.formationsBottomUp.size();
	const std::size_t covers = count - 1; // formations above the basement
	double basementAge = 0.0;
	double coverEnd = 0.0;
	const std::vector<TimeSpan> timeline =
	    CoverTimeline(planetAge, input.eventAgeMyr, input.crustAgeMyr, input.type, covers, &basementAge, &coverEnd);

	const auto makeId = [&](std::uint64_t index) {
		return GeologyRandom::Stream(input.seed, kEventStage, input.province, index);
	};

	// Basement.
	{
		GeologicEvent event;
		event.id = makeId(0);
		event.type = EventType::BasementFormation;
		event.province = input.province;
		event.startMyr = std::min(planetAge, basementAge * 1.02 + 1.0);
		event.endMyr = basementAge;
		event.rock = input.formationsBottomUp[0].rock;
		event.magnitude = input.formationsBottomUp[0].thicknessMeters;
		event.erased = input.formationsBottomUp[0].erased;
		event.products.push_back({ObjectRef::Kind::Formation, 0});
		history.events.push_back(event);
		history.formationEvents.push_back(0);
	}

	// Formations in the order of superposition: each is younger than the one
	// below it.
	for (std::size_t k = 0; k < covers; ++k)
	{
		const HistoryFormation &formation = input.formationsBottomUp[k + 1];
		GeologicEvent event;
		event.id = makeId(k + 1);
		event.type = FormationEventType(formation.rockClass);
		event.province = input.province;
		event.startMyr = timeline[k].startMyr;
		event.endMyr = timeline[k].endMyr;
		event.rock = formation.rock;
		event.magnitude = formation.thicknessMeters;
		event.erased = formation.erased;
		event.parents.push_back(history.events.back().id);
		event.products.push_back({ObjectRef::Kind::Formation, static_cast<std::uint32_t>(k + 1)});
		history.formationEvents.push_back(static_cast<int>(history.events.size()));
		history.events.push_back(event);
	}

	// Deformation after the last formation: folding first, faults cutting the
	// folds.
	const double afterCover = coverEnd;
	{
		GeologicEvent folding;
		folding.id = makeId(count);
		folding.type = EventType::Folding;
		folding.province = input.province;
		// An orogeny lasts tens of millions of years (scaled to the planet's age),
		// not the whole time since the cover was laid down.
		const double orogeny = std::clamp(0.1 * (afterCover - mainEvent), 1.0, 100.0 * planetAge / 4540.0);
		folding.startMyr =
		    role == MainEventRole::Deformation ? std::min(afterCover, mainEvent + orogeny) : afterCover * 0.9;
		folding.endMyr = role == MainEventRole::Deformation ? mainEvent : afterCover * 0.7;
		folding.magnitude = input.dipDegrees;
		folding.parents.push_back(history.events.back().id);
		folding.products.push_back({ObjectRef::Kind::Structure, 0});
		history.foldingEvent = static_cast<int>(history.events.size());
		history.events.push_back(folding);
	}
	{
		const GeologicEvent &folding = history.events[static_cast<std::size_t>(history.foldingEvent)];
		GeologicEvent faulting;
		faulting.id = makeId(count + 1);
		faulting.type = EventType::Faulting;
		faulting.province = input.province;
		faulting.startMyr = folding.endMyr * 0.9;
		faulting.endMyr = std::min(faulting.startMyr, std::max(faulting.startMyr * 0.6, 0.0));
		faulting.magnitude = input.maxFaultSlipMeters;
		faulting.parents.push_back(folding.id);
		faulting.products.push_back({ObjectRef::Kind::FaultSet, 0});
		history.faultingEvent = static_cast<int>(history.events.size());
		history.events.push_back(faulting);
	}

	// Erosion down to the present level, when it stripped formations or cut
	// deeply.
	const bool stripped = std::any_of(input.formationsBottomUp.begin(), input.formationsBottomUp.end(),
	                                  [](const HistoryFormation &formation) { return formation.erased; });
	if (stripped || input.erosionDepthKm >= 1.0)
	{
		const GeologicEvent &faulting = history.events[static_cast<std::size_t>(history.faultingEvent)];
		GeologicEvent erosion;
		erosion.id = makeId(count + 2);
		erosion.type = EventType::Erosion;
		erosion.province = input.province;
		erosion.startMyr = faulting.endMyr * 0.9;
		erosion.endMyr = 0.0;
		erosion.magnitude = input.erosionDepthKm;
		erosion.parents.push_back(faulting.id);
		history.erosionEvent = static_cast<int>(history.events.size());
		history.events.push_back(erosion);
	}
	return history;
}
} // namespace Infinitesia
