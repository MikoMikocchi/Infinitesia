#include "domains/natural/chemistry/Reaction.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>
#include <stdexcept>

namespace Infinitesia::Chemistry {
namespace {
struct Arrow {
  std::size_t position = std::string_view::npos;
  std::size_t length = 0;
};

// The first arrow in the text. "=>" and "->" are tried before the bare "=".
Arrow FindArrow(std::string_view text) {
  static constexpr std::string_view kArrows[] = {"->", "=>", "\xE2\x86\x92",
                                                 "="};
  Arrow best;
  for (std::string_view arrow : kArrows) {
    const std::size_t at = text.find(arrow);
    if (at != std::string_view::npos &&
        (best.position == std::string_view::npos || at < best.position ||
         (at == best.position && arrow.size() > best.length))) {
      best = {at, arrow.size()};
    }
  }
  return best;
}

std::string_view Trim(std::string_view text) {
  while (!text.empty() && text.front() == ' ') {
    text.remove_prefix(1);
  }
  while (!text.empty() && text.back() == ' ') {
    text.remove_suffix(1);
  }
  return text;
}

ReactionTerm ParseTerm(std::string_view text) {
  text = Trim(text);
  std::size_t digits = 0;
  while (
      digits < text.size() &&
      ((text[digits] >= '0' && text[digits] <= '9') || text[digits] == '.')) {
    ++digits;
  }

  ReactionTerm term;
  if (digits > 0) {
    double coefficient = 0.0;
    const auto result =
        std::from_chars(text.data(), text.data() + digits, coefficient);
    if (result.ec != std::errc() || result.ptr != text.data() + digits ||
        !std::isfinite(coefficient) || coefficient <= 0.0) {
      throw std::invalid_argument("Invalid reaction coefficient in \"" +
                                  std::string(text) + "\"");
    }
    term.coefficient = coefficient;
  }
  term.formula = std::string(Trim(text.substr(digits)));
  term.composition = Composition::Parse(term.formula);
  return term;
}

std::vector<ReactionTerm> ParseSide(std::string_view text,
                                    const char* sideName) {
  std::vector<ReactionTerm> terms;
  while (true) {
    const std::size_t plus = text.find('+');
    const std::string_view part = Trim(text.substr(0, plus));
    if (part.empty()) {
      throw std::invalid_argument(std::string("Empty species on the ") +
                                  sideName + " side");
    }
    terms.push_back(ParseTerm(part));
    if (plus == std::string_view::npos) {
      break;
    }
    text.remove_prefix(plus + 1);
  }
  return terms;
}

double SideMass(const std::vector<ReactionTerm>& terms) {
  double mass = 0.0;
  for (const ReactionTerm& term : terms) {
    mass += term.coefficient * term.composition.MolarMass();
  }
  return mass;
}
}  // namespace

Reaction Reaction::Parse(std::string_view equation) {
  const Arrow arrow = FindArrow(equation);
  if (arrow.position == std::string_view::npos) {
    throw std::invalid_argument("Reaction has no arrow: \"" +
                                std::string(equation) + "\"");
  }

  Reaction reaction;
  reaction.reactants =
      ParseSide(equation.substr(0, arrow.position), "reactant");
  reaction.products =
      ParseSide(equation.substr(arrow.position + arrow.length), "product");
  return reaction;
}

std::vector<AtomImbalance> Reaction::Imbalances(double tolerance) const {
  std::map<int, double> delta;
  std::map<int, double> larger;
  std::map<int, double> reactantAtoms;
  std::map<int, double> productAtoms;
  for (const ReactionTerm& term : reactants) {
    for (const Composition::Entry& entry : term.composition.Entries()) {
      reactantAtoms[entry.atomicNumber] += term.coefficient * entry.amount;
    }
  }
  for (const ReactionTerm& term : products) {
    for (const Composition::Entry& entry : term.composition.Entries()) {
      productAtoms[entry.atomicNumber] += term.coefficient * entry.amount;
    }
  }
  for (const auto& [z, amount] : reactantAtoms) {
    delta[z] -= amount;
    larger[z] = std::max(larger[z], amount);
  }
  for (const auto& [z, amount] : productAtoms) {
    delta[z] += amount;
    larger[z] = std::max(larger[z], amount);
  }

  std::vector<AtomImbalance> result;
  for (const auto& [z, change] : delta) {
    if (std::abs(change) > tolerance * std::max(1.0, larger[z])) {
      result.push_back({z, change});
    }
  }
  return result;
}

double Reaction::ReactantMass() const { return SideMass(reactants); }

double Reaction::ProductMass() const { return SideMass(products); }
}  // namespace Infinitesia::Chemistry
