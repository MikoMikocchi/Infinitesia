#pragma once

#include "world/Grid.h"
#include "gameplay/navigation/GridPathfinder.h"
#include "gameplay/GameWorld.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace Infinitesia
{
    struct WorkerConfig
    {
        float movementSpeed;
        float choppingDuration;
        float diggingDuration;
        int logsPerTree;
    };

    enum class WorkOrderResult
    {
        Accepted,
        InvalidWorker,
        InvalidTarget,
        WorkerBusy,
        TargetBusy,
        NoReachableWorkCell
    };

    enum class WorkerActivity
    {
        Idle,
        Moving,
        Working
    };

    class WorkSystem
    {
    public:
        explicit WorkSystem(WorkerConfig config);

        WorkOrderResult AssignChopOrder(WorldObjectId worker, WorldObjectId tree, GameWorld& world, const Grid& grid);
        WorkOrderResult AssignDigOrder(
            WorldObjectId worker,
            GridCell target,
            GameWorld& world,
            const Grid& grid);
        WorkOrderResult AssignMoveOrder(
            WorldObjectId worker,
            GridCell target,
            GameWorld& world,
            const Grid& grid);
        void Tick(GameWorld& world, const Grid& grid, float fixedDeltaTime);
        WorkerActivity GetActivity(WorldObjectId worker) const;
        float GetWorkProgress(WorldObjectId worker) const;
        std::vector<GridCell> GetRemainingPath(WorldObjectId worker) const;

        friend class WorkSystemHelper;

    public:
        enum class WorkPhase
        {
            Moving,
            Working
        };

        enum class WorkType
        {
            ChopTree,
            DigBlock,
            MoveToCell
        };

        struct WorkOrder
        {
            WorkType type;
            WorldObjectId worker;
            std::optional<WorldObjectId> targetObject;
            GridCell targetCell;
            GridCell workCell;
            std::vector<GridCell> path;
            std::size_t nextPathCell = 1;
            WorkPhase phase;
            float workProgress = 0.0f;
        };

        float GetMovementSpeed() const { return m_config.movementSpeed; }
        const WorkerConfig& GetConfig() const { return m_config; }
        GridPathfinder& GetPathfinder() { return m_pathfinder; }
        std::vector<WorkOrder>& GetOrders() { return m_orders; }
        const std::vector<WorkOrder>& GetOrders() const { return m_orders; }

    private:
        bool HasOrderForWorker(WorldObjectId worker) const;
        bool HasOrderForTree(WorldObjectId tree) const;
        bool HasOrderForDigCell(GridCell cell) const;
        float GetWorkDuration(const WorkOrder& order) const;
        bool IsReserved(GridCell cell) const;
        std::optional<std::vector<GridCell>> FindPath(
            GridCell start,
            GridCell goal,
            WorldObjectId worker,
            const GameWorld& world,
            const Grid& grid) const;
        bool AdvanceOrder(WorkOrder& order, GameWorld& world, const Grid& grid, float fixedDeltaTime);
        bool AdvanceMovement(WorkOrder& order, GameWorld& world, const Grid& grid, float fixedDeltaTime);

        WorkerConfig m_config;
        GridPathfinder m_pathfinder;
        std::vector<WorkOrder> m_orders;
    };
}
