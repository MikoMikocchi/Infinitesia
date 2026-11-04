#include "blocks/Tag.h"

namespace Infinitesia
{
    namespace
    {
        constexpr std::string_view kNames[kEngineTagCount] = {
            "air", "fluid", "water", "soil", "rock", "ice", "built", "glass", "light_source", "plant", "tree_wood", "tree_leaves",
        };
    }

    std::string_view TagName(Tag tag) noexcept
    {
        return kNames[static_cast<int>(tag)];
    }

    std::optional<Tag> TagFromName(std::string_view name) noexcept
    {
        for (int i = 0; i < kEngineTagCount; ++i)
        {
            if (kNames[i] == name)
            {
                return static_cast<Tag>(i);
            }
        }
        return std::nullopt;
    }
}
