// ===========================================================================
// WITCHERSEAMLESS MULTIPLAYER - STRESS TEST MANAGER
// ===========================================================================
// Network Chaos Injection for Simulating "Bad Internet"
// Zero-Bloat Production Implementation
// ===========================================================================

#include "../std_include.hpp"
#include "../loader/component_loader.hpp"

#include "network.hpp"
#include "scripting.hpp"
#include "scheduler.hpp"

#include "../w3m_logger.h"

#include <random>
#include <chrono>
#include <queue>
#include <mutex>

namespace stress_test
{
    namespace
    {
        // ===================================================================
        // CHAOS CONFIGURATION
        // ===================================================================

        std::atomic<bool> g_chaos_mode_enabled{false};
        std::atomic<uint32_t> g_artificial_latency_ms{0};
        std::atomic<uint32_t> g_packet_loss_percent{0};

        // Random number generator for packet loss simulation
        std::random_device g_random_device;
        std::mt19937 g_random_generator{g_random_device()};
        std::uniform_int_distribution<uint32_t> g_loss_distribution{0, 100};

        // ===================================================================
        // DELAYED PACKET QUEUE
        // ===================================================================

        struct delayed_packet
        {
            network::address target_address;
            std::string command;
            std::string data;
            std::chrono::steady_clock::time_point send_time;
        };

        std::queue<delayed_packet> g_delayed_packet_queue;
        std::mutex g_queue_mutex;

        // ===================================================================
        // NETWORK CHAOS INJECTION
        // ===================================================================

        bool should_drop_packet()
        {
            if (!g_chaos_mode_enabled.load())
            {
                return false;
            }

            const auto loss_percent = g_packet_loss_percent.load();

            if (loss_percent == 0)
            {
                return false;
            }

            const auto roll = g_loss_distribution(g_random_generator);
            return roll < loss_percent;
        }

        void inject_latency(const network::address& address, const std::string& command, const std::string& data)
        {
            if (!g_chaos_mode_enabled.load())
            {
                network::send(address, command, data);
                return;
            }

            const auto latency_ms = g_artificial_latency_ms.load();

            if (latency_ms == 0)
            {
                network::send(address, command, data);
                return;
            }

            delayed_packet packet{};
            packet.target_address = address;
            packet.command = command;
            packet.data = data;
            packet.send_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(latency_ms);

            {
                std::lock_guard<std::mutex> lock(g_queue_mutex);
                g_delayed_packet_queue.push(packet);
            }
        }

        void process_delayed_packets()
        {
            std::lock_guard<std::mutex> lock(g_queue_mutex);

            const auto now = std::chrono::steady_clock::now();

            while (!g_delayed_packet_queue.empty())
            {
                const auto& packet = g_delayed_packet_queue.front();

                if (packet.send_time > now)
                {
                    break;
                }

                if (!should_drop_packet())
                {
                    network::send(packet.target_address, packet.command, packet.data);
                }
                else
                {
                    printf("[W3MP CHAOS] Packet DROPPED: %s (loss simulation)\n", packet.command.c_str());
                }

                g_delayed_packet_queue.pop();
            }
        }

        // ===================================================================
        // NETWORK WRAPPER - CHAOS INJECTOR
        // ===================================================================

        bool chaos_send(const network::address& address, const std::string& command, const std::string& data)
        {
            if (!g_chaos_mode_enabled.load())
            {
                return network::send(address, command, data);
            }

            if (should_drop_packet())
            {
                printf("[W3MP CHAOS] Packet DROPPED immediately: %s\n", command.c_str());
                return false;
            }

            inject_latency(address, command, data);
            return true;
        }

        // ===================================================================
        // CHAOS CONTROL FUNCTIONS
        // ===================================================================

        void enable_chaos_mode(uint32_t latency_ms, uint32_t loss_percent)
        {
            if (loss_percent > 100)
            {
                loss_percent = 100;
            }

            g_chaos_mode_enabled.store(true);
            g_artificial_latency_ms.store(latency_ms);
            g_packet_loss_percent.store(loss_percent);

            printf("[W3MP CHAOS] ENABLED: %ums latency, %u%% packet loss\n", latency_ms, loss_percent);
        }

        void disable_chaos_mode()
        {
            g_chaos_mode_enabled.store(false);
            g_artificial_latency_ms.store(0);
            g_packet_loss_percent.store(0);

            {
                std::lock_guard<std::mutex> lock(g_queue_mutex);
                while (!g_delayed_packet_queue.empty())
                {
                    g_delayed_packet_queue.pop();
                }
            }

            printf("[W3MP CHAOS] DISABLED\n");
        }

        bool is_chaos_mode_enabled()
        {
            return g_chaos_mode_enabled.load();
        }

        uint32_t get_artificial_latency()
        {
            return g_artificial_latency_ms.load();
        }

        uint32_t get_packet_loss_percent()
        {
            return g_packet_loss_percent.load();
        }

        // ===================================================================
        // BRIDGE FUNCTIONS - WITCHERSCRIPT CALLABLE
        // ===================================================================

        void W3mInjectNetworkChaos(int32_t latency_ms, int32_t loss_percent)
        {
            if (latency_ms < 0)
            {
                latency_ms = 0;
            }

            if (loss_percent < 0)
            {
                loss_percent = 0;
            }

            if (latency_ms == 0 && loss_percent == 0)
            {
                disable_chaos_mode();
            }
            else
            {
                enable_chaos_mode(static_cast<uint32_t>(latency_ms), static_cast<uint32_t>(loss_percent));
            }
        }

        void W3mChaosMode(int32_t latency_ms, int32_t loss_percent)
        {
            W3mInjectNetworkChaos(latency_ms, loss_percent);
        }

        void W3mDisableChaos()
        {
            disable_chaos_mode();
        }

        bool W3mIsChaosEnabled()
        {
            return is_chaos_mode_enabled();
        }

        int32_t W3mGetChaosLatency()
        {
            return static_cast<int32_t>(get_artificial_latency());
        }

        int32_t W3mGetChaosLoss()
        {
            return static_cast<int32_t>(get_packet_loss_percent());
        }

        // ===================================================================
        // STATISTICS TRACKING
        // ===================================================================

        std::atomic<uint32_t> g_total_packets_sent{0};
        std::atomic<uint32_t> g_total_packets_dropped{0};
        std::atomic<uint32_t> g_total_packets_delayed{0};

        void increment_packet_sent()
        {
            g_total_packets_sent++;
        }

        void increment_packet_dropped()
        {
            g_total_packets_dropped++;
        }

        void increment_packet_delayed()
        {
            g_total_packets_delayed++;
        }

        struct W3mChaosStats
        {
            int32_t total_sent{0};
            int32_t total_dropped{0};
            int32_t total_delayed{0};
            int32_t current_latency_ms{0};
            int32_t current_loss_percent{0};
            bool chaos_enabled{false};
        };

        W3mChaosStats W3mGetChaosStats()
        {
            W3mChaosStats stats{};
            stats.total_sent = static_cast<int32_t>(g_total_packets_sent.load());
            stats.total_dropped = static_cast<int32_t>(g_total_packets_dropped.load());
            stats.total_delayed = static_cast<int32_t>(g_total_packets_delayed.load());
            stats.current_latency_ms = static_cast<int32_t>(g_artificial_latency_ms.load());
            stats.current_loss_percent = static_cast<int32_t>(g_packet_loss_percent.load());
            stats.chaos_enabled = g_chaos_mode_enabled.load();

            return stats;
        }

        void W3mResetChaosStats()
        {
            g_total_packets_sent.store(0);
            g_total_packets_dropped.store(0);
            g_total_packets_delayed.store(0);

            printf("[W3MP CHAOS] Statistics reset\n");
        }

        // ===================================================================
        // COMPONENT REGISTRATION
        // ===================================================================

        class component final : public component_interface
        {
          public:
            void post_load() override
            {
                W3mLog("=== REGISTERING STRESS TEST FUNCTIONS ===");

                scripting::register_function<W3mInjectNetworkChaos>(L"W3mInjectNetworkChaos");
                scripting::register_function<W3mChaosMode>(L"W3mChaosMode");
                scripting::register_function<W3mDisableChaos>(L"W3mDisableChaos");
                scripting::register_function<W3mIsChaosEnabled>(L"W3mIsChaosEnabled");
                scripting::register_function<W3mGetChaosLatency>(L"W3mGetChaosLatency");
                scripting::register_function<W3mGetChaosLoss>(L"W3mGetChaosLoss");
                scripting::register_function<W3mGetChaosStats>(L"W3mGetChaosStats");
                scripting::register_function<W3mResetChaosStats>(L"W3mResetChaosStats");

                W3mLog("Registered 8 stress test functions");

                scheduler::loop([] { process_delayed_packets(); }, scheduler::pipeline::async, std::chrono::milliseconds(10));

                printf("[W3MP CHAOS] Stress test manager initialized\n");
            }
        };
    }
}

REGISTER_COMPONENT(stress_test::component)
