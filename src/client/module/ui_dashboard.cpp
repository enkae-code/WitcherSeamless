// ===========================================================================
// WITCHERSEAMLESS MULTIPLAYER - INTERACTIVE COMMAND DASHBOARD
// ===========================================================================
// Production-Grade UI with Zero-Jank, Zero-Bloat Architecture
// ===========================================================================

#include "../std_include.hpp"
#include "../loader/component_loader.hpp"

#include "ui_dashboard.hpp"
#include "renderer.hpp"
#include "input_manager.hpp"
#include "network.hpp"
#include "stress_test.hpp"
#include "quest_sync.hpp"
#include "scheduler.hpp"

#include "../w3m_logger.h"

#include <chrono>
#include <string>
#include <sstream>

namespace ui_dashboard
{
    namespace
    {
        // ===================================================================
        // VISUAL CONSTANTS
        // ===================================================================

        constexpr float COMMAND_BAR_X = 210.0f; // Centered at 1920/2 - 600/2
        constexpr float COMMAND_BAR_Y = 200.0f;
        constexpr float COMMAND_BAR_WIDTH = 600.0f;
        constexpr float COMMAND_BAR_HEIGHT = 40.0f;

        constexpr uint32_t COLOR_MIDNIGHT = 0xC0000000; // 75% transparent black
        constexpr uint32_t COLOR_WHITE = 0xFFFFFFFF;    // Opaque white
        constexpr uint32_t COLOR_YELLOW = 0xFFFFFF00;   // Yellow for warnings

        constexpr float TEXT_OFFSET_X = 10.0f;
        constexpr float TEXT_OFFSET_Y = 12.0f;

        constexpr float HUD_X = 1600.0f; // Top-right corner
        constexpr float HUD_Y = 50.0f;

        // ===================================================================
        // BLINKING CURSOR LOGIC
        // ===================================================================

        std::chrono::steady_clock::time_point g_last_blink_time{};
        bool g_cursor_visible{true};

        void update_cursor_blink()
        {
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_last_blink_time).count();

            constexpr int64_t BLINK_INTERVAL_MS = 500;

            if (elapsed_ms >= BLINK_INTERVAL_MS)
            {
                g_cursor_visible = !g_cursor_visible;
                g_last_blink_time = now;
            }
        }

        bool is_cursor_visible()
        {
            return g_cursor_visible;
        }

        // ===================================================================
        // COMMAND PARSING & EXECUTION
        // ===================================================================

        void execute_command(const std::string& command)
        {
            if (command.empty())
            {
                return;
            }

            printf("[W3MP DASHBOARD] Executing command: %s\n", command.c_str());

            // Parse command tokens
            std::istringstream iss(command);
            std::string cmd_type;
            iss >> cmd_type;

            if (cmd_type == "join")
            {
                std::string address;
                iss >> address;

                if (address.empty())
                {
                    printf("[W3MP DASHBOARD] ERROR: 'join' command requires an address (e.g., join 192.168.1.100:28960)\n");
                    return;
                }

                if (network::connect(address))
                {
                    printf("[W3MP DASHBOARD] Successfully connected to %s\n", address.c_str());
                }
                else
                {
                    printf("[W3MP DASHBOARD] Failed to connect to %s\n", address.c_str());
                }
            }
            else if (cmd_type == "chaos")
            {
                int32_t latency_ms = 0;
                int32_t loss_percent = 0;

                iss >> latency_ms >> loss_percent;

                if (latency_ms < 0 || loss_percent < 0 || loss_percent > 100)
                {
                    printf("[W3MP DASHBOARD] ERROR: 'chaos' command requires valid latency (>=0ms) and loss (0-100%%)\n");
                    return;
                }

                // Toggle chaos mode via stress_test manager
                // Note: stress_test functions are registered in stress_test.cpp
                printf("[W3MP DASHBOARD] Activating network chaos: %dms latency, %d%% loss\n", latency_ms, loss_percent);

                // Call stress_test directly (no WitcherScript bridge needed)
                // This assumes stress_test exposes a C++ API
                // For now, log the command - full integration requires stress_test::enable_chaos_mode to be exposed
                printf("[W3MP DASHBOARD] Chaos mode command received (integration pending)\n");
            }
            else
            {
                printf("[W3MP DASHBOARD] ERROR: Unknown command '%s'. Available: join, chaos\n", cmd_type.c_str());
            }
        }

        // ===================================================================
        // COMMAND PALETTE RENDERING
        // ===================================================================

        void render_command_palette()
        {
            if (!input_manager::is_ui_active())
            {
                return;
            }

            update_cursor_blink();

            // Draw background fill (Midnight)
            renderer::draw_rect({COMMAND_BAR_X, COMMAND_BAR_Y}, {COMMAND_BAR_WIDTH, COMMAND_BAR_HEIGHT}, COLOR_MIDNIGHT);

            // Draw 1px border (White) - top, bottom, left, right
            renderer::draw_rect({COMMAND_BAR_X, COMMAND_BAR_Y}, {COMMAND_BAR_WIDTH, 1.0f}, COLOR_WHITE);
            renderer::draw_rect({COMMAND_BAR_X, COMMAND_BAR_Y + COMMAND_BAR_HEIGHT - 1.0f}, {COMMAND_BAR_WIDTH, 1.0f}, COLOR_WHITE);
            renderer::draw_rect({COMMAND_BAR_X, COMMAND_BAR_Y}, {1.0f, COMMAND_BAR_HEIGHT}, COLOR_WHITE);
            renderer::draw_rect({COMMAND_BAR_X + COMMAND_BAR_WIDTH - 1.0f, COMMAND_BAR_Y}, {1.0f, COMMAND_BAR_HEIGHT}, COLOR_WHITE);

            // Get input buffer and add blinking cursor
            std::string display_text = input_manager::get_input_buffer();

            if (is_cursor_visible())
            {
                display_text += "|";
            }

            // Draw text inside the bar
            renderer::draw_text(display_text, {COMMAND_BAR_X + TEXT_OFFSET_X, COMMAND_BAR_Y + TEXT_OFFSET_Y}, {255, 255, 255, 255});
        }

        // ===================================================================
        // GLOBAL HUD STATUS (TOP-RIGHT CORNER)
        // ===================================================================

        std::chrono::steady_clock::time_point g_last_warning_blink_time{};
        bool g_warning_visible{true};

        void update_warning_blink()
        {
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_last_warning_blink_time).count();

            constexpr int64_t WARNING_BLINK_INTERVAL_MS = 750;

            if (elapsed_ms >= WARNING_BLINK_INTERVAL_MS)
            {
                g_warning_visible = !g_warning_visible;
                g_last_warning_blink_time = now;
            }
        }

        void render_global_hud()
        {
            update_warning_blink();

            // Display party count (placeholder - will integrate with actual party manager)
            constexpr int32_t PARTY_COUNT = 1; // TODO: Get from party manager
            const std::string party_text = "W3M PARTY: " + std::to_string(PARTY_COUNT) + "/5";

            renderer::draw_text(party_text, {HUD_X, HUD_Y}, {255, 255, 255, 255});

            // Display story lock warning if global sync is in progress
            if (quest_sync::is_global_sync_active() && g_warning_visible)
            {
                renderer::draw_text("STORY LOCKED", {HUD_X, HUD_Y + 20.0f}, {255, 255, 0, 255}); // Yellow
            }
        }

        // ===================================================================
        // COMPONENT REGISTRATION
        // ===================================================================

        class component final : public component_interface
        {
          public:
            void post_load() override
            {
                W3mLog("=== REGISTERING UI DASHBOARD ===");

                // Register dashboard rendering on the renderer pipeline
                scheduler::loop(
                    [] {
                        render_command_palette();
                        render_global_hud();
                    },
                    scheduler::pipeline::renderer,
                    std::chrono::milliseconds(16)); // 60 FPS rendering

                // Register command execution callback with input manager
                input_manager::set_command_callback([](const std::string& command) { execute_command(command); });

                printf("[W3MP DASHBOARD] UI Dashboard initialized (Alt+S to toggle)\n");
            }
        };
    }
}

REGISTER_COMPONENT(ui_dashboard::component)
