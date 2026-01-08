# ===========================================================================
# WITCHERSEAMLESS MULTIPLAYER - DLL BUILD CONFIGURATION
# ===========================================================================
# Refactored architecture using scripting_experiments_refactored.cpp
# Builds as SHARED library (DLL) for game plugin injection
# ===========================================================================

# Core refactored multiplayer source
set(REFACTORED_SOURCES
  asi_loader_entry.cpp
  module/scripting_experiments_refactored.cpp
  module/network.cpp
  module/network.hpp
  module/scheduler.cpp
  module/scheduler.hpp
  module/scripting.cpp
  module/scripting.hpp
  module/properties.cpp
  module/properties.hpp
  module/renderer.cpp
  module/renderer.hpp
  module/steam_proxy.cpp
  module/steam_proxy.hpp
  module/game_path.cpp
  module/game_path.hpp
  loader/component_loader.cpp
  loader/component_loader.hpp
  loader/loader.cpp
  loader/loader.hpp
  utils/identity.cpp
  utils/identity.hpp
  utils/smbios.cpp
  utils/smbios.hpp
  steam/interface.cpp
  steam/interface.hpp
  std_include.hpp
)

# Build as SHARED library (DLL) for game injection
add_library(scripting_experiments SHARED ${REFACTORED_SOURCES})

momo_assign_source_group(${REFACTORED_SOURCES})

target_precompile_headers(scripting_experiments PRIVATE std_include.hpp)

# Link against required libraries with static linking
target_link_libraries(scripting_experiments PRIVATE
  version
  common
  libcurl_static
  udis86
  asmjit
  minhook
  ws2_32
  crypt32
  wldap32
  normaliz
)

# Force static linking for CRT and dependencies
set_target_properties(scripting_experiments PROPERTIES
  OUTPUT_NAME "scripting_experiments"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/artifacts-release"
  LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/artifacts-release"
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# Static linking flags for self-contained DLL
if(MSVC)
  target_compile_options(scripting_experiments PRIVATE /MT$<$<CONFIG:Debug>:d>)
  target_link_options(scripting_experiments PRIVATE
    /NODEFAULTLIB:msvcrt.lib
    /NODEFAULTLIB:msvcrtd.lib
    /INCREMENTAL:NO
  )
endif()

# Ensure the artifacts directory exists first
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/artifacts-release")

# Only attempt to copy if the data directory actually exists to avoid build failures
if(EXISTS "${CMAKE_SOURCE_DIR}/data")
    file(COPY "${CMAKE_SOURCE_DIR}/data" DESTINATION "${CMAKE_BINARY_DIR}/artifacts-release")
endif()

# Set as startup project for debugging
set_property(GLOBAL PROPERTY VS_STARTUP_PROJECT scripting_experiments)
