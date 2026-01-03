# Witcher 3 Seamless Multiplayer: Core Architecture

## 1. Project Philosophy
- Role: Senior Full-Stack Architect.
- Style: Zero-Bloat, Native Web APIs, Defensive Functional Patterns.
- Standard: "Elden Ring" style seamless co-op (Sync everything).

## 2. Technical Map & Depot
- Native Depot: C:\Users\hertz\Desktop\W3_Redkit_Depot\
- Logic Source: data/scripts/w3mp.ws
- C++ Bridge: src/client/module/scripting_experiments.cpp
- Networking: src/common/network/ (UDP based)

## 3. UI & Scaling Rules
- Native HUD: 1 Local + 2 Remote (Slots 0 & 1).
- Scaling Bypass: 3+ Remote players MUST use 'UpdateFloatingUI()' ASCII nameplates/health bars. Never assume a 3-player limit.
- UI Logic: thePlayer.GetVisualDebug().AddText() for 3D world-space ASCII bars.

## 4. Sync & Networking Optimizations
- Protocol: Use W3mFactPacket and W3mAttackPacket in protocol.hpp.
- Header Optimization: Do NOT write redundant 'packet_type' to the buffer; 'network::on' handles dispatch.
- Shared Purse: All crowns (AddItem('Crowns')) must be broadcast via the Heartbeat system.
- Heartbeat: Implement a 5s Reconciliation Heartbeat for world state and total currency to correct UDP drops.