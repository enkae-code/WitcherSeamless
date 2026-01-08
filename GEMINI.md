# Gemini CLI - Senior Architect Instructions: Witcher3Seamless Engine

## 1. PROJECT VISION
Building a high-performance, native C++ multiplayer bridge for The Witcher 3 (RedEngine 3). We avoid high-level scripting bloat by hooking directly into engine memory and the DirectX 12 graphics pipeline.

## 2. CORE STANDARDS & ARCHITECTURE
- **Memory Hook Strategy**: 'bink2w64.dll' Hijack (Plan B). 
  - The engine MUST load this to play video. We act as the proxy to load 'scripting_experiments.dll'.
- **The Bridge Logic**: 
  - C++ Backend: Handles networking (libcurl), 64-bit SessionID exchange, and memory patching.
  - WitcherScript Frontend: Handles UI (W3mDebugMonitor.ws) and game-side event triggers.
- **Zero-Bloat Enforcement**: NO NEW SCRIPTS. Only 'Launch_W3MP.bat'.
- **Deployment Accuracy**: 
  - DLL must land in: 'bin\x64_dx12\plugins\'
  - WS Scripts must land in: 'content\content0\scripts\game\multiplayer\'

## 3. ENGINE-SPECIFIC TRUTHS (REDENGINE 3)
- **DirectX 12 Focus**: Always target 'bin\x64_dx12'. 
- **Script Compilation**: Must force 'scripts.blob' deletion if WS changes aren't appearing.
- **Console Interface**: Mapping to 'F2' via 'W3M_Input.xml' is mandatory for 60% keyboards.
- **Logging**: 'W3M_Debug.log' is the single source of truth for C++ registration. 21 functions must be registered.

## 4. MULTIPLAYER HANDSHAKE LOGIC
- **W3mTestHandshake()**: Verifies the C++/WitcherScript bridge.
- **W3mShareSession()**: Copies the networked IP/SessionID to the Windows clipboard.
- **W3mToggleMonitor()**: Toggles the 10ms-refresh telemetry overlay.

## 5. FORBIDDEN ACTIONS
- DO NOT use WSL or Linux-style '-c' flags (causes Windows shell crashes).
- DO NOT download 'RED3ext' (outdated/broken). Use the 'Ultimate ASI Loader' only.
- DO NOT create redundant .bat files. Perform 'powershell' purges on any duplicates.