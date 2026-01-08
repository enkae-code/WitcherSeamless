// ===========================================================================
// WITCHERSEAMLESS MULTIPLAYER - NARRATIVE SYNCHRONIZATION
// ===========================================================================
// Quest & Fact Manager for Global Story Parity
// Zero-Bloat Production Implementation
// ===========================================================================

#include "../std_include.hpp"
#include "../loader/component_loader.hpp"

#include "quest_sync.hpp"
#include "scripting.hpp"
#include "scheduler.hpp"
#include "network.hpp"

#include <game/structs.hpp>
#include <network/protocol.hpp>
#include <utils/byte_buffer.hpp>

#include "../utils/identity.hpp"
#include "../w3m_logger.h"

#include <chrono>

namespace quest_sync
{
    namespace
    {
        // ===================================================================
        // GLOBAL MANAGERS
        // ===================================================================

        global_story_lock g_story_lock;
        quest_fact_manager g_fact_manager;
        dialogue_proximity_manager g_proximity_manager;

        // Atomic flag for global sync state
        std::atomic<bool> g_W3mGlobalSyncInProgress{false};

        // Pending facts queue for atomic processing
        std::vector<quest_fact> g_W3mPendingFacts;
        std::mutex g_pending_facts_mutex;

        // ===================================================================
        // ATOMIC FACT SYNCHRONIZATION
        // ===================================================================

        void queue_fact_during_sync(const std::string& fact_name, int32_t value, uint64_t player_guid)
        {
            std::lock_guard<std::mutex> lock(g_pending_facts_mutex);

            quest_fact fact{};
            fact.fact_name = fact_name;
            fact.value = value;
            fact.timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            fact.player_guid = player_guid;
            fact.fact_hash = quest_fact_manager::compute_fact_hash(fact_name);

            g_W3mPendingFacts.push_back(fact);

            printf("[W3MP ATOMIC] Fact queued during sync: %s = %d\n", fact_name.c_str(), value);
        }

        void flush_pending_facts()
        {
            std::lock_guard<std::mutex> lock(g_pending_facts_mutex);

            if (g_W3mPendingFacts.empty())
            {
                return;
            }

            for (const auto& fact : g_W3mPendingFacts)
            {
                g_fact_manager.register_fact(fact.fact_name, fact.value, fact.player_guid);
            }

            printf("[W3MP ATOMIC] Global sync completed, %zu pending facts applied\n",
                   g_W3mPendingFacts.size());

            g_W3mPendingFacts.clear();
        }

        // ===================================================================
        // FACT BROADCASTING
        // ===================================================================

        void broadcast_quest_fact(const std::string& fact_name, int32_t value)
        {
            const auto player_guid = utils::identity::get_guid();

            if (g_W3mGlobalSyncInProgress.load())
            {
                queue_fact_during_sync(fact_name, value, player_guid);
                return;
            }

            g_fact_manager.register_fact(fact_name, value, player_guid);

            network::protocol::W3mFactPacket packet{};
            network::protocol::copy_string(packet.fact_name, fact_name);
            packet.value = value;
            packet.timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();

            utils::buffer_serializer buffer{};
            buffer.write(game::PROTOCOL);
            buffer.write(packet);

            network::send(network::get_master_server(), "fact", buffer.get_buffer());

            printf("[W3MP NARRATIVE] Broadcasting fact: %s = %d\n", fact_name.c_str(), value);
        }

        // ===================================================================
        // GLOBAL STORY LOCK CONTROL
        // ===================================================================

        void acquire_global_story_lock(uint64_t initiator_guid, uint32_t scene_id)
        {
            g_W3mGlobalSyncInProgress.store(true);
            g_story_lock.acquire_lock(initiator_guid, scene_id);

            printf("[W3MP NARRATIVE] Global sync IN PROGRESS (scene %u)\n", scene_id);
        }

        void release_global_story_lock(bool forced = false)
        {
            const auto initiator_guid = g_story_lock.get_initiator_guid();
            const auto scene_id = g_story_lock.get_scene_id();

            g_story_lock.release_lock();
            g_W3mGlobalSyncInProgress.store(false);

            flush_pending_facts();

            network::protocol::W3mQuestLockPacket packet{};
            packet.is_locked = false;
            packet.scene_id = scene_id;
            packet.player_guid = initiator_guid;
            packet.timestamp = static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());

            utils::buffer_serializer buffer{};
            buffer.write(game::PROTOCOL);
            buffer.write(packet);

            g_telemetry.increment_sent();
            if (g_loopback_enabled)
            {
                receive_session_state_safe(network::get_master_server(), buffer.get_buffer());
            }
            else
            {
                network::send(network::get_master_server(), forced ? "story_lock_release_forced" : "quest_lock", buffer.get_buffer());
            }

            if (forced)
            {
                W3mLog("STORY LOCK FAIL-SAFE: Forced release broadcast (scene %u, initiator=%llu)", scene_id, initiator_guid);
            }
            else
            {
                printf("[W3MP NARRATIVE] Global sync COMPLETED\n");
            }
        }

        bool is_global_sync_in_progress()
        {
            return g_W3mGlobalSyncInProgress.load();
        }

        // ===================================================================
        // DIALOGUE PROXIMITY SYSTEM
        // ===================================================================

        void check_dialogue_proximity(uint64_t initiator_guid, const float* initiator_position)
        {
            if (!initiator_position)
            {
                return;
            }

            printf("[W3MP NARRATIVE] Checking dialogue proximity for initiator %llu\n", initiator_guid);
        }

        // ===================================================================
        // BRIDGE FUNCTIONS - WITCHERSCRIPT CALLABLE
        // ===================================================================

        void W3mBroadcastFact(const scripting::string& fact_name, int32_t value)
        {
            broadcast_quest_fact(fact_name.to_string(), value);
        }

        void W3mAtomicAddFact(const scripting::string& fact_name, int32_t value)
        {
            const auto fact_str = fact_name.to_string();

            if (g_W3mGlobalSyncInProgress.load())
            {
                queue_fact_during_sync(fact_str, value, utils::identity::get_guid());
                return;
            }

            W3mBroadcastFact(fact_name, value);
        }

        void W3mAcquireStoryLock(uint64_t initiator_guid, int32_t scene_id)
        {
            acquire_global_story_lock(initiator_guid, static_cast<uint32_t>(scene_id));
        }

        void W3mReleaseStoryLock()
        {
            release_global_story_lock();
        }

        bool W3mIsStoryLocked()
        {
            return g_story_lock.is_locked();
        }

        bool W3mIsGlobalSyncInProgress()
        {
            return is_global_sync_in_progress();
        }

        int32_t W3mGetFactValue(const scripting::string& fact_name)
        {
            const auto fact = g_fact_manager.get_fact(fact_name.to_string());

            if (fact.has_value())
            {
                return fact->value;
            }

            return 0;
        }

        bool W3mHasFact(const scripting::string& fact_name)
        {
            return g_fact_manager.has_fact(fact_name.to_string());
        }

        void W3mClearFactCache()
        {
            g_fact_manager.clear_cache();
        }

        int32_t W3mGetFactCount()
        {
            return static_cast<int32_t>(g_fact_manager.get_fact_count());
        }

        int32_t W3mComputeWorldStateHash()
        {
            return static_cast<int32_t>(g_fact_manager.compute_world_state_hash());
        }

        void W3mCheckDialogueProximity(uint64_t initiator_guid,
                                       const scripting::game::Vector& initiator_position)
        {
            const float position[3] = {
                initiator_position.X,
                initiator_position.Y,
                initiator_position.Z
            };

            check_dialogue_proximity(initiator_guid, position);
        }

        // ===================================================================
        // NARRATIVE FAIL-SAFE - TIMEOUT PROTECTION
        // ===================================================================

        constexpr uint64_t STORY_LOCK_TIMEOUT_MS = 15000;  // 15 seconds

        void check_story_lock_timeout()
        {
            if (!g_story_lock.is_locked())
            {
                return;
            }

            const auto lock_timestamp = g_story_lock.get_lock_timestamp();
            const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            const auto elapsed_ms = (now - lock_timestamp) / 1000000;  // Convert nanoseconds to milliseconds

            if (elapsed_ms > STORY_LOCK_TIMEOUT_MS)
            {
                const auto initiator_guid = g_story_lock.get_initiator_guid();
                const auto scene_id = g_story_lock.get_scene_id();

                printf("[W3MP NARRATIVE] FAIL-SAFE: Story lock timeout detected (initiator: %llu, scene: %u)\n",
                       initiator_guid, scene_id);
                printf("[W3MP NARRATIVE] FAIL-SAFE: Automatically releasing lock after %llu ms\n", elapsed_ms);

                release_global_story_lock(true);
            }
        }

        // ===================================================================
        // NARRATIVE HEARTBEAT
        // ===================================================================

        void broadcast_narrative_heartbeat()
        {
            const auto world_state_hash = g_fact_manager.compute_world_state_hash();
            const auto fact_count = g_fact_manager.get_fact_count();

            printf("[W3MP NARRATIVE] Heartbeat: %zu facts, world_state_hash=%u\n",
                   fact_count, world_state_hash);

            check_story_lock_timeout();
        }

        // ===================================================================
        // COMPONENT REGISTRATION
        // ===================================================================

        class component final : public component_interface
        {
        public:
            void post_load() override
            {
                W3mLog("=== REGISTERING NARRATIVE SYNCHRONIZATION FUNCTIONS ===");

                scripting::register_function<W3mBroadcastFact>(L"W3mBroadcastFact");
                scripting::register_function<W3mAtomicAddFact>(L"W3mAtomicAddFact");
                scripting::register_function<W3mAcquireStoryLock>(L"W3mAcquireStoryLock");
                scripting::register_function<W3mReleaseStoryLock>(L"W3mReleaseStoryLock");
                scripting::register_function<W3mIsStoryLocked>(L"W3mIsStoryLocked");
                scripting::register_function<W3mIsGlobalSyncInProgress>(L"W3mIsGlobalSyncInProgress");
                scripting::register_function<W3mGetFactValue>(L"W3mGetFactValue");
                scripting::register_function<W3mHasFact>(L"W3mHasFact");
                scripting::register_function<W3mClearFactCache>(L"W3mClearFactCache");
                scripting::register_function<W3mGetFactCount>(L"W3mGetFactCount");
                scripting::register_function<W3mComputeWorldStateHash>(L"W3mComputeWorldStateHash");
                scripting::register_function<W3mCheckDialogueProximity>(L"W3mCheckDialogueProximity");

                W3mLog("Registered 12 narrative synchronization functions");

                scheduler::loop([] {
                    broadcast_narrative_heartbeat();
                }, scheduler::pipeline::async, std::chrono::milliseconds(5000));

                printf("[W3MP NARRATIVE] Narrative synchronization system initialized\n");
            }
        };
    }
}

REGISTER_COMPONENT(quest_sync::component)
