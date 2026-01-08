# WitcherSeamless: Hardened Alpha Engine

A "Zero-Bloat," high-velocity multiplayer synchronization engine for The Witcher 3: Wild Hunt. This project replaces legacy text-based protocols with a hardened binary serialization system.

## 🛡️ Core Engineering Pillars
* **Protocol-First Design**: Custom binary serialization via `protocol.hpp` for minimal latency.
* **Security Hardened**: Strict 8KB packet ceiling enforced at the network manager level to prevent buffer overflow exploits.
* **High-Velocity Sync**: Native C++/WitcherScript bridge for frame-perfect attack and quest fact replication.

## 🛠️ Tech Stack
* **Engine**: C++20 with custom component-based architecture.
* **Scripting**: Native WitcherScript (WS) integration via custom CFunction allocators.
* **Networking**: Low-overhead UDP with binary packet reconstruction.

## 🚀 Alpha Roadmap
- [x] **P0: Hardened Networking**: Binary protocol and 8KB security shield.
- [x] **P0: Combat Bridge**: `W3mSendAttack` API and attack serialization.
- [x] **P0: Quest Sync**: Fact-broadcasting for story progression parity.
- [ ] **P1: Position Interpolation**: Fixing character snapping via 3-snapshot ring buffers (In Progress).

## 📦 Installation (Alpha)
1. Place `w3m.asi` and `dinput8.dll` in the game's `bin/x64/` directory.
2. Configure your session in `w3m_server.txt`.
3. Ensure the `redkit_scripts` symlink is active for local development context.
