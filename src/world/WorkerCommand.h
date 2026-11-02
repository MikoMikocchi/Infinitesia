#pragma once

#include "world/Grid.h"
#include "world/World.h"

#include <optional>
#include <string>
#include <string_view>

namespace Infinitesia
{
    class WorkSystem;

    struct WorkerCommandOption
    {
        std::string id;
        std::string label;
        bool enabled;
        std::string disabledReason;
    };

    struct WorkerCommandResult
    {
        bool accepted;
        std::string message;
    };

    struct WorkerCommandRequest
    {
        std::string commandId;
        WorldObjectId actor;
        WorldObjectId target;
    };

    class WorkerCommand
    {
    public:
        virtual ~WorkerCommand() = default;

        virtual std::string_view GetId() const = 0;
        virtual std::string_view GetLabel() const = 0;
        virtual bool IsApplicable(const WorldObject& target) const = 0;
        virtual std::string GetDisabledReason(
            std::optional<WorldObjectId> actor,
            const WorldObject& target,
            const World& world,
            const Grid& grid,
            const WorkSystem& workSystem) const = 0;
        virtual WorkerCommandResult Execute(
            WorldObjectId actor,
            WorldObjectId target,
            World& world,
            const Grid& grid,
            WorkSystem& workSystem) const = 0;
    };
}
