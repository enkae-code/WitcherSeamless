#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>

#include "protocol.hpp"

namespace network::interpolation
{
    using namespace protocol;
    using steady_clock = std::chrono::steady_clock;
    using time_point = steady_clock::time_point;

    // ===========================================================================
    // SNAPSHOT INTERPOLATION FOR MULTIPLAYER MOVEMENT
    // ===========================================================================
    // Implements a 3-snapshot ring buffer strategy to eliminate character snapping
    // in high-latency or high-player-count sessions (e.g., 5 players).
    // Uses Linear Interpolation (LERP) with ~100ms render delay for smooth motion.
    // ===========================================================================

    constexpr size_t SNAPSHOT_BUFFER_SIZE = 3;
    constexpr uint64_t INTERPOLATION_DELAY_MS = 100;  // Render delay for smoothing

    // ---------------------------------------------------------------------------
    // SNAPSHOT STRUCTURE
    // ---------------------------------------------------------------------------
    // Stores a player state packet with its arrival timestamp for interpolation

    struct snapshot
    {
        player_state_packet state{};
        time_point timestamp{};
        bool valid{false};
    };

    // ---------------------------------------------------------------------------
    // PLAYER INTERPOLATOR
    // ---------------------------------------------------------------------------
    // Ring buffer-based interpolator for smooth player movement synchronization
    // Zero-Bloat Philosophy: No dynamic allocations, fixed-size ring buffer

    class player_interpolator
    {
    public:
        player_interpolator() = default;

        // -----------------------------------------------------------------------
        // ADD SNAPSHOT
        // -----------------------------------------------------------------------
        // Inserts a new player state packet into the ring buffer
        // Automatically overwrites oldest snapshot when buffer is full

        void add_snapshot(const player_state_packet& packet)
        {
            const auto now = steady_clock::now();

            snapshots_[write_index_].state = packet;
            snapshots_[write_index_].timestamp = now;
            snapshots_[write_index_].valid = true;

            write_index_ = (write_index_ + 1) % SNAPSHOT_BUFFER_SIZE;

            if (snapshot_count_ < SNAPSHOT_BUFFER_SIZE)
            {
                snapshot_count_++;
            }
        }

        // -----------------------------------------------------------------------
        // GET INTERPOLATED POSITION
        // -----------------------------------------------------------------------
        // Returns smoothly interpolated player state using LERP
        // Uses ~100ms render delay to ensure two snapshots are available
        //
        // Returns std::nullopt if insufficient snapshots for interpolation

        std::optional<player_state_packet> get_interpolated_state()
        {
            if (snapshot_count_ < 2)
            {
                return std::nullopt;  // Need at least 2 snapshots for interpolation
            }

            const auto now = steady_clock::now();
            const auto render_time = now - std::chrono::milliseconds(INTERPOLATION_DELAY_MS);

            // Find two snapshots surrounding the render time
            const snapshot* older = nullptr;
            const snapshot* newer = nullptr;

            for (size_t i = 0; i < snapshot_count_; i++)
            {
                const auto& snap = snapshots_[i];
                if (!snap.valid) continue;

                if (snap.timestamp <= render_time)
                {
                    if (!older || snap.timestamp > older->timestamp)
                    {
                        older = &snap;
                    }
                }
                else
                {
                    if (!newer || snap.timestamp < newer->timestamp)
                    {
                        newer = &snap;
                    }
                }
            }

            // If we don't have both snapshots, use the most recent one
            if (!older || !newer)
            {
                return get_most_recent_snapshot();
            }

            // LERP between older and newer snapshots
            const auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                newer->timestamp - older->timestamp).count();

            if (time_diff == 0)
            {
                return older->state;  // Avoid division by zero
            }

            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                render_time - older->timestamp).count();

            const float t = static_cast<float>(elapsed) / static_cast<float>(time_diff);
            const float clamped_t = std::clamp(t, 0.0f, 1.0f);

            // Create interpolated state
            player_state_packet interpolated = older->state;

            // LERP position
            interpolated.position.x = lerp(older->state.position.x, newer->state.position.x, clamped_t);
            interpolated.position.y = lerp(older->state.position.y, newer->state.position.y, clamped_t);
            interpolated.position.z = lerp(older->state.position.z, newer->state.position.z, clamped_t);
            interpolated.position.w = lerp(older->state.position.w, newer->state.position.w, clamped_t);

            // LERP angles (rotation)
            interpolated.angles.x = lerp_angle(older->state.angles.x, newer->state.angles.x, clamped_t);
            interpolated.angles.y = lerp_angle(older->state.angles.y, newer->state.angles.y, clamped_t);
            interpolated.angles.z = lerp_angle(older->state.angles.z, newer->state.angles.z, clamped_t);

            // LERP velocity
            interpolated.velocity.x = lerp(older->state.velocity.x, newer->state.velocity.x, clamped_t);
            interpolated.velocity.y = lerp(older->state.velocity.y, newer->state.velocity.y, clamped_t);
            interpolated.velocity.z = lerp(older->state.velocity.z, newer->state.velocity.z, clamped_t);
            interpolated.velocity.w = lerp(older->state.velocity.w, newer->state.velocity.w, clamped_t);

            // LERP speed
            interpolated.speed = lerp(older->state.speed, newer->state.speed, clamped_t);

            return interpolated;
        }

        // -----------------------------------------------------------------------
        // RESET
        // -----------------------------------------------------------------------
        // Clears all snapshots (used when player disconnects or teleports)

        void reset()
        {
            for (auto& snap : snapshots_)
            {
                snap.valid = false;
            }
            write_index_ = 0;
            snapshot_count_ = 0;
        }

        // -----------------------------------------------------------------------
        // GET MOST RECENT SNAPSHOT
        // -----------------------------------------------------------------------
        // Returns the most recent valid snapshot (fallback for insufficient data)

        std::optional<player_state_packet> get_most_recent_snapshot() const
        {
            const snapshot* most_recent = nullptr;

            for (const auto& snap : snapshots_)
            {
                if (!snap.valid) continue;

                if (!most_recent || snap.timestamp > most_recent->timestamp)
                {
                    most_recent = &snap;
                }
            }

            if (most_recent)
            {
                return most_recent->state;
            }

            return std::nullopt;
        }

    private:
        // -----------------------------------------------------------------------
        // LINEAR INTERPOLATION HELPERS
        // -----------------------------------------------------------------------

        static float lerp(float a, float b, float t)
        {
            return a + (b - a) * t;
        }

        // LERP for angles with wrapping (handles 359° -> 1° transition smoothly)
        static float lerp_angle(float a, float b, float t)
        {
            float diff = b - a;

            // Normalize to [-180, 180]
            while (diff > 180.0f) diff -= 360.0f;
            while (diff < -180.0f) diff += 360.0f;

            return a + diff * t;
        }

        // -----------------------------------------------------------------------
        // RING BUFFER STATE
        // -----------------------------------------------------------------------

        std::array<snapshot, SNAPSHOT_BUFFER_SIZE> snapshots_{};
        size_t write_index_{0};
        size_t snapshot_count_{0};
    };
}
