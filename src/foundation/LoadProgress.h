#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <initializer_list>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace Infinitesia
{
    // Progress of a multi-step load, shared between a worker and the UI thread.
    //
    // The worker announces steps (BeginStep) and, optionally, how far it is inside
    // the current step (SetStepFraction). Each step's overall share of the bar is
    // its duration in the previous run, read from a profile file. Every run
    // measures the real step durations and writes them back (SaveProfile), so the
    // bar re-proportions itself after the code changes; no weights are hardcoded.
    //
    // Without a profile (first run) steps are weighted equally. Steps absent from
    // the profile get the average weight; steps in the profile that no longer run
    // are simply never reached. Either way the bar never moves backwards and
    // reaches 100% on Finish(); only that one run is less proportional.
    //
    // Threading: LoadProfile/SetDefaultPlan before the worker starts; BeginStep,
    // SetStepFraction and Finish from the worker only; GetFraction/GetLabel from
    // any thread; SaveProfile after the worker has finished.
    class LoadProgress
    {
    public:
        void SetDefaultPlan(std::initializer_list<const char*> stepIds)
        {
            m_expected.clear();
            for (const char* id : stepIds)
            {
                m_expected.push_back({ id, 1.0 });
            }
        }

        // Reads "<id> <seconds>" lines. Keeps the default plan if the file is
        // missing or unusable.
        void LoadProfile(const std::string& path)
        {
            std::ifstream file(path);
            if (!file)
            {
                return;
            }
            std::vector<Entry> loaded;
            std::string id;
            double seconds = 0.0;
            while (file >> id >> seconds)
            {
                loaded.push_back({ id, std::max(seconds, kMinimumSeconds) });
            }
            if (!loaded.empty())
            {
                m_expected = std::move(loaded);
            }
        }

        bool SaveProfile(const std::string& path) const
        {
            std::ofstream file(path, std::ios::trunc);
            if (!file)
            {
                return false;
            }
            for (const Entry& step : m_visited)
            {
                file << step.id << ' ' << step.seconds << '\n';
            }
            return static_cast<bool>(file);
        }

        void BeginStep(const char* id, const char* label)
        {
            CloseCurrentStep();
            m_visited.push_back({ id, 0.0 });
            m_stepStart = Clock::now();
            m_stepOpen = true;
            m_stepFraction = 0.0;
            {
                const std::lock_guard<std::mutex> lock(m_mutex);
                m_label = label;
            }
            Publish();
        }

        void SetStepFraction(double fraction)
        {
            m_stepFraction = std::clamp(fraction, 0.0, 1.0);
            Publish();
        }

        void Finish()
        {
            CloseCurrentStep();
            m_fraction.store(1.0f, std::memory_order_relaxed);
        }

        float GetFraction() const noexcept { return m_fraction.load(std::memory_order_relaxed); }

        std::string GetLabel() const
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            return m_label;
        }

    private:
        using Clock = std::chrono::steady_clock;
        static constexpr double kMinimumSeconds = 1.0e-4;

        struct Entry
        {
            std::string id;
            double seconds = 0.0; // expected (plan) or measured (visited)
        };

        double ExpectedSeconds(const std::string& id) const
        {
            for (const Entry& entry : m_expected)
            {
                if (entry.id == id)
                {
                    return entry.seconds;
                }
            }
            if (m_expected.empty())
            {
                return 1.0;
            }
            double sum = 0.0;
            for (const Entry& entry : m_expected)
            {
                sum += entry.seconds;
            }
            return sum / static_cast<double>(m_expected.size());
        }

        void CloseCurrentStep()
        {
            if (m_stepOpen)
            {
                m_visited.back().seconds = std::max(
                    std::chrono::duration<double>(Clock::now() - m_stepStart).count(),
                    kMinimumSeconds);
                m_stepOpen = false;
                m_stepFraction = 1.0;
            }
        }

        void Publish()
        {
            // Total = every planned step + any visited step the plan did not know.
            double total = 0.0;
            for (const Entry& entry : m_expected)
            {
                total += entry.seconds;
            }
            double done = 0.0;
            for (std::size_t i = 0; i < m_visited.size(); ++i)
            {
                const std::string& id = m_visited[i].id;
                const double weight = ExpectedSeconds(id);
                const bool planned = std::any_of(
                    m_expected.begin(), m_expected.end(),
                    [&](const Entry& entry) { return entry.id == id; });
                if (!planned)
                {
                    total += weight;
                }
                const bool isCurrent = m_stepOpen && i + 1 == m_visited.size();
                done += isCurrent ? weight * m_stepFraction : weight;
            }
            const float fraction = total > 0.0 ? static_cast<float>(done / total) : 0.0f;
            // Never move backwards (an unplanned step can enlarge the total mid-run).
            const float published = std::max(m_fraction.load(std::memory_order_relaxed),
                                             std::min(fraction, 0.999f));
            m_fraction.store(published, std::memory_order_relaxed);
        }

        std::vector<Entry> m_expected; // from profile / default plan (read-only while running)
        std::vector<Entry> m_visited;  // steps run so far with measured durations (worker only)
        Clock::time_point m_stepStart{};
        double m_stepFraction = 0.0;
        bool m_stepOpen = false;

        std::atomic<float> m_fraction{ 0.0f };
        mutable std::mutex m_mutex;
        std::string m_label;
    };

    // Null-safe helpers so generators can run without a progress sink.
    inline void BeginLoadStep(LoadProgress* progress, const char* id, const char* label)
    {
        if (progress != nullptr)
        {
            progress->BeginStep(id, label);
        }
    }
}
