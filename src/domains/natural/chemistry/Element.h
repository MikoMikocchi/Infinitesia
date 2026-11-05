#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace Infinitesia::Chemistry
{
inline constexpr int kElementCount = 118;

enum class ElementBlock : std::uint8_t
{
	S,
	P,
	D,
	F
};

enum class ElementCategory : std::uint8_t
{
	AlkaliMetal,
	AlkalineEarthMetal,
	TransitionMetal,
	PostTransitionMetal,
	Metalloid,
	Nonmetal,
	Halogen,
	NobleGas,
	Lanthanide,
	Actinide
};

// Reference data of one chemical element. The atomic number is the stable
// numeric ID of the element everywhere (saves, registries) and is never reused.
//
// Richer properties (density, melting point, oxidation states, crustal
// abundance, geochemical affinity) are content data layered on top of this
// table by the registries that need them; this table only holds what defines
// the element.
struct Element
{
	int atomicNumber = 0;
	std::string_view symbol;
	std::string_view name; // English; localized names live in content data
	// g/mol. IUPAC conventional atomic weight (abridged, NIST) when
	// hasStandardWeight is true. Otherwise the element has no stable isotope and
	// this is the mass number of its longest-lived known isotope.
	double atomicMass = 0.0;
	bool hasStandardWeight = false;
	// 1..18; 0 for La-Yb and Ac-No (f-block without a group). Lu and Lr are
	// group 3.
	int group = 0;
	int period = 0;
	ElementBlock block = ElementBlock::S;
	ElementCategory category = ElementCategory::Nonmetal;
};

// All 118 elements ordered by atomic number.
std::span<const Element> AllElements() noexcept;

// Throws std::out_of_range unless 1 <= atomicNumber <= kElementCount.
const Element &GetElement(int atomicNumber);

// Case-sensitive element symbol ("Fe"); nullptr if there is no such element.
const Element *FindElement(std::string_view symbol) noexcept;
} // namespace Infinitesia::Chemistry
