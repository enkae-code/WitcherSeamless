#pragma once

#include <array>
#include <cstdint>
#include "../game/structs.hpp"

namespace network::protocol
{
    // ===========================================================================
    // TRUE CO-OP PROTOCOL STRUCTURES
    // ===========================================================================
    // These packets enable quest sync, combat sync, and cutscene sync
    // for cooperative multiplayer gameplay
    // ===========================================================================

    constexpr size_t MAX_FACT_NAME_LENGTH = 128;
    constexpr size_t MAX_TAG_LENGTH = 64;
    constexpr size_t MAX_CUTSCENE_PATH_LENGTH = 256;

    // ---------------------------------------------------------------------------
    // QUEST SYNC: Fact Broadcasting
    // ---------------------------------------------------------------------------
    // Synchronizes quest progression via the Witcher 3 Facts system
    // Examples: quest objectives, tutorial flags, kill counts, discovered locations

    struct fact_packet
    {
        std::array<char, MAX_FACT_NAME_LENGTH> fact_name{};  // Quest fact identifier (e.g., "killed_griffin")
        int32_t value{};                                      // Fact value (usually 1, can be counters)
        uint64_t timestamp{};                                 // Sync timestamp for ordering
    };

    // ---------------------------------------------------------------------------
    // COMBAT SYNC: Attack Broadcasting
    // ---------------------------------------------------------------------------
    // Replicates player attacks and damage to NPCs across all clients
    // Ensures all players see the same combat encounters and NPC health

    enum class attack_type : uint8_t
    {
        light = 0,      // Fast attack (low damage)
        heavy = 1,      // Strong attack (high damage)
        special = 2     // Signs, finishers, special moves
    };

    struct attack_packet
    {
        uint64_t attacker_guid{};                         // Attacking player's unique ID
        std::array<char, MAX_TAG_LENGTH> target_tag{};    // Target NPC tag (e.g., "drowner_001")
        float damage_amount{};                            // Damage value to apply
        attack_type type{};                               // Attack type enum
        bool force_kill{};                                // Force NPC death reconciliation
        uint64_t timestamp{};                             // Attack timestamp
    };

    // ---------------------------------------------------------------------------
    // CUTSCENE SYNC: Story Scene Broadcasting
    // ---------------------------------------------------------------------------
    // Synchronizes cutscene playback across all players
    // Ensures everyone watches story events simultaneously

    struct cutscene_packet
    {
        std::array<char, MAX_CUTSCENE_PATH_LENGTH> cutscene_path{};  // .w2scene file path
        game::vec4_t position{};                                     // World position
        game::vec3_t rotation{};                                     // World rotation (EulerAngles)
        uint64_t timestamp{};                                        // Start timestamp
    };

    // ---------------------------------------------------------------------------
    // ANIMATION SYNC: Non-Movement Animation Broadcasting
    // ---------------------------------------------------------------------------
    // Synchronizes player animations like looting, drinking, meditating
    // Ensures all players see each other's contextual actions

    constexpr size_t MAX_ANIM_NAME_LENGTH = 64;

    struct anim_packet
    {
        uint64_t player_guid{};                           // Player performing animation
        std::array<char, MAX_ANIM_NAME_LENGTH> anim_name{}; // Animation name (e.g., "meditation")
        int32_t exploration_action{};                     // EPlayerExplorationAction enum
        uint64_t timestamp{};                             // Animation start timestamp
    };

    // ---------------------------------------------------------------------------
    // VEHICLE SYNC: Mount/Dismount Broadcasting
    // ---------------------------------------------------------------------------
    // Synchronizes vehicle mounting across all players
    // Ensures remote clients see players on horses/boats

    constexpr size_t MAX_VEHICLE_TEMPLATE_LENGTH = 128;

    struct vehicle_packet
    {
        uint64_t player_guid{};                                      // Player mounting vehicle
        std::array<char, MAX_VEHICLE_TEMPLATE_LENGTH> vehicle_template{}; // Vehicle entity template path
        bool is_mounting{};                                          // true = mount, false = dismount
        game::vec4_t vehicle_position{};                             // Vehicle spawn position
        game::vec3_t vehicle_rotation{};                             // Vehicle spawn rotation
        uint64_t timestamp{};                                        // Mount/dismount timestamp
    };

    // ---------------------------------------------------------------------------
    // QUEST LOCK: Quest Spectatorship System
    // ---------------------------------------------------------------------------
    // Synchronizes cutscene/dialogue states across all players
    // Freezes remote players during quest scenes to prevent wandering

    struct quest_lock_packet
    {
        bool is_locked{};                // true = lock (scene started), false = unlock (scene ended)
        uint32_t scene_id{};             // Unique scene identifier for tracking
        uint64_t player_guid{};          // Player who started the scene
        uint32_t timestamp{};            // Lock/unlock timestamp
    };

    // ---------------------------------------------------------------------------
    // LOOT SYNC: Shared Loot & Instant Economy
    // ---------------------------------------------------------------------------
    // Synchronizes relic/boss loot and instant gold distribution
    // Ensures all players receive valuable items and currency

    constexpr size_t MAX_ITEM_NAME_LENGTH = 64;

    struct loot_packet
    {
        std::array<char, MAX_ITEM_NAME_LENGTH> item_name{}; // Item name (e.g., "Crowns", "Relic Sword")
        uint32_t quantity{};                                 // Item quantity
        uint64_t player_guid{};                              // Player who looted
        uint32_t timestamp{};                                // Loot timestamp
    };

    // ---------------------------------------------------------------------------
    // ACHIEVEMENT SYNC: Progression Broadcasting
    // ---------------------------------------------------------------------------
    // Synchronizes achievement unlocks across all players
    // Ensures party members share progression milestones

    constexpr size_t MAX_ACHIEVEMENT_ID_LENGTH = 64;

    struct achievement_packet
    {
        std::array<char, MAX_ACHIEVEMENT_ID_LENGTH> achievement_id{}; // Achievement name (e.g., "EA_FindCiri")
        uint64_t player_guid{};                                       // Player who unlocked achievement
        uint32_t timestamp{};                                         // Unlock timestamp
    };

    // ---------------------------------------------------------------------------
    // HANDSHAKE: Session Establishment
    // ---------------------------------------------------------------------------
    // Secure session establishment before gameplay packets

    struct handshake_packet
    {
        uint64_t session_id{};                     // 64-bit session identifier
        uint32_t player_guid{};                    // Player's unique ID
        uint32_t protocol_version{};               // Protocol version for compatibility
        char player_name[32]{};                    // Player display name
        uint32_t timestamp{};                      // Handshake timestamp
    };

    // ---------------------------------------------------------------------------
    // RECONCILIATION HEARTBEAT: World State Sync
    // ---------------------------------------------------------------------------
    // 5-second heartbeat to correct UDP packet drops
    // Synchronizes shared economy (crowns) and critical world state

    struct heartbeat_packet
    {
        uint64_t player_guid{};        // Player's unique ID
        uint32_t total_crowns{};       // Total currency for shared purse
        uint32_t world_fact_hash{};    // Hash of critical world facts for validation
        uint32_t script_version{};     // WitcherScript version for compatibility check
        uint32_t game_time{};          // Current world clock (GameTime in seconds)
        uint16_t weather_id{};         // Current active weather effect ID
        uint64_t timestamp{};          // Heartbeat timestamp
    };

    struct player_state_packet
    {
        uint64_t player_guid{};
        Vector position{};
        EulerAngles angles{};
        Vector velocity{};
        int32_t move_type{};
        float speed{};
    };

    // ===========================================================================
    // SCRIPTING & LEGACY TYPE ALIASES
    // ===========================================================================
    // For WitcherScript integration and maintaining consistent naming
    // as requested by the architectural specification.
    // ===========================================================================

    using Vector = game::vec4_t;
    using EulerAngles = game::vec3_t;

    using W3mFactPacket = fact_packet;
    using W3mAttackPacket = attack_packet;
    using W3mCutscenePacket = cutscene_packet;
    using W3mAnimPacket = anim_packet;
    using W3mVehiclePacket = vehicle_packet;
    using W3mQuestLockPacket = quest_lock_packet;
    using W3mLootPacket = loot_packet;
    using W3mAchievementPacket = achievement_packet;
    using W3mHandshakePacket = handshake_packet;
    using W3mHeartbeatPacket = heartbeat_packet;
    using W3mPlayerStatePacket = player_state_packet;

    // ===========================================================================
    // PACKET TYPE ENUMERATION
    // ===========================================================================
    // Used for packet identification in the network layer

    enum class packet_type : uint8_t
    {
        player_state = 0,   // Existing: position/rotation/velocity sync
        fact = 1,           // New: Quest fact sync
        attack = 2,         // New: Combat attack sync
        cutscene = 3,       // New: Cutscene trigger sync
        anim = 4,           // New: Animation sync for contextual actions
        vehicle = 5,        // New: Vehicle mount/dismount sync
        quest_lock = 6,     // New: Quest spectatorship and scene locking
        loot = 7,           // New: Shared loot and instant economy
        achievement = 8,    // New: Achievement unlock sync
        handshake = 9,      // New: Session establishment
        heartbeat = 10      // New: Reconciliation heartbeat for world state
    };

    // ===========================================================================
    // HELPER FUNCTIONS
    // ===========================================================================

    // Safe string copy into fixed-size arrays
    template <size_t N>
    inline void copy_string(std::array<char, N>& dest, const std::string& src)
    {
        const size_t copy_len = std::min(src.size(), N - 1);
        std::memcpy(dest.data(), src.data(), copy_len);
        dest[copy_len] = '\0';  // Null terminate
    }

    // Extract string from fixed-size array
    template <size_t N>
    inline std::string extract_string(const std::array<char, N>& src)
    {
        return std::string(src.data(), strnlen(src.data(), N));
    }
}
