#pragma once

#include <string>
#include <utility>

namespace Infinitesia
{
    // The stable string key of a simulation system ("system.work"); saves and logs refer to a
    // system by it.
    class SystemId
    {
    public:
        SystemId() = default;
        explicit SystemId(std::string key)
            : m_key(std::move(key))
        {
        }

        const std::string& Key() const { return m_key; }
        bool IsEmpty() const { return m_key.empty(); }

        friend bool operator==(const SystemId&, const SystemId&) = default;
        friend auto operator<=>(const SystemId&, const SystemId&) = default;

    private:
        std::string m_key;
    };
}
