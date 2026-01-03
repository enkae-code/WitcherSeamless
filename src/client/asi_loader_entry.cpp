// ===========================================================================
// WITCHERSEAMLESS - UNIVERSAL DLL ENTRY POINT
// ===========================================================================
// Compatible with Ultimate ASI Loader (version.dll injection)
// Provides standard DllMain export for ASI loader compatibility
// ===========================================================================

#include "std_include.hpp"
#include "loader/component_loader.hpp"
#include "w3m_logger.h"

// ===========================================================================
// GLOBAL STATE
// ===========================================================================

static bool g_initialized = false;

// ===========================================================================
// INITIALIZATION FUNCTION
// ===========================================================================

void InitializeWitcherSeamless()
{
    try
    {
        W3mLog("=== WITCHERSEAMLESS MULTIPLAYER INITIALIZATION ===");
        W3mLog("Entry point: DllMain (Ultimate ASI Loader compatible)");
        W3mLog("Game process attached successfully");

        // Wait for game to initialize before loading components
        W3mLog("Waiting 3 seconds for game initialization...");
        Sleep(3000);

        // Initialize component loader
        W3mLog("Initializing component loader...");

        try
        {
            if (!component_loader::post_start())
            {
                W3mLog("ERROR: component_loader::post_start() returned false");
                return;
            }
            W3mLog("Component loader started successfully");
        }
        catch (const std::exception& e)
        {
            W3mLog("EXCEPTION in post_start: %s", e.what());
            return;
        }

        // Post-load initialization
        W3mLog("Loading components...");

        try
        {
            if (!component_loader::post_load())
            {
                W3mLog("ERROR: component_loader::post_load() returned false");
                return;
            }
            W3mLog("Components loaded successfully");
        }
        catch (const std::exception& e)
        {
            W3mLog("EXCEPTION in post_load: %s", e.what());
            return;
        }

        g_initialized = true;
        W3mLog("=== WITCHERSEAMLESS INITIALIZATION COMPLETE ===");
        W3mLog("Version: Production Build (Ultimate ASI Loader)");
        W3mLog("Hook: d3d11.dll -> scripting_experiments.dll");

        MessageBoxA(nullptr, "WitcherSeamless loaded successfully!\nCheck W3M_Debug.log for details.",
                   "WitcherSeamless - Success", MB_ICONINFORMATION);
    }
    catch (const std::exception& e)
    {
        W3mLog("FATAL EXCEPTION during initialization: %s", e.what());
        MessageBoxA(nullptr, e.what(), "WitcherSeamless - Initialization Error", MB_ICONERROR);
    }
    catch (...)
    {
        W3mLog("FATAL UNKNOWN EXCEPTION during initialization");
        MessageBoxA(nullptr, "Unknown exception during initialization", "WitcherSeamless - Critical Error", MB_ICONERROR);
    }
}

// ===========================================================================
// CLEANUP FUNCTION
// ===========================================================================

void ShutdownWitcherSeamless()
{
    if (!g_initialized)
    {
        return;
    }

    try
    {
        W3mLog("=== WITCHERSEAMLESS SHUTDOWN ===");
        component_loader::pre_destroy();
        W3mLog("Components destroyed successfully");

        g_initialized = false;
    }
    catch (const std::exception& e)
    {
        W3mLog("ERROR during shutdown: %s", e.what());
    }
    catch (...)
    {
        W3mLog("UNKNOWN ERROR during shutdown");
    }
}

// ===========================================================================
// STANDARD DLL ENTRY POINT - ASI LOADER COMPATIBLE
// ===========================================================================

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(lpReserved);

    // CRITICAL: Write to log IMMEDIATELY to confirm DLL is being loaded
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        W3mLog("=== DLL_PROCESS_ATTACH BEGIN ===");
        W3mLog("Module: scripting_experiments.dll");
        W3mLog("Handle: 0x%p", hModule);
        W3mLog("Loader: Ultimate ASI Loader (d3d11.dll)");
    }

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        {
            // Disable thread library calls for performance
            DisableThreadLibraryCalls(hModule);
            W3mLog("Thread library calls disabled");

            // Initialize on a separate thread to avoid blocking the loader
            W3mLog("Creating initialization thread...");
            HANDLE hThread = CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
                W3mLog("Initialization thread started");
                InitializeWitcherSeamless();
                W3mLog("Initialization thread complete");
                return 0;
            }, nullptr, 0, nullptr);

            if (hThread == nullptr)
            {
                W3mLog("ERROR: Failed to create initialization thread!");
            }
            else
            {
                W3mLog("Initialization thread created successfully");
                CloseHandle(hThread);
            }

            W3mLog("=== DLL_PROCESS_ATTACH END ===");
        }
        break;

    case DLL_PROCESS_DETACH:
        W3mLog("=== DLL_PROCESS_DETACH BEGIN ===");
        ShutdownWitcherSeamless();
        W3mLog("=== DLL_PROCESS_DETACH END ===");
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        // Not needed - thread library calls disabled
        break;
    }

    return TRUE;
}
