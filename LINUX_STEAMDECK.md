# 🐧 Steam Deck & Linux Optimization Guide

WitcherSeamless is fully compatible with Linux via Valve's Proton compatibility layer. Follow these steps to ensure the `W3m.dll` hook is correctly injected.

## 🛠️ Injection Setup
Since Proton handles DLLs differently than native Windows, you must set a launch override:
1. Right-click **The Witcher 3** in your Steam Library.
2. Select **Properties** > **General**.
3. In the **Launch Options** field, paste the following:
   `WINEDLLOVERRIDES="W3m=n,b" %command%`

*Note: This tells Proton to load our 'Native' (n) DLL first, falling back to 'Built-in' (b) if necessary.*

## ⚡ Performance Tuning
To maintain 60fps for the **Midnight UI** on Steam Deck:
- Use **Proton Experimental** or **Proton GE** for the best compatibility with DX12.
- If you experience input lag, ensure "Allow Tearing" is enabled in the Steam Deck's Quick Access Menu to reduce latency in the message pump.
