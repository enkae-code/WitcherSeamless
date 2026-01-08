# WITCHERSEAMLESS MULTIPLAYER - PRODUCTION ARCHITECTURE
## Lead Architect Instructions - Refactored Standards

---

## OVERVIEW

WitcherSeamless is a **True Seamless Cooperative Multiplayer Mod** for The Witcher 3: Wild Hunt. This document outlines the **Production-Grade Refactored Architecture** that powers the mod's core synchronization systems.

---

## CORE PILLARS

### 1. STATE MACHINE ARCHITECTURE

**Session State Enum (`W3mSessionState`)**
- `W3mState_FreeRoam` - Normal gameplay with full player control
- `W3mState_Spectator` - Quest spectator mode (ghosted, frozen, teleported)
- `W3mState_Dialogue` - Active dialogue participant

**Implementation:**
- Global state variable: `g_W3mSessionState`
- Consolidated event handler: `OnSessionStateChanged()`
- Automatic teleportation for quest initiators (30m threshold)
- Player ghosting (50% transparency via `dithered_50` appearance)
- Input blocking via `thePlayer.BlockAction()` for movement/sprint/dodge/roll/jump
- State transitions broadcast via `W3mBroadcastSessionState(newState, sceneID)`

---

### 2. SHARED ECONOMY SYSTEM

**Crown Synchronization:**
- **Broadcast:** Intercept `thePlayer.inv.AddMoney()` and `thePlayer.inv.RemoveMoney()`
- **Reconciliation:** 5-second heartbeat via `W3mReconcileCrowns()` to correct UDP drops
- **Instant Gold:** Looted crowns trigger `W3mInventoryBridge()` with `isRelicOrBoss=false`

**Shared Loot Distribution:**
- **Quality Check:** Only items with `quality >= 4` (Relic/Legendary) or boss containers
- **Async Queue:** `W3mInventoryBridge` queues items off-thread to prevent frame drops
- **Deduplication:** Items tracked via `item_key = itemName_quantity` to prevent double-sync
- **Packet:** `W3mLootPacket` (item_name, quantity, player_guid, timestamp)

---

### 3. GLOBAL QUEST SYNCHRONIZATION

**Fact Broadcasting:**
- Use `AddFactWrapper()` to intercept quest state changes
- Broadcast via `W3mBroadcastFact(factName, value)`
- Lock quest choices session-wide to prevent divergence

**World State Reconciliation:**
- **Game Time:** Sync via `W3mReconcileWorldState(hostGameTime, hostWeatherID)`
- **Weather:** Request weather change if `currentWeather != hostWeather`
- **Threshold:** Only reconcile time if difference > 60 seconds

---

### 4. COMBAT & DAMAGE SYNC

**Attack Broadcasting:**
- Intercept `W3DamageAction` via hooks
- Broadcast via `W3mBroadcastAttack(attackerGuid, targetTag, damageAmount, attackType)`
- Ensure `theGame.damageMgr.ProcessAction()` is called with identical parameters on all clients

**Party Scaling:**
- **Health Multiplier:** `1.0 + (partyCount - 1) * 0.5` (e.g., 2 players = 1.5x health)
- **Boss Damage Bonus:** `(partyCount - 1) * 0.2` (e.g., 3 players = +40% boss damage)
- **Implementation:** `W3mApplyPartyScaling(npc, partyCount)` adds abilities dynamically

**NPC Death Reconciliation:**
- Broadcast NPC deaths via `W3mBroadcastNPCDeath(targetTag)`
- Force-kill NPCs on remote clients to prevent resurrection bugs

---

### 5. ASYNC PACKET QUEUE SYSTEM

**Inventory Bridge (`W3mInventoryBridge` class):**
- **Queue:** `std::queue<W3mLootPacket>` with `std::mutex` protection
- **Processing:** 100ms loop on `scheduler::async` thread
- **Reliability:** Reliable packets for inventory to prevent item loss
- **Loopback Support:** Route packets back to local client for testing

**Benefits:**
- No frame drops during loot spam (e.g., opening 10 chests)
- Prevents UDP packet loss for critical items
- Allows single-threaded WitcherScript to remain responsive

---

### 6. SECURITY & PACKET VALIDATION

**XOR Cipher (Always Active):**
- All packets are XOR-encrypted in production builds
- Cipher key rotates per session
- Visible in Live Monitor as `XOR ACTIVE` (Green)

**Silent Recovery System:**
- Malformed packets are logged and discarded without crashes
- Protocol version check: `packet.script_version == SCRIPT_VERSION`
- Max packet size: 1024 bytes (exceeding packets dropped)

**Packet Structure:**
```cpp
utils::buffer_serializer buffer{};
buffer.write(game::PROTOCOL);       // Version header
buffer.write(packet);                // Packet data
network::send(address, "type", buffer);
```

---

### 7. LIVE MONITOR OVERLAY

**HUD Display (Top-Right Corner):**
- **File:** `W3mDebugMonitor.ws`
- **Toggle:** `W3mToggleMonitor()` console command
- **Refresh:** Every frame via `W3mUpdateMonitor()`

**Displayed Metrics:**
- **Session State:** FreeRoam/Spectator/Dialogue/Offline
- **RTT (ms):** Round-trip time (0ms in loopback mode)
- **Packets/Sec:** Real-time packet throughput
- **XOR ACTIVE:** Security status (Green=Active, Red=Inactive)
- **Players:** Connected player count

**Color Coding (CDPR Hex Codes):**
- `0xFFFFFF` (White) - Headers and normal text
- `0x00FF00` (Green) - OK status (RTT < 100ms)
- `0xFFFF00` (Yellow) - Warning (RTT 100-200ms)
- `0xFF0000` (Red) - Error (RTT > 200ms or health bars)

**C++ Telemetry Backend:**
- `W3mGetNetworkStats()` RTTI-bound function
- Returns `W3mNetworkStats` struct with real-time metrics
- Packet counters: `g_telemetry.increment_sent()` / `increment_received()`
- Packets/Sec calculation: Rolling 1-second window with auto-reset

---

### 8. NATIVE UI RENDERING (C++ Layer)

**W3mNativeUI Class:**
```cpp
static constexpr uint32_t COLOR_HEALTH = 0xFF0000;    // Red
static constexpr uint32_t COLOR_TEXT = 0xFFFFFF;      // White
static constexpr uint32_t COLOR_WARNING = 0xFFFF00;   // Yellow

void draw_health_bar(player_name, health_percent, position);
void draw_player_name(name, position);
void draw_warning(message, position);
```

**WitcherScript Wrapper:**
```witcherscript
function W3mNativeUI_ShowMessage(message : string, colorHex : int)
{
    DisplayFeedMessage(message);  // Fallback to CDPR's native system
}
```

---

### 9. RECONCILIATION HEARTBEAT

**5-Second Broadcast Loop:**
- **Crowns:** Sync total party bank via `packet.total_crowns`
- **Game Time:** Sync world clock in seconds via `packet.game_time`
- **Weather:** Sync active weather effect via `packet.weather_id`
- **Version Check:** Validate `packet.script_version` to prevent mismatches

**Packet Structure (`W3mHeartbeatPacket`):**
```cpp
uint64_t player_guid;
uint32_t total_crowns;
uint32_t world_fact_hash;        // Reserved for future quest validation
uint32_t script_version;
uint32_t game_time;
uint16_t weather_id;
uint64_t timestamp;
```

---

### 10. NETWORKING STANDARDS

**Packet Reliability:**
- `Reliable` for inventory, achievements, quest locks (critical state)
- `Unreliable` for position/rotation/velocity (recoverable via interpolation)

**RunMultiplayerFrame Loop:**
- Priority tick rate: **33ms (30 FPS network sync)**
- Separate from render loop to maintain consistency during FPS drops

**Loopback Mode:**
- Enable via `W3mSetLoopback(true)` for solo testing
- Packets route back to local client without network stack
- RTT shows 0ms in Live Monitor

---

## CONSOLE COMMANDS (DEBUG & TESTING)

| Command | Description |
|---------|-------------|
| `W3mToggleMonitor()` | Enable/disable Live Monitor overlay |
| `W3mTestLoopback(true/false)` | Toggle loopback mode |
| `W3mTestSessionState(0/1/2)` | Force session state (0=FreeRoam, 1=Spectator, 2=Dialogue) |
| `W3mShareSession()` | Copy session IP to clipboard |
| `W3mTestScaling()` | Apply party scaling to nearest NPC |
| `W3mTestInventory("ItemName", 5)` | Broadcast test loot item |
| `W3mTestAchievement("EA_FindCiri")` | Broadcast test achievement |

---

## TESTING PROTOCOL - FINAL SAFETY PILLARS

### **SAFETY PILLAR 1: ATOMIC FACTS SYSTEM**
**Purpose:** Prevent database corruption during global synchronization using atomic lock-check

**Architecture:**
- **Native Wrapper**: `W3mNativeFactsAdd()` replaces direct `FactsAdd()` calls
- **Atomic Lock**: `g_W3mGlobalSyncInProgress` flag prevents concurrent operations
- **Queue System**: `g_W3mPendingFacts` array stores changes during sync
- **Lock-Check**: `W3mAtomicAddFact()` validates lock state before applying

**Test Protocol:**
1. Enable global sync: `W3mTestSessionState(1)` (Spectator mode)
2. During sync, execute: `W3mTestAtomicFact("test_quest", 1)`
3. Verify fact is **queued** (not applied instantly)
4. End sync: `W3mTestSessionState(0)` (FreeRoam mode)
5. Verify queued fact is **flushed** and applied

**Expected Logs:**
```
[W3MP ATOMIC] Fact queued during sync: test_quest = 1
[W3MP ATOMIC] Global sync completed, pending facts applied
```

**Verification Criteria:**
- ✅ Facts queued during global sync (no instant application)
- ✅ Facts flushed after sync completion
- ✅ No database corruption during concurrent operations
- ✅ **Atomic Lock** prevents race conditions
- ✅ **Native Wrapper** ensures consistent fact application

---

### **SAFETY PILLAR 2: HANDSHAKE PROTOCOL**
**Purpose:** Secure session establishment using 64-bit SessionID before gameplay packets

**Architecture:**
- **64-bit SessionID**: Cryptographically sufficient session identifier
- **Packet Blocking**: All gameplay packets (Loot, Move, Achievement) blocked until handshake complete
- **Session Validation**: `g_handshake_complete` atomic flag controls packet processing
- **Security Check**: `receive_packet_safe()` validates handshake state before processing

**Test Protocol:**
1. Start multiplayer session
2. Execute: `W3mTestHandshake()`
3. Verify **64-bit SessionID** exchange
4. Check that gameplay packets are **blocked** until handshake complete
5. Verify handshake completion enables packet processing
6. Monitor Live Monitor shows "HANDSHAKE: PENDING" → "HANDSHAKE: OK"

**Expected Logs:**
```
[W3MP] Initiating handshake with SessionID: 1234567890123456
[W3MP SECURITY] INVENTORY packet blocked - handshake not complete
[W3MP HANDSHAKE] Session established: ID=1234567890123456, Player=PlayerName
[W3MP] Handshake complete: SessionID=1234567890123456
```

**Verification Criteria:**
- ✅ **64-bit SessionID** generated and exchanged
- ✅ Gameplay packets blocked during handshake
- ✅ Handshake completion enables packet processing
- ✅ Session validation prevents unauthorized connections
- ✅ Live Monitor shows handshake status

---

### **SAFETY PILLAR 3: TIME SMOOTHING**
**Purpose:** Seamless time reconciliation without clock snapping

**Test Protocol:**
1. Get current time: `W3mGetGameTime()`
2. Simulate 7-second drift: `W3mTestTimeSmoothing()`
3. Verify gradual adjustment (3-second smoothing window)
4. Confirm no time jumps or visual glitches
5. Test large drift (>10s) for snap behavior

**Expected Logs:**
```
[W3MP TIME] Starting smooth adjustment: 7.0s drift
[W3MP TIME] Smoothing progress: 33%
[W3MP TIME] Smoothing progress: 66%
[W3MP TIME] Smooth adjustment completed, time synced
```

**Verification Criteria:**
- ✅ Small drift (5-10s) uses smoothing (SetTimeMultiplier)
- ✅ Large drift (>10s) snaps immediately
- ✅ No visual time jumps during smoothing
- ✅ Time multiplier restored after smoothing

---

## PRODUCTION VERIFICATION CHECKLIST

### **Core Functionality:**
- [ ] Live Monitor shows `XOR ACTIVE` in green
- [ ] Loopback mode displays RTT as 0ms
- [ ] Packets/Sec > 0 when network activity occurs
- [ ] Session state transitions (FreeRoam → Spectator) work smoothly
- [ ] Crown reconciliation syncs within 5 seconds
- [ ] Shared loot appears for all players
- [ ] Party scaling applies correct health multipliers
- [ ] No frame drops during inventory spam (async queue test)
- [ ] Malformed packets are silently discarded (no crashes)

### **Safety Pillars:**
- [ ] **Atomic Facts**: Facts queued during global sync, flushed after completion
- [ ] **Handshake Protocol**: 64-bit SessionID exchange before gameplay packets
- [ ] **Time Smoothing**: Gradual adjustment for 5-10s drift, snap for >10s

### **Security Validation:**
- [ ] XOR cipher active (green indicator)
- [ ] Packet size validation (≤1024 bytes)
- [ ] Silent Recovery handles malformed packets
- [ ] Session validation prevents unauthorized connections

### **Performance Metrics:**
- [ ] 33ms network tick rate maintained
- [ ] Async queue prevents frame drops
- [ ] RTT < 100ms in normal conditions
- [ ] Memory usage stable during extended sessions

---

## ARCHITECTURE NOTES

**Zero-Bloat Philosophy:**
- No redundant hooks or duplicate listeners
- Single consolidated event handlers (e.g., `OnSessionStateChanged`)
- Minimal WitcherScript footprint (all heavy lifting in C++)

**CDPR Polish Standards:**
- Use native CDPR color codes (0xFF0000, 0xFFFFFF, 0xFFFF00)
- Respect REDengine threading model (scheduler::main for script calls)
- Follow CDPR naming conventions (`W3m` prefix, PascalCase for functions)

**Maintainability:**
- All packet structures documented in `protocol.hpp`
- Clear separation: WitcherScript (game logic) ↔ C++ (networking/performance)
- Silent Recovery ensures one bad packet doesn't crash the session

---

## FINAL NOTES FOR LEAD ARCHITECT

This refactored architecture is **production-ready**. All systems prioritize:

1. **Smoothness** - 33ms network tick, async queues, no frame drops
2. **Reliability** - XOR encryption, packet validation, silent recovery
3. **Seamlessness** - Shared economy, global quests, synchronized combat
4. **Visibility** - Live Monitor for real-time diagnostics

Always use `Reliable` packets for state changes. Always validate protocol version. Always test in loopback mode first.

**RunMultiplayerFrame must never exceed 33ms latency.**
