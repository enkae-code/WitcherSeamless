#pragma once

#include <cstdint>
#include <optional>

namespace scaling
{
    // ===========================================================================
    // DYNAMIC DIFFICULTY SCALING FOR MULTIPLAYER
    // ===========================================================================
    // Implements party-based NPC scaling to maintain challenge in co-op play
    // Follows the formula: 1.0 + (partyCount - 1) * 0.5
    // Example: 2 players = 1.5x health, 3 players = 2.0x health, 5 players = 3.0x health
    // ===========================================================================

    constexpr float HEALTH_MULTIPLIER_PER_PLAYER = 0.5f;
    constexpr float BOSS_DAMAGE_BONUS_PER_PLAYER = 0.2f;

    // ---------------------------------------------------------------------------
    // SCALING CONFIGURATION
    // ---------------------------------------------------------------------------
    // Party scaling parameters for different NPC types

    struct scaling_config
    {
        float health_multiplier{1.0f}; // Health multiplier for this NPC
        float damage_multiplier{1.0f}; // Damage multiplier for boss NPCs
        bool is_boss{false};           // Boss NPCs get damage bonus
        int32_t party_count{1};        // Current party size
    };

    // ---------------------------------------------------------------------------
    // PARTY SCALING MANAGER
    // ---------------------------------------------------------------------------
    // Handles dynamic NPC scaling based on active player count
    // Zero-Bloat Philosophy: No dynamic allocations, pure calculation

    class scaling_manager
    {
      public:
        // -----------------------------------------------------------------------
        // CALCULATE HEALTH MULTIPLIER
        // -----------------------------------------------------------------------
        // Returns health multiplier based on party count
        // Formula: 1.0 + (partyCount - 1) * 0.5

        static float calculate_health_multiplier(int32_t party_count)
        {
            if (party_count <= 1)
            {
                return 1.0f;
            }

            return 1.0f + static_cast<float>(party_count - 1) * HEALTH_MULTIPLIER_PER_PLAYER;
        }

        // -----------------------------------------------------------------------
        // CALCULATE DAMAGE MULTIPLIER
        // -----------------------------------------------------------------------
        // Returns damage multiplier for boss NPCs based on party count
        // Formula: 1.0 + (partyCount - 1) * 0.2

        static float calculate_damage_multiplier(int32_t party_count, bool is_boss)
        {
            if (party_count <= 1 || !is_boss)
            {
                return 1.0f;
            }

            return 1.0f + static_cast<float>(party_count - 1) * BOSS_DAMAGE_BONUS_PER_PLAYER;
        }

        // -----------------------------------------------------------------------
        // CREATE SCALING CONFIG
        // -----------------------------------------------------------------------
        // Generates a complete scaling configuration for an NPC

        static scaling_config create_config(int32_t party_count, bool is_boss = false)
        {
            scaling_config config{};
            config.party_count = party_count;
            config.is_boss = is_boss;
            config.health_multiplier = calculate_health_multiplier(party_count);
            config.damage_multiplier = calculate_damage_multiplier(party_count, is_boss);

            return config;
        }

        // -----------------------------------------------------------------------
        // APPLY HEALTH SCALING
        // -----------------------------------------------------------------------
        // Applies health multiplier to NPC vitality stats
        // Returns scaled health value

        static std::optional<float> apply_health_scaling(float base_health, int32_t party_count)
        {
            if (party_count <= 1)
            {
                return std::nullopt; // No scaling needed for single player
            }

            const float multiplier = calculate_health_multiplier(party_count);
            return base_health * multiplier;
        }

        // -----------------------------------------------------------------------
        // APPLY DAMAGE SCALING
        // -----------------------------------------------------------------------
        // Applies damage multiplier to boss NPC attacks
        // Returns scaled damage value

        static std::optional<float> apply_damage_scaling(float base_damage, int32_t party_count, bool is_boss)
        {
            if (party_count <= 1 || !is_boss)
            {
                return std::nullopt; // No scaling needed
            }

            const float multiplier = calculate_damage_multiplier(party_count, is_boss);
            return base_damage * multiplier;
        }

        // -----------------------------------------------------------------------
        // GET SCALING DESCRIPTION
        // -----------------------------------------------------------------------
        // Returns human-readable description of scaling applied
        // Used for logging and debugging

        static const char* get_scaling_description(int32_t party_count, bool is_boss)
        {
            if (party_count <= 1)
            {
                return "No scaling (solo play)";
            }

            if (is_boss)
            {
                return "Boss scaling (health + damage)";
            }

            return "Standard scaling (health only)";
        }
    };
}
