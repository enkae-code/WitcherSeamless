// ===========================================================================
// WITCHERSEAMLESS - SHARED LOGGING INFRASTRUCTURE
// ===========================================================================
// W3mLog macro for debugging - writes to W3M_Debug.log
// Shared between asi_loader_entry.cpp and scripting_experiments_refactored.cpp
// ===========================================================================

#pragma once

#include <fstream>
#include <cstdio>

// ===========================================================================
// DEBUG LOGGING MACRO - W3M_DEBUG.LOG
// ===========================================================================

#define W3M_LOG_FILE "W3M_Debug.log"

#define W3mLog(format, ...) \
    do { \
        std::ofstream log_file(W3M_LOG_FILE, std::ios::app); \
        if (log_file.is_open()) { \
            char buffer[512]; \
            snprintf(buffer, sizeof(buffer), format, ##__VA_ARGS__); \
            log_file << "[W3MP] " << buffer << std::endl; \
            log_file.close(); \
        } \
    } while(0)
