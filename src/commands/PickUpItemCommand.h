#pragma once

#include "commands/Command.h"

namespace Infinitesia
{
    class PickUpItemCommand final : public Command
    {
    public:
        std::string_view GetId() const override;
        std::string_view GetLabel() const override;
        bool IsApplicable(const WorldObject& target) const override;
        std::string GetDisabledReason(
            std::optional<WorldObjectId> actor,
            const WorldObject& target,
            const World& world) const override;
        CommandResult Execute(
            std::optional<WorldObjectId> actor,
            WorldObjectId target,
            World& world) const override;
    };
}
