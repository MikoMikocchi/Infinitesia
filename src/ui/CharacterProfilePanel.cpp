#include "ui/CharacterProfilePanel.h"

#include <algorithm>
#include <cstdio>

namespace Infinitesia
{
    namespace
    {
        constexpr float kPanelWidth = 460.0f;
        constexpr float kMinimumPanelHeight = 230.0f;
        constexpr float kSlotSize = 90.0f;
        constexpr float kSlotGap = 12.0f;
        constexpr float kSlotStartX = 24.0f;
        constexpr float kSlotStartY = 116.0f;
        constexpr float kSlotRowGap = 12.0f;
        constexpr float kPanelBottomPadding = 24.0f;
        constexpr std::size_t kSlotColumnCount = 4;
        constexpr Color kPanelColor = { 34, 39, 45, 250 };
        constexpr Color kPanelBorderColor = { 115, 125, 135, 255 };
        constexpr Color kSlotColor = { 53, 60, 67, 255 };
        constexpr Color kSlotBorderColor = { 125, 135, 145, 255 };
        constexpr Color kTextColor = { 240, 240, 240, 255 };
        constexpr Color kMutedTextColor = { 170, 180, 190, 255 };
        constexpr Color kLogColor = { 145, 95, 55, 255 };
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
        int screenWidth,
        int screenHeight) const
    {
        if (!IsOpen())
        {
            return;
        }

        const std::size_t slotCount = character.inventory.GetSlots().size();
        const Rectangle bounds = GetBounds(screenWidth, screenHeight, slotCount);
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.48f));
        DrawRectangleRec(bounds, kPanelColor);
        DrawRectangleLinesEx(bounds, 2.0f, kPanelBorderColor);

        char title[64];
        std::snprintf(
            title,
            sizeof(title),
            "Worker #%llu",
            static_cast<unsigned long long>(character.id.value));
        DrawText(title, static_cast<int>(bounds.x + 24.0f), static_cast<int>(bounds.y + 20.0f), 24, kTextColor);
        DrawText(
            "Character profile",
            static_cast<int>(bounds.x + 24.0f),
            static_cast<int>(bounds.y + 52.0f),
            18,
            kMutedTextColor);
        DrawText(
            "X",
            static_cast<int>(GetCloseButtonBounds(screenWidth, screenHeight, slotCount).x + 11.0f),
            static_cast<int>(bounds.y + 17.0f),
            22,
            kTextColor);
        DrawText(
            "Inventory",
            static_cast<int>(bounds.x + 24.0f),
            static_cast<int>(bounds.y + 84.0f),
            18,
            kTextColor);

        const auto& slots = character.inventory.GetSlots();
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
            DrawRectangle(
                static_cast<int>(slotBounds.x + 28.0f),
                static_cast<int>(slotBounds.y + 12.0f),
                34,
                20,
                item.GetType() == ItemType::Logs ? kLogColor : GOLD);
            DrawText(
                item.GetType() == ItemType::Logs ? "Logs" : "Item",
                static_cast<int>(slotBounds.x + 8.0f),
                static_cast<int>(slotBounds.y + 42.0f),
                16,
                kTextColor);
            char quantity[16];
            std::snprintf(quantity, sizeof(quantity), "%d", item.GetQuantity());
            DrawText(
                quantity,
                static_cast<int>(slotBounds.x + 64.0f),
                static_cast<int>(slotBounds.y + 42.0f),
                16,
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
            panelBounds.x + panelBounds.width - 40.0f,
            panelBounds.y + 8.0f,
            32.0f,
            32.0f
        };
    }
}
