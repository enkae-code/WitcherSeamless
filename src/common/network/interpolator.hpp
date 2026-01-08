#pragma once

#include <algorithm>
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
    constexpr uint64_t RECOVERY_BLEND_DURATION_MS = 500;  // 0.5 second visual recovery blend

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
                return handle_extrapolation();
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

            // If we don't have both snapshots, check for extrapolation
            if (!older || !newer)
            {
                return handle_extrapolation();
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

            auto result = apply_blend_if_needed(interpolated, now);
            last_returned_state_ = result;
            return result;
        }

        // -----------------------------------------------------------------------
        // POSITION EXTRAPOLATION (DEAD RECKONING)
        // -----------------------------------------------------------------------
        // Predicts position when snapshots are missing or outdated (>100ms)
        // Formula: predicted_pos = current_pos + (velocity * delta_time)
        // Handles 3+ consecutive missed packets gracefully

        std::optional<player_state_packet> get_extrapolated_position()
        {
            const auto most_recent = get_most_recent_snapshot();

            if (!most_recent.has_value())
            {
                return std::nullopt;
            }

            const auto now = steady_clock::now();
            const auto* latest_snapshot = get_latest_snapshot_ptr();

            if (!latest_snapshot)
            {
                return most_recent;
            }

            const auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - latest_snapshot->timestamp).count();

            constexpr int64_t EXTRAPOLATION_THRESHOLD_MS = 100;

            if (age_ms <= EXTRAPOLATION_THRESHOLD_MS)
            {
                return most_recent;
            }

            const float delta_time_seconds = static_cast<float>(age_ms - INTERPOLATION_DELAY_MS) / 1000.0f;

            player_state_packet extrapolated = most_recent.value();

            extrapolated.position.x += extrapolated.velocity.x * delta_time_seconds;
            extrapolated.position.y += extrapolated.velocity.y * delta_time_seconds;
            extrapolated.position.z += extrapolated.velocity.z * delta_time_seconds;

            return extrapolated;
        }

        const snapshot* get_latest_snapshot_ptr() const
        {
            const snapshot* latest = nullptr;

            for (const auto& snap : snapshots_)
            {
                if (!snap.valid) continue;

                if (!latest || snap.timestamp > latest->timestamp)
                {
                    latest = &snap;
                }
            }

            return latest;
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
        std::optional<player_state_packet> handle_extrapolation()
        {
            auto extrapolated = get_extrapolated_position();
            if (extrapolated)
            {
                in_extrapolation_ = true;
                blend_active_ = false;
                extrapolation_anchor_ = extrapolated;
                last_returned_state_ = extrapolated;
            }
            return extrapolated;
        }

        player_state_packet apply_blend_if_needed(const player_state_packet& target_state, const time_point& now)
        {
            if (in_extrapolation_ && extrapolation_anchor_.has_value())
            {
                begin_blend(*extrapolation_anchor_, target_state, now);
                in_extrapolation_ = false;
            }

            if (!blend_active_)
            {
                return target_state;
            }

            const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - blend_start_time_).count();
            const float t = std::clamp(static_cast<float>(elapsed_ms) / static_cast<float>(RECOVERY_BLEND_DURATION_MS), 0.0f, 1.0f);

            blend_target_state_ = target_state;
            auto blended = blend_packets(blend_start_state_, blend_target_state_, t);

            if (elapsed_ms >= static_cast<int64_t>(RECOVERY_BLEND_DURATION_MS))
            {
                blend_active_ = false;
                extrapolation_anchor_.reset();
            }

            return blended;
        }

        void begin_blend(const player_state_packet& from_state, const player_state_packet& to_state, const time_point& now)
        {
            blend_active_ = true;
            blend_start_time_ = now;
            blend_start_state_ = from_state;
            blend_target_state_ = to_state;
        }

        static player_state_packet blend_packets(const player_state_packet& from, const player_state_packet& to, float t)
        {
            player_state_packet blended = from;
            blended.position.x = lerp(from.position.x, to.position.x, t);
            blended.position.y = lerp(from.position.y, to.position.y, t);
            blended.position.z = lerp(from.position.z, to.position.z, t);
            blended.position.w = lerp(from.position.w, to.position.w, t);

            blended.angles.x = lerp_angle(from.angles.x, to.angles.x, t);
            blended.angles.y = lerp_angle(from.angles.y, to.angles.y, t);
            blended.angles.z = lerp_angle(from.angles.z, to.angles.z, t);

            blended.velocity.x = lerp(from.velocity.x, to.velocity.x, t);
            blended.velocity.y = lerp(from.velocity.y, to.velocity.y, t);
            blended.velocity.z = lerp(from.velocity.z, to.velocity.z, t);
            blended.velocity.w = lerp(from.velocity.w, to.velocity.w, t);

            blended.speed = lerp(from.speed, to.speed, t);

            return blended;
        }

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

        bool in_extrapolation_{false};
        bool blend_active_{false};
        time_point blend_start_time_{};
        player_state_packet blend_start_state_{};
        player_state_packet blend_target_state_{};
        std::optional<player_state_packet> extrapolation_anchor_;
        std::optional<player_state_packet> last_returned_state_;
    };
}
