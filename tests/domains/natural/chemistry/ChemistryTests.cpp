#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "domains/natural/chemistry/Composition.h"
#include "domains/natural/chemistry/Element.h"
#include "domains/natural/chemistry/Reaction.h"

#include <cmath>
#include <set>
#include <span>
#include <stdexcept>
#include <string>

#define Require(condition, message)     do { if (!(condition)) { FAIL_CHECK(message); } } while (0)

namespace
{
    using namespace Infinitesia::Chemistry;

    int Z(std::string_view symbol)
    {
        const Element* element = FindElement(symbol);
        REQUIRE(element != nullptr);
        return element->atomicNumber;
    }

    int CountCategory(ElementCategory category)
    {
        int count = 0;
        for (const Element& element : AllElements())
        {
            count += element.category == category ? 1 : 0;
        }
        return count;
    }

    int CountBlock(ElementBlock block)
    {
        int count = 0;
        for (const Element& element : AllElements())
        {
            count += element.block == block ? 1 : 0;
        }
        return count;
    }

    // Molar mass tolerance: the reference values are rounded to three decimals.
    constexpr double kMassTolerance = 0.002;
}

TEST_CASE("ElementTableCoversAll118ElementsInOrder")
{
    REQUIRE(AllElements().size() == static_cast<std::size_t>(kElementCount));

    std::set<std::string_view> symbols;
    std::set<std::string_view> names;
    for (int z = 1; z <= kElementCount; ++z)
    {
        const Element& element = GetElement(z);
        Require(element.atomicNumber == z, "atomic numbers must be 1..118 in order");
        Require(!element.symbol.empty() && !element.name.empty(), "symbol and name must be set");
        Require(element.symbol.size() <= 2, "symbols have at most two letters");
        Require(element.atomicMass > 0.0, "atomic mass must be positive");
        Require(element.atomicMass >= static_cast<double>(z), "mass cannot be below the atomic number");
        Require(symbols.insert(element.symbol).second, "symbols must be unique");
        Require(names.insert(element.name).second, "names must be unique");
        Require(FindElement(element.symbol) == &element, "lookup by symbol must find the same element");
    }

    CHECK(FindElement("Xx") == nullptr);
    CHECK(FindElement("fe") == nullptr);
    CHECK(FindElement("") == nullptr);
    CHECK_THROWS_AS(GetElement(0), std::out_of_range);
    CHECK_THROWS_AS(GetElement(kElementCount + 1), std::out_of_range);
}

TEST_CASE("ElementPeriodicStructureIsConsistent")
{
    CHECK(GetElement(Z("H")).group == 1);
    CHECK(GetElement(Z("He")).group == 18);
    CHECK(GetElement(Z("Fe")).group == 8);
    CHECK(GetElement(Z("Fe")).period == 4);
    CHECK(GetElement(Z("Zn")).group == 12);
    CHECK(GetElement(Z("Al")).group == 13);
    CHECK(GetElement(Z("Pb")).group == 14);
    CHECK(GetElement(Z("Cl")).group == 17);
    CHECK(GetElement(Z("La")).group == 0);
    CHECK(GetElement(Z("Yb")).group == 0);
    CHECK(GetElement(Z("Lu")).group == 3);
    CHECK(GetElement(Z("Hf")).group == 4);
    CHECK(GetElement(Z("Ac")).group == 0);
    CHECK(GetElement(Z("No")).group == 0);
    CHECK(GetElement(Z("Lr")).group == 3);
    CHECK(GetElement(Z("Og")).group == 18);
    CHECK(GetElement(Z("Og")).period == 7);

    // Period lengths 2, 8, 8, 18, 18, 32, 32.
    const int lengths[] = { 2, 8, 8, 18, 18, 32, 32 };
    for (int period = 1; period <= 7; ++period)
    {
        int count = 0;
        for (const Element& element : AllElements())
        {
            count += element.period == period ? 1 : 0;
        }
        CHECK_MESSAGE(count == lengths[period - 1], "period ", period);
    }

    // A group-less (f-block) element is exactly a La-Yb or Ac-No one.
    for (const Element& element : AllElements())
    {
        const bool fBlock = (element.atomicNumber >= 57 && element.atomicNumber <= 70) ||
                            (element.atomicNumber >= 89 && element.atomicNumber <= 102);
        Require((element.group == 0) == fBlock, "group 0 only for the f-block");
        Require((element.block == ElementBlock::F) == fBlock, "f block only for La-Yb and Ac-No");
        Require(element.group >= 0 && element.group <= 18, "group out of range");
    }

    CHECK(CountBlock(ElementBlock::S) == 14);
    CHECK(CountBlock(ElementBlock::P) == 36);
    CHECK(CountBlock(ElementBlock::D) == 40);
    CHECK(CountBlock(ElementBlock::F) == 28);
}

TEST_CASE("ElementCategoriesHaveExpectedSizes")
{
    CHECK(CountCategory(ElementCategory::AlkaliMetal) == 6);
    CHECK(CountCategory(ElementCategory::AlkalineEarthMetal) == 6);
    CHECK(CountCategory(ElementCategory::Metalloid) == 6);
    CHECK(CountCategory(ElementCategory::Nonmetal) == 7);
    CHECK(CountCategory(ElementCategory::Halogen) == 6);
    CHECK(CountCategory(ElementCategory::NobleGas) == 7);
    CHECK(CountCategory(ElementCategory::Lanthanide) == 15);
    CHECK(CountCategory(ElementCategory::Actinide) == 15);
    CHECK(GetElement(Z("Fe")).category == ElementCategory::TransitionMetal);
    CHECK(GetElement(Z("Pb")).category == ElementCategory::PostTransitionMetal);
    CHECK(GetElement(Z("Si")).category == ElementCategory::Metalloid);
}

TEST_CASE("ElementAtomicMassesMatchReference")
{
    CHECK(GetElement(Z("H")).atomicMass == doctest::Approx(1.008).epsilon(1e-4));
    CHECK(GetElement(Z("C")).atomicMass == doctest::Approx(12.011).epsilon(1e-4));
    CHECK(GetElement(Z("O")).atomicMass == doctest::Approx(15.999).epsilon(1e-4));
    CHECK(GetElement(Z("Si")).atomicMass == doctest::Approx(28.085).epsilon(1e-4));
    CHECK(GetElement(Z("S")).atomicMass == doctest::Approx(32.06).epsilon(1e-4));
    CHECK(GetElement(Z("Fe")).atomicMass == doctest::Approx(55.845).epsilon(1e-4));
    CHECK(GetElement(Z("Cu")).atomicMass == doctest::Approx(63.546).epsilon(1e-4));
    CHECK(GetElement(Z("Au")).atomicMass == doctest::Approx(196.96657).epsilon(1e-4));
    CHECK(GetElement(Z("U")).atomicMass == doctest::Approx(238.02891).epsilon(1e-4));

    // Elements without a stable isotope have no standard weight; thorium, protactinium
    // and uranium do.
    CHECK(GetElement(Z("Fe")).hasStandardWeight);
    CHECK(GetElement(Z("Bi")).hasStandardWeight);
    CHECK(GetElement(Z("U")).hasStandardWeight);
    CHECK_FALSE(GetElement(Z("Tc")).hasStandardWeight);
    CHECK_FALSE(GetElement(Z("Pm")).hasStandardWeight);
    CHECK_FALSE(GetElement(Z("Ra")).hasStandardWeight);
    CHECK_FALSE(GetElement(Z("Np")).hasStandardWeight);
    int without = 0;
    for (const Element& element : AllElements())
    {
        without += element.hasStandardWeight ? 0 : 1;
    }
    CHECK(without == 34);
}

TEST_CASE("FormulaParsesCommonCompounds")
{
    const Composition water = Composition::Parse("H2O");
    CHECK(water.Amount(Z("H")) == 2.0);
    CHECK(water.Amount(Z("O")) == 1.0);
    CHECK(water.Amount(Z("Fe")) == 0.0);
    CHECK(water.AtomCount() == 3.0);
    CHECK(water.MolarMass() == doctest::Approx(18.015).epsilon(1e-4));

    CHECK(Composition::Parse("SiO2").MolarMass() == doctest::Approx(60.083).epsilon(1e-4));
    CHECK(Composition::Parse("Fe2O3").MolarMass() == doctest::Approx(159.687).epsilon(1e-4));
    CHECK(Composition::Parse("CaCO3").MolarMass() == doctest::Approx(100.086).epsilon(1e-4));
    CHECK(Composition::Parse("NaCl").MolarMass() == doctest::Approx(58.44).epsilon(1e-3));
    CHECK(Composition::Parse("CuFeS2").MolarMass() == doctest::Approx(183.511).epsilon(1e-4));
    CHECK(Composition::Parse("Au").MolarMass() == doctest::Approx(196.96657).epsilon(1e-5));

    // Entries come out sorted by atomic number regardless of the written order.
    const Composition cuFeS2 = Composition::Parse("S2FeCu");
    REQUIRE(cuFeS2.Entries().size() == 3);
    CHECK(cuFeS2.Entries()[0].atomicNumber == Z("S"));
    CHECK(cuFeS2.Entries()[1].atomicNumber == Z("Fe"));
    CHECK(cuFeS2.Entries()[2].atomicNumber == Z("Cu"));
    CHECK(cuFeS2 == Composition::Parse("CuFeS2"));
}

TEST_CASE("FormulaGroupsMultiplyAndNest")
{
    const Composition garnet = Composition::Parse("Ca3Al2(SiO4)3");
    CHECK(garnet.Amount(Z("Ca")) == 3.0);
    CHECK(garnet.Amount(Z("Al")) == 2.0);
    CHECK(garnet.Amount(Z("Si")) == 3.0);
    CHECK(garnet.Amount(Z("O")) == 12.0);

    const Composition nested = Composition::Parse("Ca3[Al2(SiO4)3]2");
    CHECK(nested.Amount(Z("Al")) == 4.0);
    CHECK(nested.Amount(Z("O")) == 24.0);

    // Repeated elements are merged.
    CHECK(Composition::Parse("Fe3O4") == Composition::Parse("FeOFe2O3"));
    CHECK(Composition::Parse("C2H5OH") == Composition::Parse("C2H6O"));
}

TEST_CASE("FormulaHydratesAndFractionalCounts")
{
    const Composition gypsumAscii = Composition::Parse("CaSO4*2H2O");
    const Composition gypsumDot = Composition::Parse("CaSO4" "\xC2\xB7" "2H2O");
    CHECK(gypsumAscii == gypsumDot);
    CHECK(gypsumAscii == Composition::Parse("CaSH4O6"));
    CHECK(gypsumAscii.MolarMass() == doctest::Approx(172.164).epsilon(1e-4));

    CHECK(Composition::Parse("CuSO4*5H2O").MolarMass() == doctest::Approx(249.677).epsilon(1e-4));
    CHECK(Composition::Parse(" Ca SO4 * 2 H2O ").Empty() == false);

    // Without a coefficient the part after the separator counts once.
    CHECK(Composition::Parse("FeO*Fe2O3") == Composition::Parse("Fe3O4"));

    const Composition olivine = Composition::Parse("(Mg1.8Fe0.2)SiO4");
    CHECK(olivine.Amount(Z("Mg")) == doctest::Approx(1.8));
    CHECK(olivine.Amount(Z("Fe")) == doctest::Approx(0.2));
    CHECK(Composition::Parse("Fe0.95S").Amount(Z("Fe")) == doctest::Approx(0.95));
    CHECK(Composition::Parse("H0.5").Amount(Z("H")) == doctest::Approx(0.5));
}

TEST_CASE("MalformedFormulasAreRejectedWithAPosition")
{
    const char* bad[] = {
        "", "   ", "Xx2", "h2o", "H0", "H-2", "(H2O", "H2O)", "H2)O", "(H2O]", "H2O*", "*H2O",
        "2H2O", "H2.", "H.5", "Fe 2O3", "H2O+", "Ca(OH", "()", "H1e9", "H9999999"
    };
    for (const char* formula : bad)
    {
        std::string error;
        Composition out = Composition::Parse("O2");
        CHECK_MESSAGE(!Composition::TryParse(formula, out, &error), "formula \"", formula, "\"");
        CHECK_MESSAGE(error.find("position") != std::string::npos, "formula \"", formula, "\"");
        CHECK_MESSAGE(out == Composition::Parse("O2"), "failed parse must leave the output unchanged");
        INFO("formula \"", formula, "\"");
        CHECK_THROWS_AS(Composition::Parse(formula), std::invalid_argument);
    }

    // Nesting depth is capped.
    std::string deep(40, '(');
    deep += "H";
    deep += std::string(40, ')');
    CHECK_THROWS_AS(Composition::Parse(deep), std::invalid_argument);
}

TEST_CASE("MassFractionsSumToOneAndMatchStoichiometry")
{
    const Composition water = Composition::Parse("H2O");
    CHECK(water.MassFraction(Z("H")) == doctest::Approx(2.016 / 18.015).epsilon(1e-5));
    CHECK(water.MassFraction(Z("Fe")) == 0.0);

    const char* formulas[] = { "H2O", "Fe2O3", "CuFeS2", "Ca3Al2(SiO4)3", "KAlSi3O8", "CaSO4*2H2O", "(Mg1.8Fe0.2)SiO4" };
    for (const char* formula : formulas)
    {
        const Composition composition = Composition::Parse(formula);
        const ElementMassFractions fractions = composition.MassFractions();
        CHECK_MESSAGE(fractions.Sum() == doctest::Approx(1.0).epsilon(1e-12), formula);
        for (const ElementMassFractions::Entry& entry : fractions.Entries())
        {
            CHECK_MESSAGE(entry.fraction == doctest::Approx(composition.MassFraction(entry.atomicNumber)), formula);
        }
    }

    // Chalcopyrite is 34.6% copper, hematite 69.9% iron.
    CHECK(Composition::Parse("CuFeS2").MassFraction(Z("Cu")) == doctest::Approx(0.3463).epsilon(1e-3));
    CHECK(Composition::Parse("Fe2O3").MassFraction(Z("Fe")) == doctest::Approx(0.6994).epsilon(1e-3));
    CHECK(Composition().MassFractions().Empty());
}

TEST_CASE("BlendMixesEndMembersByMoles")
{
    const CompositionPart parts[] = {
        { Composition::Parse("Mg2SiO4"), 0.9 },
        { Composition::Parse("Fe2SiO4"), 0.1 },
    };
    const Composition blended = Blend(parts);
    const Composition direct = Composition::Parse("(Mg1.8Fe0.2)SiO4");
    for (const Composition::Entry& entry : direct.Entries())
    {
        CHECK(blended.Amount(entry.atomicNumber) == doctest::Approx(entry.amount));
    }
    CHECK(blended.MolarMass() == doctest::Approx(0.9 * Composition::Parse("Mg2SiO4").MolarMass() +
                                                 0.1 * Composition::Parse("Fe2SiO4").MolarMass()));

    const CompositionPart negative[] = { { Composition::Parse("H2O"), -1.0 } };
    CHECK_THROWS_AS(Blend(negative), std::invalid_argument);
    CHECK(Blend(std::span<const CompositionPart>()).Empty());
}

TEST_CASE("ScalingAndAddition")
{
    const Composition water = Composition::Parse("H2O");
    CHECK(water.Scaled(2.0) == Composition::Parse("H4O2"));
    CHECK_THROWS_AS(water.Scaled(0.0), std::invalid_argument);
    CHECK_THROWS_AS(water.Scaled(-1.0), std::invalid_argument);
    CHECK_THROWS_AS(water.Scaled(std::nan("")), std::invalid_argument);

    Composition sum = Composition::Parse("FeO");
    sum += Composition::Parse("Fe2O3");
    CHECK(sum == Composition::Parse("Fe3O4"));
}

TEST_CASE("MassWeightedMixtureOfOreAndHostRock")
{
    const ElementMassFractions pyrite = Composition::Parse("FeS2").MassFractions();
    const ElementMassFractions quartz = Composition::Parse("SiO2").MassFractions();

    const double oreFraction = 0.25;
    const MassPart parts[] = { { pyrite, oreFraction }, { quartz, 1.0 - oreFraction } };
    const ElementMassFractions ore = Mix(parts);

    CHECK(ore.Sum() == doctest::Approx(1.0).epsilon(1e-12));
    CHECK(ore.Fraction(Z("Fe")) == doctest::Approx(oreFraction * pyrite.Fraction(Z("Fe"))));
    CHECK(ore.Fraction(Z("Si")) == doctest::Approx((1.0 - oreFraction) * quartz.Fraction(Z("Si"))));
    CHECK(ore.Fraction(Z("Au")) == 0.0);

    // Weights are relative: any common unit gives the same mixture.
    const MassPart kilograms[] = { { pyrite, 250.0 }, { quartz, 750.0 } };
    CHECK(Mix(kilograms).Fraction(Z("Fe")) == doctest::Approx(ore.Fraction(Z("Fe"))));

    const MassPart zero[] = { { pyrite, 0.0 } };
    CHECK_THROWS_AS(Mix(zero), std::invalid_argument);
    const MassPart negative[] = { { pyrite, -1.0 }, { quartz, 2.0 } };
    CHECK_THROWS_AS(Mix(negative), std::invalid_argument);
    const MassPart empty[] = { { ElementMassFractions(), 1.0 } };
    CHECK_THROWS_AS(Mix(empty), std::invalid_argument);
}

TEST_CASE("ReactionBalanceIsChecked")
{
    const char* balanced[] = {
        "2 H2 + O2 -> 2 H2O",
        "2H2+O2->2H2O",
        "CaCO3 = CaO + CO2",
        "CaCO3 => CaO + CO2",
        "4 FeS2 + 11 O2 -> 2 Fe2O3 + 8 SO2",
        "2 CuFeS2 + 4 O2 -> Cu2S + 2 FeO + 3 SO2",
        "CuSO4*5H2O -> CuSO4 + 5 H2O",
        "H2 + 0.5 O2 -> H2O",
        "Fe2O3 + 3 CO -> 2 Fe + 3 CO2",
        "Fe2O3 + 3 CO \xE2\x86\x92 2 Fe + 3 CO2",
    };
    for (const char* equation : balanced)
    {
        const Reaction reaction = Reaction::Parse(equation);
        CHECK_MESSAGE(reaction.IsBalanced(), equation);
        CHECK_MESSAGE(reaction.ReactantMass() == doctest::Approx(reaction.ProductMass()).epsilon(1e-9), equation);
    }

    const Reaction wrong = Reaction::Parse("H2 + O2 -> H2O");
    CHECK_FALSE(wrong.IsBalanced());
    const std::vector<AtomImbalance> imbalances = wrong.Imbalances();
    REQUIRE(imbalances.size() == 1);
    CHECK(imbalances[0].atomicNumber == Z("O"));
    CHECK(imbalances[0].delta == doctest::Approx(-1.0));

    // The unbalanced equation also loses mass.
    CHECK(wrong.ReactantMass() > wrong.ProductMass());

    // An element missing on one side entirely is reported.
    const Reaction missing = Reaction::Parse("NaCl -> Na");
    REQUIRE(missing.Imbalances().size() == 1);
    CHECK(missing.Imbalances()[0].atomicNumber == Z("Cl"));
}

TEST_CASE("ReactionTermsKeepCoefficientsAndFormulas")
{
    const Reaction reaction = Reaction::Parse("2 H2 + O2 -> 2 H2O");
    REQUIRE(reaction.reactants.size() == 2);
    REQUIRE(reaction.products.size() == 1);
    CHECK(reaction.reactants[0].coefficient == 2.0);
    CHECK(reaction.reactants[0].formula == "H2");
    CHECK(reaction.reactants[1].coefficient == 1.0);
    CHECK(reaction.products[0].composition == Composition::Parse("H2O"));
}

TEST_CASE("MalformedReactionsAreRejected")
{
    const char* bad[] = {
        "", "H2O", "H2 + O2", "-> H2O", "H2 ->", "H2 + -> H2O", "0 H2 -> H2", "2 Xx -> Xx2", "H2 -> H2O +", "H2 + + O2 -> H2O"
    };
    for (const char* equation : bad)
    {
        INFO("equation \"", equation, "\"");
        CHECK_THROWS_AS(Reaction::Parse(equation), std::invalid_argument);
    }
}
