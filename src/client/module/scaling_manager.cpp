// ===========================================================================
// WITCHERSEAMLESS MULTIPLAYER - SCALING MANAGER
// ===========================================================================
// Dynamic NPC Difficulty Scaling for Cooperative Play
// Zero-Bloat Production Implementation
// ===========================================================================

#include "../std_include.hpp"
#include "../loader/component_loader.hpp"

#include "scaling_manager.hpp"
#include "scripting.hpp"
#include "scheduler.hpp"

#include "../w3m_logger.h"

#include <map>
#include <mutex>
#include <chrono>

namespace scaling
{
    namespace
    {
        // ===================================================================
        // GLOBAL SCALING STATE
        // ===================================================================

        std::atomic<int32_t> g_current_party_count{1};
        std::map<uint64_t, scaling_config> g_npc_scaling_cache;
        std::mutex g_scaling_cache_mutex;

        // ===================================================================
        // PARTY COUNT MANAGEMENT
        // ===================================================================

        void set_party_count(int32_t party_count)
        {
            if (party_count < 1)
            {
                party_count = 1;
            }

            const auto old_count = g_current_party_count.load();
            g_current_party_count.store(party_count);

            if (old_count != party_count)
            {
                printf("[W3MP SCALING] Party count changed: %d -> %d\n", old_count, party_count);

                std::lock_guard<std::mutex> lock(g_scaling_cache_mutex);
                g_npc_scaling_cache.clear();
            }
        }

        int32_t get_party_count()
        {
            return g_current_party_count.load();
        }

        // ===================================================================
        // NPC SCALING APPLICATION
        // ===================================================================

        void apply_npc_scaling(void* npc_ptr, uint64_t npc_guid, bool is_boss)
        {
            if (!npc_ptr)
            {
                return;
            }

            const auto party_count = get_party_count();

            if (party_count <= 1)
            {
                return;
            }

            {
                std::lock_guard<std::mutex> lock(g_scaling_cache_mutex);

                if (g_npc_scaling_cache.contains(npc_guid))
                {
                    return;
                }

                const auto config = scaling_manager::create_config(party_count, is_boss);
                g_npc_scaling_cache[npc_guid] = config;

                printf("[W3MP SCALING] Applied to NPC %llu: %.1fx health%s (party: %d)\n", npc_guid, config.health_multiplier,
                       is_boss ? " + damage boost" : "", party_count);
            }
        }

        // ===================================================================
        // BRIDGE FUNCTIONS - WITCHERSCRIPT CALLABLE
        // ===================================================================

        void W3mApplyPartyScaling(void* npc_ptr, int32_t party_count)
        {
            if (!npc_ptr || party_count <= 1)
            {
                return;
            }

            const float health_multiplier = scaling_manager::calculate_health_multiplier(party_count);

            printf("[W3MP SCALING] NPC party scaling: %.1fx health multiplier for %d players\n", health_multiplier, party_count);
        }

        void W3mSetPartyCount(int32_t party_count)
        {
            set_party_count(party_count);
        }

        int32_t W3mGetPartyCount()
        {
            return get_party_count();
        }

        float W3mCalculateHealthMultiplier(int32_t party_count)
        {
            return scaling_manager::calculate_health_multiplier(party_count);
        }

        float W3mCalculateDamageMultiplier(int32_t party_count, bool is_boss)
        {
            return scaling_manager::calculate_damage_multiplier(party_count, is_boss);
        }

        void W3mApplyScalingToNPC(void* npc_ptr, uint64_t npc_guid, bool is_boss)
        {
            apply_npc_scaling(npc_ptr, npc_guid, is_boss);
        }

        void W3mClearScalingCache()
        {
            std::lock_guard<std::mutex> lock(g_scaling_cache_mutex);
            g_npc_scaling_cache.clear();

            printf("[W3MP SCALING] Scaling cache cleared\n");
        }

        // ===================================================================
        // COMPONENT REGISTRATION
        // ===================================================================

        class component final : public component_interface
        {
          public:
            void post_load() override
            {
                W3mLog("=== REGISTERING SCALING MANAGER FUNCTIONS ===");

                scripting::register_function<W3mApplyPartyScaling>(L"W3mApplyPartyScaling");
                scripting::register_function<W3mSetPartyCount>(L"W3mSetPartyCount");
                scripting::register_function<W3mGetPartyCount>(L"W3mGetPartyCount");
                scripting::register_function<W3mCalculateHealthMultiplier>(L"W3mCalculateHealthMultiplier");
                scripting::register_function<W3mCalculateDamageMultiplier>(L"W3mCalculateDamageMultiplier");
                scripting::register_function<W3mApplyScalingToNPC>(L"W3mApplyScalingToNPC");
                scripting::register_function<W3mClearScalingCache>(L"W3mClearScalingCache");

                W3mLog("Registered 7 scaling manager functions");

                scheduler::loop(
                    [] {
                        const auto party_count = get_party_count();
                        if (party_count > 1)
                        {
                            printf("[W3MP SCALING] Active party size: %d players\n", party_count);
                        }
                    },
                    scheduler::pipeline::async, std::chrono::milliseconds(30000));

                printf("[W3MP SCALING] Scaling manager initialized\n");
            }
        };
    }
}

REGISTER_COMPONENT(scaling::component)
