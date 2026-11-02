#include "ui/CharacterProfilePanel.h"

#include <algorithm>
#include <cstdio>

namespace Infinitesia
{
    namespace
    {
        constexpr float kPanelWidth = 360.0f;
        constexpr float kMinimumPanelHeight = 184.0f;
        constexpr float kSlotSize = 68.0f;
        constexpr float kSlotGap = 8.0f;
        constexpr float kSlotStartX = 16.0f;
        constexpr float kSlotStartY = 90.0f;
        constexpr float kSlotRowGap = 8.0f;
        constexpr float kPanelBottomPadding = 14.0f;
        constexpr std::size_t kSlotColumnCount = 4;
        constexpr float kItemIconSize = 46.0f;
        constexpr Color kPanelColor = { 34, 39, 45, 250 };
        constexpr Color kPanelBorderColor = { 115, 125, 135, 255 };
        constexpr Color kSlotColor = { 53, 60, 67, 255 };
        constexpr Color kSlotBorderColor = { 125, 135, 145, 255 };
        constexpr Color kTextColor = { 240, 240, 240, 255 };
        constexpr Color kMutedTextColor = { 170, 180, 190, 255 };
        constexpr Color kTooltipColor = { 24, 28, 33, 245 };
        constexpr Color kTooltipBorderColor = { 105, 115, 125, 255 };
    }

    void CharacterProfilePanel::Open(WorldObjectId character)
    {
        m_character = character;
    }

    void CharacterProfilePanel::Close()
    {
        m_character.reset();
    }

    bool CharacterProfilePanel::IsOpen() const
    {
        return m_character.has_value();
    }

    WorldObjectId CharacterProfilePanel::GetCharacter() const
    {
        return *m_character;
    }

    bool CharacterProfilePanel::Update(
        Vector2 mousePosition,
        bool leftClickPressed,
        bool escapePressed,
        int screenWidth,
        int screenHeight,
        std::size_t slotCount)
    {
        if (!IsOpen())
        {
            return false;
        }

        if (escapePressed ||
            (leftClickPressed &&
             (!CheckCollisionPointRec(mousePosition, GetBounds(screenWidth, screenHeight, slotCount)) ||
              CheckCollisionPointRec(
                  mousePosition,
                  GetCloseButtonBounds(screenWidth, screenHeight, slotCount)))))
        {
            Close();
        }
        return true;
    }

    void CharacterProfilePanel::Draw(
        const WorldObject& character,
        const TextureAssetManager& assets,
        int screenWidth,
        int screenHeight) const
    {
        if (!IsOpen())
        {
            return;
        }

        const std::size_t slotCount = character.inventory.GetSlots().size();
        const Rectangle bounds = GetBounds(screenWidth, screenHeight, slotCount);
        DrawRectangleRec(bounds, kPanelColor);
        DrawRectangleLinesEx(bounds, 2.0f, kPanelBorderColor);

        char title[64];
        std::snprintf(
            title,
            sizeof(title),
            "Worker #%llu",
            static_cast<unsigned long long>(character.id.value));
        DrawText(title, static_cast<int>(bounds.x + 16.0f), static_cast<int>(bounds.y + 12.0f), 20, kTextColor);
        DrawText(
            "Character profile",
            static_cast<int>(bounds.x + 16.0f),
            static_cast<int>(bounds.y + 38.0f),
            14,
            kMutedTextColor);
        DrawText(
            "X",
            static_cast<int>(GetCloseButtonBounds(screenWidth, screenHeight, slotCount).x + 10.0f),
            static_cast<int>(bounds.y + 8.0f),
            18,
            kTextColor);
        DrawText(
            "Inventory",
            static_cast<int>(bounds.x + 16.0f),
            static_cast<int>(bounds.y + 64.0f),
            16,
            kTextColor);

        const auto& slots = character.inventory.GetSlots();
        const Vector2 mousePosition = GetMousePosition();
        const char* hoveredItemName = nullptr;
        for (std::size_t index = 0; index < slots.size(); ++index)
        {
            const std::size_t column = index % kSlotColumnCount;
            const std::size_t row = index / kSlotColumnCount;
            const Rectangle slotBounds{
                bounds.x + kSlotStartX + static_cast<float>(column) * (kSlotSize + kSlotGap),
                bounds.y + kSlotStartY + static_cast<float>(row) * (kSlotSize + kSlotRowGap),
                kSlotSize,
                kSlotSize
            };
            DrawRectangleRec(slotBounds, kSlotColor);
            DrawRectangleLinesEx(slotBounds, 1.0f, kSlotBorderColor);

            if (!slots[index].has_value())
            {
                continue;
            }

            const Item& item = *slots[index];
            if (item.GetType() == ItemType::Logs)
            {
                const Texture2D& texture = assets.GetTexture("items/Log");
                const float scale = std::min(
                    kItemIconSize / static_cast<float>(texture.width),
                    kItemIconSize / static_cast<float>(texture.height));
                const float iconWidth = static_cast<float>(texture.width) * scale;
                const float iconHeight = static_cast<float>(texture.height) * scale;
                DrawTexturePro(
                    texture,
                    { 0.0f, 0.0f, static_cast<float>(texture.width), static_cast<float>(texture.height) },
                    {
                        slotBounds.x + (slotBounds.width - iconWidth) * 0.5f,
                        slotBounds.y + (slotBounds.height - iconHeight) * 0.5f,
                        iconWidth,
                        iconHeight
                    },
                    { 0.0f, 0.0f },
                    0.0f,
                    WHITE);
            }

            char quantity[16];
            std::snprintf(quantity, sizeof(quantity), "%d", item.GetQuantity());
            const int quantityWidth = MeasureText(quantity, 14);
            const int quantityX = static_cast<int>(
                slotBounds.x + slotBounds.width - static_cast<float>(quantityWidth) - 5.0f);
            const int quantityY = static_cast<int>(slotBounds.y + slotBounds.height - 19.0f);
            DrawText(
                quantity,
                quantityX + 1,
                quantityY + 1,
                14,
                { 0, 0, 0, 220 });
            DrawText(
                quantity,
                quantityX,
                quantityY,
                14,
                kTextColor);

            if (CheckCollisionPointRec(mousePosition, slotBounds))
            {
                hoveredItemName = item.GetType() == ItemType::Logs ? "Logs" : "Item";
            }
        }

        if (hoveredItemName != nullptr)
        {
            constexpr int kTooltipFontSize = 16;
            constexpr float kTooltipPaddingX = 8.0f;
            constexpr float kTooltipPaddingY = 6.0f;
            const float tooltipWidth =
                static_cast<float>(MeasureText(hoveredItemName, kTooltipFontSize)) +
                kTooltipPaddingX * 2.0f;
            const float tooltipHeight =
                static_cast<float>(kTooltipFontSize) + kTooltipPaddingY * 2.0f;
            const float tooltipX = std::clamp(
                mousePosition.x + 14.0f,
                4.0f,
                std::max(4.0f, static_cast<float>(screenWidth) - tooltipWidth - 4.0f));
            const float tooltipY = std::clamp(
                mousePosition.y + 14.0f,
                4.0f,
                std::max(4.0f, static_cast<float>(screenHeight) - tooltipHeight - 4.0f));
            const Rectangle tooltipBounds{
                tooltipX,
                tooltipY,
                tooltipWidth,
                tooltipHeight
            };
            DrawRectangleRec(tooltipBounds, kTooltipColor);
            DrawRectangleLinesEx(tooltipBounds, 1.0f, kTooltipBorderColor);
            DrawText(
                hoveredItemName,
                static_cast<int>(tooltipX + kTooltipPaddingX),
                static_cast<int>(tooltipY + kTooltipPaddingY),
                kTooltipFontSize,
                kTextColor);
        }
    }

    Rectangle CharacterProfilePanel::GetBounds(
        int screenWidth,
        int screenHeight,
        std::size_t slotCount) const
    {
        const std::size_t rowCount = std::max(
            std::size_t{ 1 },
            (slotCount + kSlotColumnCount - 1) / kSlotColumnCount);
        const float contentHeight = kSlotStartY +
            static_cast<float>(rowCount) * kSlotSize +
            static_cast<float>(rowCount - 1) * kSlotRowGap +
            kPanelBottomPadding;
        const float panelHeight = std::max(kMinimumPanelHeight, contentHeight);
        return {
            (static_cast<float>(screenWidth) - kPanelWidth) * 0.5f,
            (static_cast<float>(screenHeight) - panelHeight) * 0.5f,
            kPanelWidth,
            panelHeight
        };
    }

    Rectangle CharacterProfilePanel::GetCloseButtonBounds(
        int screenWidth,
        int screenHeight,
        std::size_t slotCount) const
    {
        const Rectangle panelBounds = GetBounds(screenWidth, screenHeight, slotCount);
        return {
            panelBounds.x + panelBounds.width - 32.0f,
            panelBounds.y + 4.0f,
            24.0f,
            24.0f
        };
    }
}
