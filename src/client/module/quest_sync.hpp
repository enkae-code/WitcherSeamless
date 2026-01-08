#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <optional>
#include <functional>

namespace quest_sync
{
    // ===========================================================================
    // NARRATIVE SYNCHRONIZATION SYSTEM
    // ===========================================================================
    // Prevents world-state divergence by ensuring all players experience the same
    // quest progression, dialogue choices, and cinematic events.
    // Global story lock prevents local actions during primary player cutscenes.
    // ===========================================================================

    constexpr uint32_t NARRATIVE_PROXIMITY_RADIUS = 30; // Meters for dialogue teleportation
    constexpr size_t MAX_FACT_NAME_LENGTH = 128;
    constexpr size_t FACT_CACHE_SIZE_LIMIT = 1024; // Maximum cached facts to prevent bloat

    // ---------------------------------------------------------------------------
    // QUEST FACT STRUCTURE
    // ---------------------------------------------------------------------------
    // Represents a synchronized world fact for quest progression

    struct quest_fact
    {
        std::string fact_name{};
        int32_t value{0};
        uint64_t timestamp{0};
        uint64_t player_guid{0}; // Player who triggered this fact
        uint32_t fact_hash{0};   // Hash for fast comparison
    };

    // ---------------------------------------------------------------------------
    // NARRATIVE EVENT TYPES
    // ---------------------------------------------------------------------------

    enum class narrative_event_type : uint8_t
    {
        dialogue_start = 0,  // Player entered dialogue
        dialogue_end = 1,    // Player exited dialogue
        cutscene_start = 2,  // Cutscene playback started
        cutscene_end = 3,    // Cutscene playback ended
        quest_objective = 4, // Quest objective updated
        fact_changed = 5     // World fact changed
    };

    // ---------------------------------------------------------------------------
    // GLOBAL STORY LOCK MANAGER
    // ---------------------------------------------------------------------------
    // Controls player movement and interaction during narrative events
    // Zero-Bloat Philosophy: Atomic flags, no dynamic allocations

    class global_story_lock
    {
      public:
        global_story_lock() = default;

        // -----------------------------------------------------------------------
        // LOCK MANAGEMENT
        // -----------------------------------------------------------------------

        void acquire_lock(uint64_t initiator_guid, uint32_t scene_id)
        {
            if (is_locked())
            {
                return;
            }

            m_lock_active.store(true);
            m_initiator_guid.store(initiator_guid);
            m_scene_id.store(scene_id);
            m_lock_timestamp.store(std::chrono::high_resolution_clock::now().time_since_epoch().count());

            printf("[W3MP NARRATIVE] Story lock ACQUIRED: Initiator=%llu, Scene=%u\n", initiator_guid, scene_id);
        }

        void release_lock()
        {
            if (!is_locked())
            {
                return;
            }

            const auto initiator = m_initiator_guid.load();
            const auto scene = m_scene_id.load();

            m_lock_active.store(false);
            m_initiator_guid.store(0);
            m_scene_id.store(0);

            printf("[W3MP NARRATIVE] Story lock RELEASED: Initiator=%llu, Scene=%u\n", initiator, scene);
        }

        bool is_locked() const
        {
            return m_lock_active.load();
        }

        uint64_t get_initiator_guid() const
        {
            return m_initiator_guid.load();
        }

        uint32_t get_scene_id() const
        {
            return m_scene_id.load();
        }

        uint64_t get_lock_timestamp() const
        {
            return m_lock_timestamp.load();
        }

      private:
        std::atomic<bool> m_lock_active{false};
        std::atomic<uint64_t> m_initiator_guid{0};
        std::atomic<uint32_t> m_scene_id{0};
        std::atomic<uint64_t> m_lock_timestamp{0};
    };

    // ---------------------------------------------------------------------------
    // QUEST FACT MANAGER
    // ---------------------------------------------------------------------------
    // Handles atomic fact synchronization and world-state consistency
    // Thread-safe with minimal locking

    class quest_fact_manager
    {
      public:
        quest_fact_manager() = default;

        // -----------------------------------------------------------------------
        // FACT REGISTRATION
        // -----------------------------------------------------------------------

        void register_fact(const std::string& fact_name, int32_t value, uint64_t player_guid)
        {
            std::lock_guard<std::mutex> lock(m_fact_mutex);

            const auto fact_hash = compute_fact_hash(fact_name);

            quest_fact fact{};
            fact.fact_name = fact_name;
            fact.value = value;
            fact.timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            fact.player_guid = player_guid;
            fact.fact_hash = fact_hash;

            m_fact_cache[fact_hash] = fact;

            if (m_fact_cache.size() > FACT_CACHE_SIZE_LIMIT)
            {
                prune_oldest_facts();
            }

            printf("[W3MP NARRATIVE] Fact registered: %s = %d (hash: %u, player: %llu)\n", fact_name.c_str(), value, fact_hash,
                   player_guid);
        }

        // -----------------------------------------------------------------------
        // FACT RETRIEVAL
        // -----------------------------------------------------------------------

        std::optional<quest_fact> get_fact(const std::string& fact_name) const
        {
            std::lock_guard<std::mutex> lock(m_fact_mutex);

            const auto fact_hash = compute_fact_hash(fact_name);
            const auto it = m_fact_cache.find(fact_hash);

            if (it != m_fact_cache.end())
            {
                return it->second;
            }

            return std::nullopt;
        }

        std::optional<quest_fact> get_fact(uint32_t fact_hash) const
        {
            std::lock_guard<std::mutex> lock(m_fact_mutex);

            const auto it = m_fact_cache.find(fact_hash);

            if (it != m_fact_cache.end())
            {
                return it->second;
            }

            return std::nullopt;
        }

        // -----------------------------------------------------------------------
        // FACT EXISTENCE CHECK
        // -----------------------------------------------------------------------

        bool has_fact(const std::string& fact_name) const
        {
            return get_fact(fact_name).has_value();
        }

        bool has_fact(uint32_t fact_hash) const
        {
            return get_fact(fact_hash).has_value();
        }

        // -----------------------------------------------------------------------
        // FACT HASH COMPUTATION
        // -----------------------------------------------------------------------
        // Uses std::hash for consistent fact identification
        // Keeps packet sizes minimal (4 bytes instead of 128 bytes)

        static uint32_t compute_fact_hash(const std::string& fact_name)
        {
            return static_cast<uint32_t>(std::hash<std::string>{}(fact_name));
        }

        // -----------------------------------------------------------------------
        // WORLD STATE CONSISTENCY
        // -----------------------------------------------------------------------
        // Generates a hash of all critical facts for verification

        uint32_t compute_world_state_hash() const
        {
            std::lock_guard<std::mutex> lock(m_fact_mutex);

            uint32_t combined_hash = 0;

            for (const auto& [fact_hash, fact] : m_fact_cache)
            {
                combined_hash ^= fact_hash;
                combined_hash ^= static_cast<uint32_t>(fact.value);
            }

            return combined_hash;
        }

        // -----------------------------------------------------------------------
        // CACHE MANAGEMENT
        // -----------------------------------------------------------------------

        void clear_cache()
        {
            std::lock_guard<std::mutex> lock(m_fact_mutex);
            m_fact_cache.clear();

            printf("[W3MP NARRATIVE] Fact cache cleared\n");
        }

        size_t get_fact_count() const
        {
            std::lock_guard<std::mutex> lock(m_fact_mutex);
            return m_fact_cache.size();
        }

      private:
        // -----------------------------------------------------------------------
        // CACHE PRUNING
        // -----------------------------------------------------------------------
        // Removes oldest facts when cache exceeds size limit

        void prune_oldest_facts()
        {
            if (m_fact_cache.size() <= FACT_CACHE_SIZE_LIMIT * 0.75)
            {
                return;
            }

            std::vector<std::pair<uint32_t, uint64_t>> fact_ages;
            fact_ages.reserve(m_fact_cache.size());

            for (const auto& [hash, fact] : m_fact_cache)
            {
                fact_ages.emplace_back(hash, fact.timestamp);
            }

            std::sort(fact_ages.begin(), fact_ages.end(), [](const auto& a, const auto& b) { return a.second < b.second; });

            const size_t prune_count = m_fact_cache.size() - static_cast<size_t>(FACT_CACHE_SIZE_LIMIT * 0.75);

            for (size_t i = 0; i < prune_count && i < fact_ages.size(); ++i)
            {
                m_fact_cache.erase(fact_ages[i].first);
            }

            printf("[W3MP NARRATIVE] Pruned %zu old facts from cache\n", prune_count);
        }

        mutable std::mutex m_fact_mutex;
        std::unordered_map<uint32_t, quest_fact> m_fact_cache;
    };

    // ---------------------------------------------------------------------------
    // DIALOGUE PROXIMITY MANAGER
    // ---------------------------------------------------------------------------
    // Handles party member teleportation during narrative events

    class dialogue_proximity_manager
    {
      public:
        dialogue_proximity_manager() = default;

        // -----------------------------------------------------------------------
        // PROXIMITY CHECK
        // -----------------------------------------------------------------------

        static bool is_within_proximity(const float* player_pos, const float* initiator_pos, float radius)
        {
            if (!player_pos || !initiator_pos)
            {
                return false;
            }

            const float dx = player_pos[0] - initiator_pos[0];
            const float dy = player_pos[1] - initiator_pos[1];
            const float dz = player_pos[2] - initiator_pos[2];

            const float distance_sq = dx * dx + dy * dy + dz * dz;
            const float radius_sq = radius * radius;

            return distance_sq <= radius_sq;
        }

        // -----------------------------------------------------------------------
        // TELEPORTATION REQUEST
        // -----------------------------------------------------------------------

        void request_teleport(uint64_t player_guid, const float* target_position)
        {
            std::lock_guard<std::mutex> lock(m_teleport_mutex);

            teleport_request request{};
            request.player_guid = player_guid;
            request.target_position[0] = target_position[0];
            request.target_position[1] = target_position[1];
            request.target_position[2] = target_position[2];
            request.timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();

            m_pending_teleports[player_guid] = request;

            printf("[W3MP NARRATIVE] Teleport requested for player %llu to (%.2f, %.2f, %.2f)\n", player_guid, target_position[0],
                   target_position[1], target_position[2]);
        }

        bool has_pending_teleport(uint64_t player_guid) const
        {
            std::lock_guard<std::mutex> lock(m_teleport_mutex);
            return m_pending_teleports.contains(player_guid);
        }

        void clear_teleport(uint64_t player_guid)
        {
            std::lock_guard<std::mutex> lock(m_teleport_mutex);
            m_pending_teleports.erase(player_guid);
        }

      private:
        struct teleport_request
        {
            uint64_t player_guid{0};
            float target_position[3]{0.0f, 0.0f, 0.0f};
            uint64_t timestamp{0};
        };

        mutable std::mutex m_teleport_mutex;
        std::unordered_map<uint64_t, teleport_request> m_pending_teleports;
    };

    // ---------------------------------------------------------------------------
    // PUBLIC API
    // ---------------------------------------------------------------------------

    bool is_global_sync_active();
}
