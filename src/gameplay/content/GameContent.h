#pragma once

#include "foundation/ContentRegistry.h"
#include "gameplay/content/ContentTypes.h"

#include <filesystem>
#include <memory>
#include <string_view>

namespace Infinitesia
{
    // The gameplay content read from assets/gameplay/*.json: the items and the kinds of
    // objects of the world. Immutable once loaded; objects and items point into it, so it
    // is kept alive (shared) for as long as any of them exists.
    class GameContent
    {
    public:
        // Reads items.json and objects.json from `directory`. Throws ContentError.
        static std::shared_ptr<const GameContent> LoadFromDirectory(const std::filesystem::path& directory);

        GameContent(const GameContent&) = delete;
        GameContent& operator=(const GameContent&) = delete;

        const ContentRegistry<ItemDef>& Items() const noexcept { return m_items; }
        const ContentRegistry<ObjectKindDef>& ObjectKinds() const noexcept { return m_objectKinds; }
        // Throw ContentError for an unknown key.
        const ItemDef& GetItem(std::string_view key) const { return m_items.Require(key); }
        const ObjectKindDef& GetObjectKind(std::string_view key) const { return m_objectKinds.Require(key); }

    private:
        GameContent() = default;

        ContentRegistry<ItemDef> m_items;
        ContentRegistry<ObjectKindDef> m_objectKinds;
    };
}
