// ===========================================================================
// WITCHERSEAMLESS MULTIPLAYER - INPUT MANAGER
// ===========================================================================
// Keyboard capture for Command Palette and Dashboard UI
// Zero-Jank Production Implementation
// ===========================================================================

#include "../std_include.hpp"
#include "../loader/component_loader.hpp"

#include "scripting.hpp"
#include "scheduler.hpp"

#include "../w3m_logger.h"

#include <mutex>
#include <string>
#include <atomic>

namespace input_manager
{
    namespace
    {
        // ===================================================================
        // INPUT STATE
        // ===================================================================

        std::atomic<bool> g_ui_active{false};
        std::string g_input_buffer;
        std::mutex g_input_mutex;

        HHOOK g_message_hook{nullptr};
        HWND g_game_window{nullptr};

        // ===================================================================
        // INPUT BUFFER MANAGEMENT
        // ===================================================================

        void append_char(char c)
        {
            std::lock_guard<std::mutex> lock(g_input_mutex);
            g_input_buffer += c;
        }

        void backspace()
        {
            std::lock_guard<std::mutex> lock(g_input_mutex);

            if (!g_input_buffer.empty())
            {
                g_input_buffer.pop_back();
            }
        }

        std::string get_input_buffer()
        {
            std::lock_guard<std::mutex> lock(g_input_mutex);
            return g_input_buffer;
        }

        void clear_input_buffer()
        {
            std::lock_guard<std::mutex> lock(g_input_mutex);
            g_input_buffer.clear();
        }

        void set_input_buffer(const std::string& text)
        {
            std::lock_guard<std::mutex> lock(g_input_mutex);
            g_input_buffer = text;
        }

        // ===================================================================
        // UI ACTIVATION CONTROL
        // ===================================================================

        void set_ui_active(bool active)
        {
            g_ui_active.store(active);

            if (!active)
            {
                clear_input_buffer();
            }

            printf("[W3MP INPUT] UI %s\n", active ? "ACTIVATED" : "DEACTIVATED");
        }

        bool is_ui_active()
        {
            return g_ui_active.load();
        }

        void toggle_ui()
        {
            set_ui_active(!is_ui_active());
        }

        // ===================================================================
        // WINDOWS MESSAGE HOOK
        // ===================================================================

        LRESULT CALLBACK message_hook_proc(int code, WPARAM wParam, LPARAM lParam)
        {
            if (code >= 0)
            {
                const auto* msg = reinterpret_cast<MSG*>(lParam);

                if (msg && msg->hwnd == g_game_window)
                {
                    if (msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN)
                    {
                        const bool alt_pressed = (GetKeyState(VK_MENU) & 0x8000) != 0;

                        if (alt_pressed && msg->wParam == 'S')
                        {
                            toggle_ui();
                            return 1;
                        }

                        if (is_ui_active())
                        {
                            if (msg->wParam == VK_ESCAPE)
                            {
                                set_ui_active(false);
                                return 1;
                            }

                            if (msg->wParam == VK_RETURN)
                            {
                                printf("[W3MP INPUT] Command submitted: %s\n", get_input_buffer().c_str());
                                clear_input_buffer();
                                set_ui_active(false);
                                return 1;
                            }

                            if (msg->wParam == VK_BACK)
                            {
                                backspace();
                                return 1;
                            }
                        }
                    }
                    else if (msg->message == WM_CHAR && is_ui_active())
                    {
                        const char c = static_cast<char>(msg->wParam);

                        if (c >= 32 && c < 127)
                        {
                            append_char(c);
                        }

                        return 1;
                    }
                }
            }

            return CallNextHookEx(g_message_hook, code, wParam, lParam);
        }

        // ===================================================================
        // HOOK INSTALLATION
        // ===================================================================

        void install_message_hook()
        {
            g_game_window = FindWindowA(nullptr, "The Witcher 3");

            if (!g_game_window)
            {
                g_game_window = GetForegroundWindow();
            }

            if (!g_game_window)
            {
                printf("[W3MP INPUT] WARNING: Game window not found\n");
                return;
            }

            const DWORD thread_id = GetWindowThreadProcessId(g_game_window, nullptr);

            g_message_hook = SetWindowsHookExA(WH_GETMESSAGE, message_hook_proc, nullptr, thread_id);

            if (g_message_hook)
            {
                printf("[W3MP INPUT] Message hook installed successfully\n");
            }
            else
            {
                printf("[W3MP INPUT] ERROR: Failed to install message hook (error: %lu)\n", GetLastError());
            }
        }

        void uninstall_message_hook()
        {
            if (g_message_hook)
            {
                UnhookWindowsHookEx(g_message_hook);
                g_message_hook = nullptr;

                printf("[W3MP INPUT] Message hook uninstalled\n");
            }
        }

        // ===================================================================
        // BRIDGE FUNCTIONS - WITCHERSCRIPT CALLABLE
        // ===================================================================

        void W3mToggleUI()
        {
            toggle_ui();
        }

        void W3mSetUIActive(bool active)
        {
            set_ui_active(active);
        }

        bool W3mIsUIActive()
        {
            return is_ui_active();
        }

        scripting::string W3mGetInputBuffer()
        {
            return scripting::string(get_input_buffer());
        }

        void W3mSetInputBuffer(const scripting::string& text)
        {
            set_input_buffer(text.to_string());
        }

        void W3mClearInputBuffer()
        {
            clear_input_buffer();
        }

        // ===================================================================
        // COMPONENT REGISTRATION
        // ===================================================================

        class component final : public component_interface
        {
        public:
            void post_load() override
            {
                W3mLog("=== REGISTERING INPUT MANAGER FUNCTIONS ===");

                scripting::register_function<W3mToggleUI>(L"W3mToggleUI");
                scripting::register_function<W3mSetUIActive>(L"W3mSetUIActive");
                scripting::register_function<W3mIsUIActive>(L"W3mIsUIActive");
                scripting::register_function<W3mGetInputBuffer>(L"W3mGetInputBuffer");
                scripting::register_function<W3mSetInputBuffer>(L"W3mSetInputBuffer");
                scripting::register_function<W3mClearInputBuffer>(L"W3mClearInputBuffer");

                W3mLog("Registered 6 input manager functions");

                scheduler::once([] {
                    install_message_hook();
                }, scheduler::pipeline::main, std::chrono::milliseconds(1000));

                printf("[W3MP INPUT] Input manager initialized\n");
            }

            void pre_destroy() override
            {
                uninstall_message_hook();
            }
        };
    }
}

REGISTER_COMPONENT(input_manager::component)
