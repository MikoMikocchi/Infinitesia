#pragma once

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Infinitesia::Chemistry
{
class ElementMassFractions;
struct MassPart;

// Amounts of atoms per formula unit, keyed by atomic number and kept sorted by
// it, so equal compositions compare equal and iteration order is deterministic.
//
// Formula syntax (no charges, no "either/or" lists like (F,OH)):
//   Fe2O3, CaCO3. A formula names one concrete composition.
//   Counts may be fractional, which is how solid solutions and
//   non-stoichiometric minerals are stored: (Mg1.8Fe0.2)SiO4, Fe0.95S. Groups
//   nest with () or []: Ca3Al2(SiO4)3, K2[Al2Si6O16]. Hydrates and adducts join
//   with U+00B7 or '*': CuSO4*5H2O, CaSO4·2H2O. The coefficient after the
//   separator multiplies the part that follows it. Spaces are ignored. '.' is
//   only ever a decimal point.
class Composition
{
public:
	struct Entry
	{
		int atomicNumber = 0;
		double amount = 0.0;

		friend bool operator==(const Entry &, const Entry &) = default;
	};

	Composition() = default;

	// Throws std::invalid_argument with the position of the problem.
	static Composition Parse(std::string_view formula);

	// Non-throwing variant. On failure `out` is left unchanged and, if given,
	// `error` receives the message.
	static bool TryParse(std::string_view formula, Composition &out, std::string *error = nullptr);

	bool Empty() const noexcept
	{
		return m_entries.empty();
	}
	std::span<const Entry> Entries() const noexcept
	{
		return m_entries;
	}

	// Atoms of the element per formula unit; 0 if absent.
	double Amount(int atomicNumber) const noexcept;
	double AtomCount() const noexcept;

	// g/mol.
	double MolarMass() const;

	// Mass share of one element, 0..1.
	double MassFraction(int atomicNumber) const;
	ElementMassFractions MassFractions() const;

	// Throws std::invalid_argument unless factor is finite and positive.
	Composition Scaled(double factor) const;
	Composition &operator+=(const Composition &other);

	// Exact comparison of the stored amounts.
	friend bool operator==(const Composition &, const Composition &) = default;

private:
	// Merges duplicates, drops zero amounts and sorts by atomic number.
	static Composition FromRaw(std::vector<Entry> raw);

	std::vector<Entry> m_entries;
};

// `moles` formula units of `composition` as one component of a mixture.
struct CompositionPart
{
	Composition composition;
	double moles = 0.0;
};

// Mole-weighted sum of compositions (end-member mixing of a solid solution):
// the total of all parts, not an average per formula unit. Throws
// std::invalid_argument for a negative amount.
Composition Blend(std::span<const CompositionPart> parts);

// Mass share of each element in a material (a rock, an ore, a mixture), summing
// to 1.
class ElementMassFractions
{
public:
	struct Entry
	{
		int atomicNumber = 0;
		double fraction = 0.0;
	};

	ElementMassFractions() = default;

	bool Empty() const noexcept
	{
		return m_entries.empty();
	}
	std::span<const Entry> Entries() const noexcept
	{
		return m_entries;
	}

	// 0 if the element is absent.
	double Fraction(int atomicNumber) const noexcept;
	double Sum() const noexcept;

private:
	friend class Composition;
	friend ElementMassFractions Mix(std::span<const MassPart> parts);

	explicit ElementMassFractions(std::vector<Entry> sorted) noexcept : m_entries(std::move(sorted))
	{
	}

	std::vector<Entry> m_entries;
};

// One component of a mass-weighted mixture; `weight` is a mass in any unit
// shared by all parts.
struct MassPart
{
	ElementMassFractions fractions;
	double weight = 0.0;
};

// Mass-weighted mixture, e.g. ore = f * assemblage + (1 - f) * host rock.
// Throws std::invalid_argument for a negative weight or a zero total weight.
ElementMassFractions Mix(std::span<const MassPart> parts);
} // namespace Infinitesia::Chemistry
