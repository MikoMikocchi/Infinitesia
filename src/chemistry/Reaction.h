#pragma once

#include "chemistry/Composition.h"

#include <string>
#include <string_view>
#include <vector>

namespace Infinitesia::Chemistry
{
    // `coefficient` units of the species written as `formula`.
    struct ReactionTerm
    {
        double coefficient = 1.0;
        std::string formula;
        Composition composition;
    };

    // Net change of the number of atoms of one element over the whole equation
    // (products minus reactants).
    struct AtomImbalance
    {
        int atomicNumber = 0;
        double delta = 0.0;
    };

    // A chemical equation reduced to what conservation checks need: species with
    // stoichiometric coefficients on two sides. Conditions, enthalpy, kinetics and
    // catalysts belong to the reaction registry built on top of this.
    //
    // Text form: "2 H2 + O2 -> 2 H2O". The arrow is "->", "=>", "=" or U+2192, species
    // are separated by '+', and an optional coefficient (integer or decimal) precedes
    // each formula (see Composition for the formula syntax).
    struct Reaction
    {
        std::vector<ReactionTerm> reactants;
        std::vector<ReactionTerm> products;

        // Throws std::invalid_argument on malformed input.
        static Reaction Parse(std::string_view equation);

        // Elements whose atom count changes by more than `tolerance`, relative to the
        // number of atoms of that element on the larger side (at least 1).
        std::vector<AtomImbalance> Imbalances(double tolerance = 1e-9) const;

        bool IsBalanced(double tolerance = 1e-9) const { return Imbalances(tolerance).empty(); }

        // Total mass of each side, g per one reaction as written. Equal whenever the
        // atoms balance.
        double ReactantMass() const;
        double ProductMass() const;
    };
}
