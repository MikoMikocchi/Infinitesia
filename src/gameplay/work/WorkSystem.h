#pragma once

#include "world/Grid.h"
#include "gameplay/navigation/GridPathfinder.h"
#include "gameplay/GameWorld.h"
#include "simulation/api/SimulationSystem.h"
#include "simulation/api/SystemDescriptor.h"

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
        // A log is this much wood: a felled tree gives its mass (the density of its wood, a cubic metre a cell)
        // divided by this, at least one log.
        float kilogramsPerLog;
    };

    // What a felled tree leaves lying: a pile object that carries the logs. Without it (a null pointer) the
    // wood is gone without trace.
    struct WoodDrop
    {
        const ObjectKindDef* pile = nullptr;
        const ItemDef* logs = nullptr;
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

    // Moves workers along their orders and carries the orders out. Runs on the scheduler
    // (see `Descriptor`), once per tick, on the world and grid it was built with, which must
    // outlive it.
    class WorkSystem final : public SimulationSystem
    {
    public:
        static SystemDescriptor Descriptor()
        {
            return { SystemId("system.work"), SystemPhase::Update, Cadence::EveryTick() };
        }

        WorkSystem(WorkerConfig config, GameWorld& world, const Grid& grid, WoodDrop drop = {});

        void Update(const SystemContext& context) override;

        // Sends the worker to fell the tree that has a cell of wood at `treeCell`; he works from the ground beside its
        // trunk.
        WorkOrderResult AssignChopOrder(WorldObjectId worker, GridCell treeCell, GameWorld& world, const Grid& grid);
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
            GridCell targetCell; // for ChopTree a cell of the tree's wood
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
        bool HasOrderForTree(const std::vector<GridCell>& wood) const;
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

        void Tick(GameWorld& world, const Grid& grid, float fixedDeltaTime);
        // Fells the tree and leaves its logs as piles on the stump.
        void FellAndDropLogs(GameWorld& world, const Grid& grid, GridCell treeCell);

        WorkerConfig m_config;
        WoodDrop m_drop;
        GameWorld& m_world;
        const Grid& m_grid;
        GridPathfinder m_pathfinder;
        std::vector<WorkOrder> m_orders;
    };
}
