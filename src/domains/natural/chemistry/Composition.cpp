#include "domains/natural/chemistry/Composition.h"

#include "domains/natural/chemistry/Element.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <stdexcept>

namespace Infinitesia::Chemistry
{
    namespace
    {
        constexpr int kMaxGroupDepth = 16;
        constexpr double kMaxCount = 1.0e6;

        struct FormulaError
        {
            std::string message;
            std::size_t position = 0;
        };

        // Recursive-descent parser for the grammar documented on Composition.
        class FormulaParser
        {
        public:
            explicit FormulaParser(std::string_view text) : m_text(text) {}

            std::vector<Composition::Entry> Parse()
            {
                std::vector<Composition::Entry> result;
                ParseSequence(result, 1.0, 0);
                while (ConsumeSeparator())
                {
                    SkipSpaces();
                    const double coefficient = AtNumber() ? ParseNumber() : 1.0;
                    ParseSequence(result, coefficient, 0);
                }
                SkipSpaces();
                if (m_pos != m_text.size())
                {
                    Fail("unexpected character");
                }
                return result;
            }

        private:
            [[noreturn]] void Fail(std::string message) const
            {
                throw FormulaError{ std::move(message), m_pos };
            }

            bool AtEnd() const { return m_pos >= m_text.size(); }
            char Peek() const { return AtEnd() ? '\0' : m_text[m_pos]; }

            void SkipSpaces()
            {
                while (Peek() == ' ')
                {
                    ++m_pos;
                }
            }

            static bool IsDigit(char c) { return c >= '0' && c <= '9'; }
            static bool IsUpper(char c) { return c >= 'A' && c <= 'Z'; }
            static bool IsLower(char c) { return c >= 'a' && c <= 'z'; }

            bool AtNumber() const { return IsDigit(Peek()); }

            // '*' or U+00B7 (UTF-8: C2 B7) between the parts of a hydrate.
            bool ConsumeSeparator()
            {
                SkipSpaces();
                if (Peek() == '*')
                {
                    ++m_pos;
                    return true;
                }
                if (m_pos + 1 < m_text.size() && static_cast<unsigned char>(m_text[m_pos]) == 0xC2 &&
                    static_cast<unsigned char>(m_text[m_pos + 1]) == 0xB7)
                {
                    m_pos += 2;
                    return true;
                }
                return false;
            }

            // digits [ '.' digits ]
            double ParseNumber()
            {
                const std::size_t start = m_pos;
                while (IsDigit(Peek()))
                {
                    ++m_pos;
                }
                if (Peek() == '.')
                {
                    ++m_pos;
                    if (!IsDigit(Peek()))
                    {
                        Fail("digits expected after the decimal point");
                    }
                    while (IsDigit(Peek()))
                    {
                        ++m_pos;
                    }
                }
                double value = 0.0;
                const char* first = m_text.data() + start;
                const char* last = m_text.data() + m_pos;
                const auto result = std::from_chars(first, last, value);
                if (result.ec != std::errc() || result.ptr != last || !std::isfinite(value))
                {
                    m_pos = start;
                    Fail("invalid number");
                }
                if (value <= 0.0 || value > kMaxCount)
                {
                    m_pos = start;
                    Fail("count must be greater than 0 and at most 1e6");
                }
                return value;
            }

            double ParseCount()
            {
                return AtNumber() ? ParseNumber() : 1.0;
            }

            // One or more groups; stops at a closing bracket, a separator or the end.
            // Appends the entries scaled by `scale`.
            void ParseSequence(std::vector<Composition::Entry>& out, double scale, int depth)
            {
                bool any = false;
                for (;;)
                {
                    SkipSpaces();
                    const char c = Peek();
                    if (IsUpper(c))
                    {
                        const std::size_t start = m_pos;
                        ++m_pos;
                        if (IsLower(Peek()))
                        {
                            ++m_pos;
                        }
                        const Element* element = FindElement(m_text.substr(start, m_pos - start));
                        if (element == nullptr)
                        {
                            m_pos = start;
                            Fail("unknown element symbol");
                        }
                        out.push_back({ element->atomicNumber, scale * ParseCount() });
                    }
                    else if (c == '(' || c == '[')
                    {
                        if (depth >= kMaxGroupDepth)
                        {
                            Fail("groups nested too deeply");
                        }
                        const char close = c == '(' ? ')' : ']';
                        ++m_pos;
                        std::vector<Composition::Entry> inner;
                        ParseSequence(inner, 1.0, depth + 1);
                        SkipSpaces();
                        if (Peek() != close)
                        {
                            Fail(std::string("expected '") + close + "'");
                        }
                        ++m_pos;
                        const double count = ParseCount();
                        for (const Composition::Entry& entry : inner)
                        {
                            out.push_back({ entry.atomicNumber, scale * count * entry.amount });
                        }
                    }
                    else
                    {
                        break;
                    }
                    any = true;
                }
                if (!any)
                {
                    Fail("element or group expected");
                }
            }

            std::string_view m_text;
            std::size_t m_pos = 0;
        };

        bool IsPositiveFinite(double value)
        {
            return std::isfinite(value) && value > 0.0;
        }
    }

    Composition Composition::FromRaw(std::vector<Entry> raw)
    {
        std::sort(raw.begin(), raw.end(),
            [](const Entry& a, const Entry& b) { return a.atomicNumber < b.atomicNumber; });

        Composition result;
        for (const Entry& entry : raw)
        {
            if (!result.m_entries.empty() && result.m_entries.back().atomicNumber == entry.atomicNumber)
            {
                result.m_entries.back().amount += entry.amount;
            }
            else
            {
                result.m_entries.push_back(entry);
            }
        }
        std::erase_if(result.m_entries, [](const Entry& entry) { return !(entry.amount > 0.0); });
        return result;
    }

    bool Composition::TryParse(std::string_view formula, Composition& out, std::string* error)
    {
        try
        {
            out = FromRaw(FormulaParser(formula).Parse());
            return true;
        }
        catch (const FormulaError& failure)
        {
            if (error != nullptr)
            {
                *error = "Invalid formula \"" + std::string(formula) + "\": " + failure.message +
                         " at position " + std::to_string(failure.position);
            }
            return false;
        }
    }

    Composition Composition::Parse(std::string_view formula)
    {
        Composition result;
        std::string error;
        if (!TryParse(formula, result, &error))
        {
            throw std::invalid_argument(error);
        }
        return result;
    }

    double Composition::Amount(int atomicNumber) const noexcept
    {
        for (const Entry& entry : m_entries)
        {
            if (entry.atomicNumber == atomicNumber)
            {
                return entry.amount;
            }
        }
        return 0.0;
    }

    double Composition::AtomCount() const noexcept
    {
        double total = 0.0;
        for (const Entry& entry : m_entries)
        {
            total += entry.amount;
        }
        return total;
    }

    double Composition::MolarMass() const
    {
        double mass = 0.0;
        for (const Entry& entry : m_entries)
        {
            mass += entry.amount * GetElement(entry.atomicNumber).atomicMass;
        }
        return mass;
    }

    double Composition::MassFraction(int atomicNumber) const
    {
        const double total = MolarMass();
        if (!(total > 0.0))
        {
            return 0.0;
        }
        return Amount(atomicNumber) * GetElement(atomicNumber).atomicMass / total;
    }

    ElementMassFractions Composition::MassFractions() const
    {
        const double total = MolarMass();
        std::vector<ElementMassFractions::Entry> entries;
        if (total > 0.0)
        {
            entries.reserve(m_entries.size());
            for (const Entry& entry : m_entries)
            {
                entries.push_back({ entry.atomicNumber, entry.amount * GetElement(entry.atomicNumber).atomicMass / total });
            }
        }
        return ElementMassFractions(std::move(entries));
    }

    Composition Composition::Scaled(double factor) const
    {
        if (!IsPositiveFinite(factor))
        {
            throw std::invalid_argument("Composition scale factor must be positive and finite");
        }
        Composition result = *this;
        for (Entry& entry : result.m_entries)
        {
            entry.amount *= factor;
        }
        return result;
    }

    Composition& Composition::operator+=(const Composition& other)
    {
        std::vector<Entry> raw = m_entries;
        raw.insert(raw.end(), other.m_entries.begin(), other.m_entries.end());
        *this = FromRaw(std::move(raw));
        return *this;
    }

    Composition Blend(std::span<const CompositionPart> parts)
    {
        Composition total;
        for (const CompositionPart& part : parts)
        {
            if (!std::isfinite(part.moles) || part.moles < 0.0)
            {
                throw std::invalid_argument("Blend amounts must be finite and not negative");
            }
            if (part.moles > 0.0)
            {
                total += part.composition.Scaled(part.moles);
            }
        }
        return total;
    }

    double ElementMassFractions::Fraction(int atomicNumber) const noexcept
    {
        for (const Entry& entry : m_entries)
        {
            if (entry.atomicNumber == atomicNumber)
            {
                return entry.fraction;
            }
        }
        return 0.0;
    }

    double ElementMassFractions::Sum() const noexcept
    {
        double total = 0.0;
        for (const Entry& entry : m_entries)
        {
            total += entry.fraction;
        }
        return total;
    }

    ElementMassFractions Mix(std::span<const MassPart> parts)
    {
        double totalWeight = 0.0;
        for (const MassPart& part : parts)
        {
            if (!std::isfinite(part.weight) || part.weight < 0.0)
            {
                throw std::invalid_argument("Mix weights must be finite and not negative");
            }
            if (part.weight > 0.0 && part.fractions.Empty())
            {
                throw std::invalid_argument("Mix part with weight has no composition");
            }
            totalWeight += part.weight;
        }
        if (!(totalWeight > 0.0))
        {
            throw std::invalid_argument("Mix needs a positive total weight");
        }

        std::vector<ElementMassFractions::Entry> raw;
        for (const MassPart& part : parts)
        {
            for (const ElementMassFractions::Entry& entry : part.fractions.Entries())
            {
                raw.push_back({ entry.atomicNumber, entry.fraction * part.weight / totalWeight });
            }
        }
        std::sort(raw.begin(), raw.end(),
            [](const ElementMassFractions::Entry& a, const ElementMassFractions::Entry& b)
            { return a.atomicNumber < b.atomicNumber; });

        std::vector<ElementMassFractions::Entry> merged;
        for (const ElementMassFractions::Entry& entry : raw)
        {
            if (!merged.empty() && merged.back().atomicNumber == entry.atomicNumber)
            {
                merged.back().fraction += entry.fraction;
            }
            else
            {
                merged.push_back(entry);
            }
        }
        return ElementMassFractions(std::move(merged));
    }
}
