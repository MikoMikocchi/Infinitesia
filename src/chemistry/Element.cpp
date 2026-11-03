#include "chemistry/Element.h"

#include <array>
#include <stdexcept>

namespace Infinitesia::Chemistry
{
    namespace
    {
        // Masses: NIST "Atomic Weights and Isotopic Compositions" (IUPAC 2021 abridged
        // conventional values where the standard weight is an interval). Z >= 95 have no
        // standard weight; the mass number of the longest-lived isotope is used.
        constexpr std::array<Element, kElementCount> kElements = {{
    { 1, "H", "Hydrogen", 1.008, true, 1, 1, ElementBlock::S, ElementCategory::Nonmetal },
    { 2, "He", "Helium", 4.002602, true, 18, 1, ElementBlock::S, ElementCategory::NobleGas },
    { 3, "Li", "Lithium", 6.94, true, 1, 2, ElementBlock::S, ElementCategory::AlkaliMetal },
    { 4, "Be", "Beryllium", 9.0121831, true, 2, 2, ElementBlock::S, ElementCategory::AlkalineEarthMetal },
    { 5, "B", "Boron", 10.81, true, 13, 2, ElementBlock::P, ElementCategory::Metalloid },
    { 6, "C", "Carbon", 12.011, true, 14, 2, ElementBlock::P, ElementCategory::Nonmetal },
    { 7, "N", "Nitrogen", 14.007, true, 15, 2, ElementBlock::P, ElementCategory::Nonmetal },
    { 8, "O", "Oxygen", 15.999, true, 16, 2, ElementBlock::P, ElementCategory::Nonmetal },
    { 9, "F", "Fluorine", 18.998403163, true, 17, 2, ElementBlock::P, ElementCategory::Halogen },
    { 10, "Ne", "Neon", 20.1797, true, 18, 2, ElementBlock::P, ElementCategory::NobleGas },
    { 11, "Na", "Sodium", 22.98976928, true, 1, 3, ElementBlock::S, ElementCategory::AlkaliMetal },
    { 12, "Mg", "Magnesium", 24.305, true, 2, 3, ElementBlock::S, ElementCategory::AlkalineEarthMetal },
    { 13, "Al", "Aluminium", 26.9815385, true, 13, 3, ElementBlock::P, ElementCategory::PostTransitionMetal },
    { 14, "Si", "Silicon", 28.085, true, 14, 3, ElementBlock::P, ElementCategory::Metalloid },
    { 15, "P", "Phosphorus", 30.973761998, true, 15, 3, ElementBlock::P, ElementCategory::Nonmetal },
    { 16, "S", "Sulfur", 32.06, true, 16, 3, ElementBlock::P, ElementCategory::Nonmetal },
    { 17, "Cl", "Chlorine", 35.45, true, 17, 3, ElementBlock::P, ElementCategory::Halogen },
    { 18, "Ar", "Argon", 39.95, true, 18, 3, ElementBlock::P, ElementCategory::NobleGas },
    { 19, "K", "Potassium", 39.0983, true, 1, 4, ElementBlock::S, ElementCategory::AlkaliMetal },
    { 20, "Ca", "Calcium", 40.078, true, 2, 4, ElementBlock::S, ElementCategory::AlkalineEarthMetal },
    { 21, "Sc", "Scandium", 44.955908, true, 3, 4, ElementBlock::D, ElementCategory::TransitionMetal },
    { 22, "Ti", "Titanium", 47.867, true, 4, 4, ElementBlock::D, ElementCategory::TransitionMetal },
    { 23, "V", "Vanadium", 50.9415, true, 5, 4, ElementBlock::D, ElementCategory::TransitionMetal },
    { 24, "Cr", "Chromium", 51.9961, true, 6, 4, ElementBlock::D, ElementCategory::TransitionMetal },
    { 25, "Mn", "Manganese", 54.938044, true, 7, 4, ElementBlock::D, ElementCategory::TransitionMetal },
    { 26, "Fe", "Iron", 55.845, true, 8, 4, ElementBlock::D, ElementCategory::TransitionMetal },
    { 27, "Co", "Cobalt", 58.933194, true, 9, 4, ElementBlock::D, ElementCategory::TransitionMetal },
    { 28, "Ni", "Nickel", 58.6934, true, 10, 4, ElementBlock::D, ElementCategory::TransitionMetal },
    { 29, "Cu", "Copper", 63.546, true, 11, 4, ElementBlock::D, ElementCategory::TransitionMetal },
    { 30, "Zn", "Zinc", 65.38, true, 12, 4, ElementBlock::D, ElementCategory::TransitionMetal },
    { 31, "Ga", "Gallium", 69.723, true, 13, 4, ElementBlock::P, ElementCategory::PostTransitionMetal },
    { 32, "Ge", "Germanium", 72.630, true, 14, 4, ElementBlock::P, ElementCategory::Metalloid },
    { 33, "As", "Arsenic", 74.921595, true, 15, 4, ElementBlock::P, ElementCategory::Metalloid },
    { 34, "Se", "Selenium", 78.971, true, 16, 4, ElementBlock::P, ElementCategory::Nonmetal },
    { 35, "Br", "Bromine", 79.904, true, 17, 4, ElementBlock::P, ElementCategory::Halogen },
    { 36, "Kr", "Krypton", 83.798, true, 18, 4, ElementBlock::P, ElementCategory::NobleGas },
    { 37, "Rb", "Rubidium", 85.4678, true, 1, 5, ElementBlock::S, ElementCategory::AlkaliMetal },
    { 38, "Sr", "Strontium", 87.62, true, 2, 5, ElementBlock::S, ElementCategory::AlkalineEarthMetal },
    { 39, "Y", "Yttrium", 88.90584, true, 3, 5, ElementBlock::D, ElementCategory::TransitionMetal },
    { 40, "Zr", "Zirconium", 91.224, true, 4, 5, ElementBlock::D, ElementCategory::TransitionMetal },
    { 41, "Nb", "Niobium", 92.90637, true, 5, 5, ElementBlock::D, ElementCategory::TransitionMetal },
    { 42, "Mo", "Molybdenum", 95.95, true, 6, 5, ElementBlock::D, ElementCategory::TransitionMetal },
    { 43, "Tc", "Technetium", 98.0, false, 7, 5, ElementBlock::D, ElementCategory::TransitionMetal },
    { 44, "Ru", "Ruthenium", 101.07, true, 8, 5, ElementBlock::D, ElementCategory::TransitionMetal },
    { 45, "Rh", "Rhodium", 102.90550, true, 9, 5, ElementBlock::D, ElementCategory::TransitionMetal },
    { 46, "Pd", "Palladium", 106.42, true, 10, 5, ElementBlock::D, ElementCategory::TransitionMetal },
    { 47, "Ag", "Silver", 107.8682, true, 11, 5, ElementBlock::D, ElementCategory::TransitionMetal },
    { 48, "Cd", "Cadmium", 112.414, true, 12, 5, ElementBlock::D, ElementCategory::TransitionMetal },
    { 49, "In", "Indium", 114.818, true, 13, 5, ElementBlock::P, ElementCategory::PostTransitionMetal },
    { 50, "Sn", "Tin", 118.710, true, 14, 5, ElementBlock::P, ElementCategory::PostTransitionMetal },
    { 51, "Sb", "Antimony", 121.760, true, 15, 5, ElementBlock::P, ElementCategory::Metalloid },
    { 52, "Te", "Tellurium", 127.60, true, 16, 5, ElementBlock::P, ElementCategory::Metalloid },
    { 53, "I", "Iodine", 126.90447, true, 17, 5, ElementBlock::P, ElementCategory::Halogen },
    { 54, "Xe", "Xenon", 131.293, true, 18, 5, ElementBlock::P, ElementCategory::NobleGas },
    { 55, "Cs", "Caesium", 132.90545196, true, 1, 6, ElementBlock::S, ElementCategory::AlkaliMetal },
    { 56, "Ba", "Barium", 137.327, true, 2, 6, ElementBlock::S, ElementCategory::AlkalineEarthMetal },
    { 57, "La", "Lanthanum", 138.90547, true, 0, 6, ElementBlock::F, ElementCategory::Lanthanide },
    { 58, "Ce", "Cerium", 140.116, true, 0, 6, ElementBlock::F, ElementCategory::Lanthanide },
    { 59, "Pr", "Praseodymium", 140.90766, true, 0, 6, ElementBlock::F, ElementCategory::Lanthanide },
    { 60, "Nd", "Neodymium", 144.242, true, 0, 6, ElementBlock::F, ElementCategory::Lanthanide },
    { 61, "Pm", "Promethium", 145.0, false, 0, 6, ElementBlock::F, ElementCategory::Lanthanide },
    { 62, "Sm", "Samarium", 150.36, true, 0, 6, ElementBlock::F, ElementCategory::Lanthanide },
    { 63, "Eu", "Europium", 151.964, true, 0, 6, ElementBlock::F, ElementCategory::Lanthanide },
    { 64, "Gd", "Gadolinium", 157.25, true, 0, 6, ElementBlock::F, ElementCategory::Lanthanide },
    { 65, "Tb", "Terbium", 158.92535, true, 0, 6, ElementBlock::F, ElementCategory::Lanthanide },
    { 66, "Dy", "Dysprosium", 162.500, true, 0, 6, ElementBlock::F, ElementCategory::Lanthanide },
    { 67, "Ho", "Holmium", 164.93033, true, 0, 6, ElementBlock::F, ElementCategory::Lanthanide },
    { 68, "Er", "Erbium", 167.259, true, 0, 6, ElementBlock::F, ElementCategory::Lanthanide },
    { 69, "Tm", "Thulium", 168.93422, true, 0, 6, ElementBlock::F, ElementCategory::Lanthanide },
    { 70, "Yb", "Ytterbium", 173.054, true, 0, 6, ElementBlock::F, ElementCategory::Lanthanide },
    { 71, "Lu", "Lutetium", 174.9668, true, 3, 6, ElementBlock::D, ElementCategory::Lanthanide },
    { 72, "Hf", "Hafnium", 178.49, true, 4, 6, ElementBlock::D, ElementCategory::TransitionMetal },
    { 73, "Ta", "Tantalum", 180.94788, true, 5, 6, ElementBlock::D, ElementCategory::TransitionMetal },
    { 74, "W", "Tungsten", 183.84, true, 6, 6, ElementBlock::D, ElementCategory::TransitionMetal },
    { 75, "Re", "Rhenium", 186.207, true, 7, 6, ElementBlock::D, ElementCategory::TransitionMetal },
    { 76, "Os", "Osmium", 190.23, true, 8, 6, ElementBlock::D, ElementCategory::TransitionMetal },
    { 77, "Ir", "Iridium", 192.217, true, 9, 6, ElementBlock::D, ElementCategory::TransitionMetal },
    { 78, "Pt", "Platinum", 195.084, true, 10, 6, ElementBlock::D, ElementCategory::TransitionMetal },
    { 79, "Au", "Gold", 196.966569, true, 11, 6, ElementBlock::D, ElementCategory::TransitionMetal },
    { 80, "Hg", "Mercury", 200.592, true, 12, 6, ElementBlock::D, ElementCategory::TransitionMetal },
    { 81, "Tl", "Thallium", 204.38, true, 13, 6, ElementBlock::P, ElementCategory::PostTransitionMetal },
    { 82, "Pb", "Lead", 207.2, true, 14, 6, ElementBlock::P, ElementCategory::PostTransitionMetal },
    { 83, "Bi", "Bismuth", 208.98040, true, 15, 6, ElementBlock::P, ElementCategory::PostTransitionMetal },
    { 84, "Po", "Polonium", 209.0, false, 16, 6, ElementBlock::P, ElementCategory::PostTransitionMetal },
    { 85, "At", "Astatine", 210.0, false, 17, 6, ElementBlock::P, ElementCategory::Halogen },
    { 86, "Rn", "Radon", 222.0, false, 18, 6, ElementBlock::P, ElementCategory::NobleGas },
    { 87, "Fr", "Francium", 223.0, false, 1, 7, ElementBlock::S, ElementCategory::AlkaliMetal },
    { 88, "Ra", "Radium", 226.0, false, 2, 7, ElementBlock::S, ElementCategory::AlkalineEarthMetal },
    { 89, "Ac", "Actinium", 227.0, false, 0, 7, ElementBlock::F, ElementCategory::Actinide },
    { 90, "Th", "Thorium", 232.0377, true, 0, 7, ElementBlock::F, ElementCategory::Actinide },
    { 91, "Pa", "Protactinium", 231.03588, true, 0, 7, ElementBlock::F, ElementCategory::Actinide },
    { 92, "U", "Uranium", 238.02891, true, 0, 7, ElementBlock::F, ElementCategory::Actinide },
    { 93, "Np", "Neptunium", 237.0, false, 0, 7, ElementBlock::F, ElementCategory::Actinide },
    { 94, "Pu", "Plutonium", 244.0, false, 0, 7, ElementBlock::F, ElementCategory::Actinide },
    { 95, "Am", "Americium", 243.0, false, 0, 7, ElementBlock::F, ElementCategory::Actinide },
    { 96, "Cm", "Curium", 247.0, false, 0, 7, ElementBlock::F, ElementCategory::Actinide },
    { 97, "Bk", "Berkelium", 247.0, false, 0, 7, ElementBlock::F, ElementCategory::Actinide },
    { 98, "Cf", "Californium", 251.0, false, 0, 7, ElementBlock::F, ElementCategory::Actinide },
    { 99, "Es", "Einsteinium", 252.0, false, 0, 7, ElementBlock::F, ElementCategory::Actinide },
    { 100, "Fm", "Fermium", 257.0, false, 0, 7, ElementBlock::F, ElementCategory::Actinide },
    { 101, "Md", "Mendelevium", 258.0, false, 0, 7, ElementBlock::F, ElementCategory::Actinide },
    { 102, "No", "Nobelium", 259.0, false, 0, 7, ElementBlock::F, ElementCategory::Actinide },
    { 103, "Lr", "Lawrencium", 266.0, false, 3, 7, ElementBlock::D, ElementCategory::Actinide },
    { 104, "Rf", "Rutherfordium", 267.0, false, 4, 7, ElementBlock::D, ElementCategory::TransitionMetal },
    { 105, "Db", "Dubnium", 268.0, false, 5, 7, ElementBlock::D, ElementCategory::TransitionMetal },
    { 106, "Sg", "Seaborgium", 269.0, false, 6, 7, ElementBlock::D, ElementCategory::TransitionMetal },
    { 107, "Bh", "Bohrium", 270.0, false, 7, 7, ElementBlock::D, ElementCategory::TransitionMetal },
    { 108, "Hs", "Hassium", 277.0, false, 8, 7, ElementBlock::D, ElementCategory::TransitionMetal },
    { 109, "Mt", "Meitnerium", 278.0, false, 9, 7, ElementBlock::D, ElementCategory::TransitionMetal },
    { 110, "Ds", "Darmstadtium", 281.0, false, 10, 7, ElementBlock::D, ElementCategory::TransitionMetal },
    { 111, "Rg", "Roentgenium", 282.0, false, 11, 7, ElementBlock::D, ElementCategory::TransitionMetal },
    { 112, "Cn", "Copernicium", 285.0, false, 12, 7, ElementBlock::D, ElementCategory::TransitionMetal },
    { 113, "Nh", "Nihonium", 286.0, false, 13, 7, ElementBlock::P, ElementCategory::PostTransitionMetal },
    { 114, "Fl", "Flerovium", 289.0, false, 14, 7, ElementBlock::P, ElementCategory::PostTransitionMetal },
    { 115, "Mc", "Moscovium", 290.0, false, 15, 7, ElementBlock::P, ElementCategory::PostTransitionMetal },
    { 116, "Lv", "Livermorium", 293.0, false, 16, 7, ElementBlock::P, ElementCategory::PostTransitionMetal },
    { 117, "Ts", "Tennessine", 294.0, false, 17, 7, ElementBlock::P, ElementCategory::Halogen },
    { 118, "Og", "Oganesson", 294.0, false, 18, 7, ElementBlock::P, ElementCategory::NobleGas },
        }};
    }

    std::span<const Element> AllElements() noexcept
    {
        return kElements;
    }

    const Element& GetElement(int atomicNumber)
    {
        if (atomicNumber < 1 || atomicNumber > kElementCount)
        {
            throw std::out_of_range("Atomic number out of range");
        }
        return kElements[static_cast<std::size_t>(atomicNumber - 1)];
    }

    const Element* FindElement(std::string_view symbol) noexcept
    {
        for (const Element& element : kElements)
        {
            if (element.symbol == symbol)
            {
                return &element;
            }
        }
        return nullptr;
    }
}
