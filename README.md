# WitcherSeamless v0.1.0-alpha

A high-velocity, native C++ co-op engine for The Witcher 3 (DX12). Built for zero-bloat performance and high-fidelity state synchronization.

## ⚡ Architectural Pillars
- **Zero-Jank Input**: Event-driven `WH_GETMESSAGE` hooks bypass the standard engine polling for instant UI response.
- **Midnight UI**: A premium, semi-transparent dashboard primitive (Alt+S) built with custom O(height) line-fill rendering.
- **Atomic Sync**: Narrative facts and quest states are synchronized via a failure-resistant atomic protocol.
- **Dead-Reckoning**: High-frequency world-space interpolation ensuring 60fps smooth player movement even under network jitter.

## 🎮 Feature Set
### 🛠️ The Interactive Dashboard
Press `Alt + S` to activate the Midnight Palette.
* **Session Management**: Instant joining via `join [IP:PORT]`.
* **Network Stress-Testing**: Live packet loss and latency simulation via `chaos [ms] [loss%]`.
* **Handshake Protocol**: Encrypted string-based session hashing for secure peer discovery.

### ⚔️ Combat & World Sync
* **Dynamic Scaling**: Enemy HP and damage parameters scale in real-time based on the 5-Witcher party size.
* **Animation Parity**: Synchronized combat state-machines ensuring hit-registration consistency across peers.

## 🛠️ Engineering Standards
This repository adheres to **Senior Full-Stack** architectural standards:
* **Memory Safety**: Functional TypeScript patterns translated to C++ logic.
* **Style**: Strict LLVM 20 (Clang-Format) compliance.
* **Reliability**: Fully automated Matrix Builds (Windows/Linux) via GitHub Actions.

## 📦 Installation
1. Download the `artifacts-windows.zip` from the latest [Release](https://github.com/enkae-code/WitcherSeamless/releases).
2. Extract `scripting_experiments.dll` (renamed to `W3m.dll`) to your game's `bin/x64_dx12` folder.
3. Launch the game and press `Alt + S` to verify the Midnight UI.

## 🐧 Platform Support
* **Linux / Steam Deck**: Fully supported via Proton. See the [Linux & Steam Deck Optimization Guide](LINUX_STEAMDECK.md) for injection steps and performance tuning.

## ⚠️ System Requirements
- **Game Version**: The Witcher 3: Wild Hunt - Next-Gen Update (v4.0 or higher).
- **Renderer**: **DirectX 12 (Required)**. 
- **Platform**: PC / x64.
- **Note**: Classic DX11 (v1.32) is currently unsupported. Support for legacy versions is planned for future iterations.