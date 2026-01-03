// ===========================================================================
// Witcher 3: Seamless Multiplayer - WitcherScript Bridge
// ===========================================================================
// Architecture: C++ Bridge <-> WitcherScript <-> Game Engine
// Player Limit: HARD-CODED 3 PLAYERS (1 local + 2 remote max)
// HUD Limit: Native companion HUD supports exactly 2 companions
// Performance: Runs at 33.3Hz (0.03s tick) - Keep operations lightweight
// Animation Sync: Captures and replicates combat stance, movement speed, locomotion
// True Co-op: Quest facts, combat damage, and cutscene synchronization (framework ready)
// ===========================================================================

@addField(CR4Game)
var w3mStateMachine : W3mStateMachine;

@addMethod(CR4Game)
public function InitializeMultiplayer()
{
    if (!w3mStateMachine)
    {
        w3mStateMachine = new W3mStateMachine in this;
        w3mStateMachine.Start();
    }
}

function StartMultiplayer()
{
    theGame.InitializeMultiplayer();
}

// ===========================================================================
// PROTOCOL STRUCTS - Must match C++ memory layout exactly
// ===========================================================================

struct W3mPlayerState
{
    var angles : EulerAngles;   // Roll, Pitch, Yaw (floats)
    var position : Vector;      // X, Y, Z, W (floats)
    var velocity : Vector;      // X, Y, Z, W (floats)
    var speed : float;          // Absolute speed
    var moveType : int;         // Movement type enum (0-4)
    var combatStance : int;     // Combat stance (0-3): Normal, AlertNear, AlertFar, Guarded
    var explorationAction : int; // Exploration action enum
}

struct W3mPlayer
{
    var guid : Uint64;                      // Unique player identifier
    var playerName : string;                // Player display name
    var playerState : array<W3mPlayerState>; // Array size 0 (no update) or 1 (delta)
}

// ===========================================================================
// C++ BRIDGE IMPORTS - Native functions exposed from scripting_experiments.cpp
// ===========================================================================

import function W3mPrint(msg : string);
import function W3mSetNpcDisplayName(npc : CNewNPC, npcName : string);
import function W3mStorePlayerState(playerState : W3mPlayerState);
import function W3mGetPlayerStates() : array<W3mPlayer>;
import function W3mUpdatePlayerName(playerName : string);
import function W3mGetMoveType(movingAgent : CMovingAgentComponent) : int;
import function W3mSetSpeed(movingAgent : CMovingAgentComponent, absSpeed: float);

// True Co-op Bridge Functions
import function W3mBroadcastFact(factName : string, value : int);
import function W3mBroadcastAttack(attackerGuid : Uint64, targetTag : name, damageAmount : float, attackType : int);
import function W3mBroadcastCutscene(cutscenePath : string, position : Vector, rotation : EulerAngles);
import function W3mBroadcastAnimation(animName : string, explorationAction : int);
import function W3mBroadcastVehicleMount(vehicleTemplate : string, isMounting : bool, position : Vector, rotation : EulerAngles);
import function W3mApplyPartyScaling(npc : CNewNPC, partyCount : int);
import function W3mBroadcastQuestLock(isLocked : bool, sceneID : int);
import function W3mBroadcastLoot(itemName : string, quantity : int, isRelicOrBoss : bool);
import function W3mBroadcastAchievement(achievementID : string);
import function W3mBroadcastNPCDeath(targetTag : name);
import function W3mSetLoopback(enabled : bool);
import function W3mCopyIP();

// Reconciliation Heartbeat Functions (called from C++)
function W3mGetTotalCrowns() : int
{
    var crowns : int;
    
    if (!thePlayer || !thePlayer.inv)
    {
        return 0;
    }
    
    // Use native GetItemQuantityByName to get total crowns
    crowns = thePlayer.inv.GetItemQuantityByName('Crowns');
    return crowns;
}

function W3mReconcileCrowns(totalCrowns : int, playerGuid : Uint64)
{
    var currentCrowns : int;
    var crownDiff : int;
    
    if (!thePlayer || !thePlayer.inv)
    {
        return;
    }
    
    currentCrowns = thePlayer.inv.GetItemQuantityByName('Crowns');
    crownDiff = totalCrowns - currentCrowns;
    
    // Only reconcile if there's a significant mismatch (> 10 crowns)
    if (AbsF(crownDiff) > 10)
    {
        if (crownDiff > 0)
        {
            // Add missing crowns
            thePlayer.inv.AddAnItem('Crowns', crownDiff);
            DisplayFeedMessage("[HEARTBEAT] Synced +" + crownDiff + " crowns");
        }
        else
        {
            // Remove excess crowns
            thePlayer.inv.RemoveItemByName('Crowns', -crownDiff);
            DisplayFeedMessage("[HEARTBEAT] Synced " + crownDiff + " crowns");
        }
    }
}

function W3mUpdateFloatingUI(playerCount : int)
{
    // This function is called from C++ when playerCount >= 3
    // The actual floating UI rendering is handled in UpdateFloatingUI() per-player
    // This is just a notification that we're in overflow mode
    
    // Optional: Could display a message when entering/exiting overflow mode
    // For now, the per-player UpdateFloatingUI() handles all rendering
}

// Animation Sync Functions (called from C++)
function W3mApplyRemoteAnimation(playerName : string, animName : string, explorationAction : int)
{
    var remoteActor : CActor;
    var remoteNPC : CNewNPC;
    var entities : array<CEntity>;
    var i : int;
    
    // Find remote player entity by name
    FindGameplayEntitiesInRange(entities, thePlayer, 100.0, 100, 'w3m_RemotePlayer');
    
    for (i = 0; i < entities.Size(); i += 1)
    {
        remoteNPC = (CNewNPC)entities[i];
        if (remoteNPC && remoteNPC.GetDisplayName() == playerName)
        {
            remoteActor = (CActor)remoteNPC;
            break;
        }
    }
    
    if (!remoteActor)
    {
        W3mPrint("[W3MP] Cannot find remote player: " + playerName);
        return;
    }
    
    // Apply animation based on exploration action
    switch (explorationAction)
    {
        case 2: // PEA_Meditation
            remoteActor.ActionPlaySlotAnimationAsync('PLAYER_SLOT', 'man_npc_sit_ground_eat_01_idle', 0.2, 0.2);
            break;
        case 17: // PEA_PourPotion
            remoteActor.ActionPlaySlotAnimationAsync('PLAYER_SLOT', 'man_npc_drink_01', 0.2, 0.2);
            break;
        default:
            // Generic slot animation
            if (animName != "")
            {
                remoteActor.ActionPlaySlotAnimationAsync('PLAYER_SLOT', animName, 0.2, 0.2);
            }
            break;
    }
    
    W3mPrint("[W3MP] Applied animation " + animName + " to " + playerName);
}

// NPC Death Reconciliation (called from C++)
function W3mForceKillNPC(targetTag : name)
{
    var targetNPC : CNewNPC;
    
    // Find NPC by tag
    targetNPC = theGame.GetNPCByTag(targetTag);
    if (!targetNPC)
    {
        W3mPrint("[W3MP] Cannot find NPC with tag: " + NameToString(targetTag));
        return;
    }
    
    // Force kill if not already dead
    if (targetNPC.IsAlive())
    {
        targetNPC.Kill('W3MP_ForceKill', false, thePlayer);
        W3mPrint("[W3MP] Force killed NPC: " + NameToString(targetTag));
    }
}

// Version Mismatch Handler (called from C++)
function W3mVersionMismatch(remoteVersion : int, localVersion : int)
{
    var message : string;
    
    message = "VERSION MISMATCH! Remote: v" + remoteVersion + ", Local: v" + localVersion;
    DisplayFeedMessage(message);
    DisplayCenterMessage("Script version mismatch detected! Sync disabled.");
    
    W3mPrint("[W3MP] " + message);
}

// ===========================================================================
// SHARED WORLD SYNC - Time, Weather, and Vehicle Functions
// ===========================================================================

// Get current game time in seconds (called from C++)
function W3mGetGameTime() : int
{
    var gameTime : GameTime;
    var seconds : int;
    
    gameTime = theGame.GetGameTime();
    seconds = GameTimeToSeconds(gameTime);
    
    return seconds;
}

// Get current weather ID (called from C++)
function W3mGetWeatherID() : int
{
    var weatherEffect : EWeatherEffect;
    
    weatherEffect = GetCurWeather();
    return (int)weatherEffect;
}

// Reconcile world time and weather (called from C++)
function W3mReconcileWorldState(hostGameTime : int, hostWeatherID : int)
{
    var currentGameTime : GameTime;
    var currentSeconds : int;
    var timeDiff : int;
    var newGameTime : GameTime;
    var currentWeather : EWeatherEffect;
    var hostWeather : EWeatherEffect;
    
    // Get current local time
    currentGameTime = theGame.GetGameTime();
    currentSeconds = GameTimeToSeconds(currentGameTime);
    timeDiff = AbsI(currentSeconds - hostGameTime);
    
    // Reconcile time if difference > 60 seconds (1 minute)
    if (timeDiff > 60)
    {
        newGameTime = GameTimeCreateFromGameSeconds(hostGameTime);
        theGame.SetGameTime(newGameTime, false);
        DisplayFeedMessage("[WORLD SYNC] Time adjusted: " + timeDiff + "s difference");
        W3mPrint("[W3MP] Time reconciled: " + timeDiff + " seconds");
    }
    
    // Reconcile weather if different
    currentWeather = GetCurWeather();
    hostWeather = (EWeatherEffect)hostWeatherID;
    
    if (currentWeather != hostWeather)
    {
        // Request weather change to match host
        // Note: Weather changes are gradual, not instant
        RequestWeatherChange(hostWeather);
        DisplayFeedMessage("[WORLD SYNC] Weather syncing to host");
        W3mPrint("[W3MP] Weather reconciled: " + hostWeatherID);
    }
}

// Request weather change helper
function RequestWeatherChange(targetWeather : EWeatherEffect)
{
    // Use environment manager to request weather change
    // This is a gradual transition, not instant
    switch (targetWeather)
    {
        case EWE_Clear:
            theGame.RequestWeatherChange('WT_Clear', 1.0, true);
            break;
        case EWE_Rain:
            theGame.RequestWeatherChange('WT_Rain_Storm', 1.0, true);
            break;
        case EWE_Snow:
            theGame.RequestWeatherChange('WT_Snow', 1.0, true);
            break;
        default:
            theGame.RequestWeatherChange('WT_Clear', 1.0, true);
            break;
    }
}

// Loot Receive Wrapper (called from C++)
function W3mReceiveLoot(itemName : string, quantity : int, playerName : string)
{
    var itemNameAsName : name;
    var addedItems : array<SItemUniqueId>;
    
    if (!thePlayer || !thePlayer.inv)
    {
        return;
    }
    
    // Convert string to name for native function
    itemNameAsName = StringToName(itemName);
    
    // Add item to player inventory
    addedItems = thePlayer.inv.AddAnItem(itemNameAsName, quantity);
    
    // Display HUD notification
    if (addedItems.Size() > 0)
    {
        DisplayFeedMessage("[W3MP] Received Shared Loot: " + itemName + " x" + quantity + " (from " + playerName + ")");
        W3mPrint("[W3MP] Loot received: " + itemName + " x" + quantity);
    }
}

// Achievement Unlock Wrapper (called from C++)
function W3mUnlockAchievement(achievementID : string)
{
    var achievementName : name;
    
    // Convert string to name for native function
    achievementName = StringToName(achievementID);
    
    // Call native unlock function
    theGame.UnlockAchievement(achievementName);
    
    // Display notification
    DisplayFeedMessage("[ACHIEVEMENT] Unlocked: " + achievementID);
    W3mPrint("[W3MP] Achievement unlocked: " + achievementID);
}

// Vehicle Sync: Apply remote player vehicle mount/dismount (called from C++)
function W3mApplyVehicleSync(playerName : string, vehicleTemplate : string, isMounting : bool, position : Vector, rotation : EulerAngles)
{
    var remoteActor : CActor;
    var remoteNPC : CNewNPC;
    var entities : array<CEntity>;
    var vehicleEntity : CEntity;
    var template : CEntityTemplate;
    var i : int;
    
    // Find remote player entity by name
    FindGameplayEntitiesInRange(entities, thePlayer, 100.0, 100, 'w3m_RemotePlayer');
    
    for (i = 0; i < entities.Size(); i += 1)
    {
        remoteNPC = (CNewNPC)entities[i];
        if (remoteNPC && remoteNPC.GetDisplayName() == playerName)
        {
            remoteActor = (CActor)remoteNPC;
            break;
        }
    }
    
    if (!remoteActor)
    {
        W3mPrint("[W3MP] Cannot find remote player: " + playerName);
        return;
    }
    
    if (isMounting)
    {
        // Spawn vehicle entity at position
        template = (CEntityTemplate)LoadResource(vehicleTemplate, true);
        if (template)
        {
            vehicleEntity = theGame.CreateEntity(template, position, rotation);
            if (vehicleEntity)
            {
                // TODO: Attach remote player to vehicle
                // This requires vehicle component access which may need C++ hooks
                W3mPrint("[W3MP] Spawned vehicle for " + playerName + ": " + vehicleTemplate);
            }
        }
    }
    else
    {
        // Dismount - remove vehicle entity
        // TODO: Find and destroy vehicle entity associated with this player
        W3mPrint("[W3MP] " + playerName + " dismounted vehicle");
    }
}

// ===========================================================================
// MOVEMENT TYPE CONVERSION
// ===========================================================================

function ConvertToMoveType(moveType : int) : EMoveType
{
    switch(moveType)
    {
        case 0: return MT_Walk;
        case 1: return MT_Run;
        case 2: return MT_FastRun;
        case 3: return MT_Sprint;
        case 4: return MT_AbsSpeed;
        default: return MT_Walk;
    }
}

// ===========================================================================
// LOCAL PLAYER STATE TRANSMISSION
// ===========================================================================

function TransmitPlayerState(actor : CActor)
{
    var playerState : W3mPlayerState;
    var movingAgent : CMovingAgentComponent;
    var witcher : W3PlayerWitcher;

    if (!actor)
    {
        return;
    }

    movingAgent = actor.GetMovingAgentComponent();
    if (!movingAgent)
    {
        return;
    }

    witcher = (W3PlayerWitcher)actor;

    playerState.position = actor.GetWorldPosition();
    playerState.angles = actor.GetWorldRotation();
    playerState.velocity = movingAgent.GetVelocity();
    playerState.speed = movingAgent.GetSpeed();
    playerState.moveType = W3mGetMoveType(movingAgent);

    // Capture combat stance and exploration action
    if (witcher)
    {
        playerState.combatStance = (int)witcher.GetPlayerCombatStance();
        playerState.explorationAction = 0; // PEA_None for now
    }
    else
    {
        playerState.combatStance = 0; // PCS_Normal
        playerState.explorationAction = 0; // PEA_None
    }

    W3mStorePlayerState(playerState);
}

function TransmitCurrentPlayerState()
{
    TransmitPlayerState((CActor)thePlayer);
}

// ===========================================================================
// REMOTE PLAYER STATE APPLICATION
// ===========================================================================

function ApplyPlayerState(actor : CActor, player : W3mPlayer)
{
    var movingAgent : CMovingPhysicalAgentComponent;
    var actorPos : Vector;
    var targetPos : Vector;
    var playerState : W3mPlayerState;
    var angleHeading : float;

    if (!actor)
    {
        return;
    }

    // Delta compression: Empty state = no update
    if (player.playerState.Size() < 1)
    {
        return;
    }

    playerState = player.playerState[0];

    // Update display name
    W3mSetNpcDisplayName((CNewNPC)actor, player.playerName);

    actorPos = actor.GetWorldPosition();
    targetPos = playerState.position;

    // Z-axis smoothing: Only update if vertical diff > 25cm
    if (AbsF(actorPos.Z - targetPos.Z) < 0.25)
    {
        targetPos.Z = actorPos.Z;
    }

    movingAgent = (CMovingPhysicalAgentComponent)actor.GetMovingAgentComponent();
    if (!movingAgent)
    {
        return;
    }

    angleHeading = VecHeading(RotForward(playerState.angles));

    actor.TeleportWithRotation(targetPos, playerState.angles);

    movingAgent.SetMoveType(ConvertToMoveType(playerState.moveType));
    movingAgent.ApplyVelocity(playerState.velocity);
    movingAgent.SetGameplayMoveDirection(angleHeading);

    W3mSetSpeed(movingAgent, playerState.speed);

    // Apply animation states
    ApplyAnimationState(actor, playerState);
}

// ===========================================================================
// ANIMATION STATE APPLICATION
// ===========================================================================

function ApplyAnimationState(actor : CActor, playerState : W3mPlayerState)
{
    var moveSpeed : float;
    var combatStance : EPlayerCombatStance;

    if (!actor)
    {
        return;
    }

    // Apply combat stance as integer behavior variable
    combatStance = ConvertToCombatStance(playerState.combatStance);
    actor.SetBehaviorVariable('combatStance', playerState.combatStance);

    // Apply movement speed for animation blending
    moveSpeed = playerState.speed;
    actor.SetBehaviorVariable('speed', moveSpeed);

    // Set movement direction for animation system
    if (moveSpeed > 0.1)
    {
        // Moving - ensure proper locomotion
        actor.SetBehaviorVariable('isMoving', 1);
    }
    else
    {
        // Idle
        actor.SetBehaviorVariable('isMoving', 0);
    }
}

function ConvertToCombatStance(stanceInt : int) : EPlayerCombatStance
{
    switch(stanceInt)
    {
        case 0: return PCS_Normal;
        case 1: return PCS_AlertNear;
        case 2: return PCS_AlertFar;
        case 3: return PCS_Guarded;
        default: return PCS_Normal;
    }
}

// ===========================================================================
// REMOTE PLAYER ENTITY FACTORY
// ===========================================================================

function AddAndEquipItem(npc : CNewNPC, item : name)
{
    var ids : array<SItemUniqueId>;

    if (!npc)
    {
        return;
    }

    ids = npc.GetInventory().AddAnItem(item, 1);
    if (ids.Size() > 0)
    {
        npc.EquipItem(ids[0]);
    }
}

function CreateRemotePlayerEntity() : CEntity
{
    var pos : Vector;
    var rot : EulerAngles;
    var ent : CEntity;
    var npc : CNewNPC;
    var template : CEntityTemplate;
    var followerMovingAgent : CMovingAgentComponent;
    var tags : array<name>;

    tags.PushBack('w3m_RemotePlayer');

    rot = thePlayer.GetWorldRotation();
    pos = thePlayer.GetWorldPosition();

    template = (CEntityTemplate)LoadResource("characters/npc_entities/main_npc/geralt_npc.w2ent", true);
    if (!template)
    {
        return NULL;
    }

    ent = theGame.CreateEntity(template, pos, rot, , , , , tags);
    if (!ent)
    {
        return NULL;
    }

    npc = (CNewNPC)ent;

    // Equip default Geralt gear
    AddAndEquipItem(npc, 'Autogen steel sword');
    AddAndEquipItem(npc, 'Autogen silver sword');
    AddAndEquipItem(npc, 'Autogen Pants');
    AddAndEquipItem(npc, 'Autogen Gloves');
    AddAndEquipItem(npc, 'Autogen Boots');
    AddAndEquipItem(npc, 'Autogen Armor');
    AddAndEquipItem(npc, 'Shaved With Tail Hairstyle');
    AddAndEquipItem(npc, 'head_3');

    npc.AddAbility('_canBeFollower', true);

    followerMovingAgent = npc.GetMovingAgentComponent();
    if (followerMovingAgent)
    {
        followerMovingAgent.SetGameplayRelativeMoveSpeed(0.0f);
        followerMovingAgent.SetDirectionChangeRate(0.16);
        followerMovingAgent.SetMaxMoveRotationPerSec(60);
    }

    npc.GotoState('NewIdle', false);
    npc.SetAttitude(thePlayer, AIA_Neutral);

    W3mSetNpcDisplayName(npc, "W3M Player");

    return ent;
}

// ===========================================================================
// REMOTE PLAYER MANAGER - Fixed pool of 2 remote entities
// ===========================================================================

class RemotePlayerManager
{
    // HARD-CODED: Maximum 2 remote players (3 total including local)
    // This matches the native companion HUD limit (2 companions max)
    private const var MAX_REMOTE_PLAYERS : int;
    default MAX_REMOTE_PLAYERS = 2;

    private var remoteEntities : array<CEntity>;
    private var activeGUIDs : array<Uint64>;
    private var assignedHUDSlots : array<int>; // Tracks which HUD slot each remote player is assigned to (-1 = no HUD)

    public function Initialize()
    {
        remoteEntities.Clear();
        activeGUIDs.Clear();
        assignedHUDSlots.Clear();
    }

    public function UpdateRemotePlayers(playerStates : array<W3mPlayer>)
    {
        var i : int;
        var playerCount : int;
        var player : W3mPlayer;

        playerCount = Min(playerStates.Size(), MAX_REMOTE_PLAYERS);

        // Adjust entity pool to match active player count
        AdjustEntityPool(playerCount);

        // Update active players
        for (i = 0; i < playerCount; i += 1)
        {
            player = playerStates[i];

            if (i < remoteEntities.Size() && remoteEntities[i])
            {
                ApplyPlayerState((CActor)remoteEntities[i], player);

                // Track GUID for this slot
                if (i < activeGUIDs.Size())
                {
                    activeGUIDs[i] = player.guid;
                }
                else
                {
                    activeGUIDs.PushBack(player.guid);
                }
            }
        }
    }

    private function AdjustEntityPool(targetCount : int)
    {
        var currentEntity : CEntity;

        // Remove excess entities
        while (remoteEntities.Size() > targetCount)
        {
            currentEntity = remoteEntities[remoteEntities.Size() - 1];
            remoteEntities.Remove(currentEntity);

            if (currentEntity)
            {
                currentEntity.Destroy();
            }

            if (activeGUIDs.Size() > 0)
            {
                activeGUIDs.Remove(activeGUIDs[activeGUIDs.Size() - 1]);
            }
        }

        // Add missing entities
        while (remoteEntities.Size() < targetCount)
        {
            currentEntity = CreateRemotePlayerEntity();
            if (currentEntity)
            {
                remoteEntities.PushBack(currentEntity);
            }
            else
            {
                // Entity creation failed - abort to prevent infinite loop
                break;
            }
        }
    }

    public function Shutdown()
    {
        // Clean up HUD before destroying entities
        CleanupHUD();

        AdjustEntityPool(0);
        remoteEntities.Clear();
        activeGUIDs.Clear();
        assignedHUDSlots.Clear();
    }

    public function GetRemoteEntityCount() : int
    {
        return remoteEntities.Size();
    }

    public function GetRemoteEntity(index : int) : CEntity
    {
        if (index >= 0 && index < remoteEntities.Size())
        {
            return remoteEntities[index];
        }
        return NULL;
    }

    public function GetAssignedHUDSlot(index : int) : int
    {
        if (index >= 0 && index < assignedHUDSlots.Size())
        {
            return assignedHUDSlots[index];
        }
        return -1;
    }

    public function AssignHUDSlot(index : int, slotIndex : int)
    {
        // Ensure the array is large enough
        while (assignedHUDSlots.Size() <= index)
        {
            assignedHUDSlots.PushBack(-1);
        }
        assignedHUDSlots[index] = slotIndex;
    }
}

// ===========================================================================
// STATE MACHINE - Main multiplayer loop
// ===========================================================================

statemachine class W3mStateMachine extends CEntity
{
    public var playerManager : RemotePlayerManager;

    public function Start()
    {
        this.GotoState('MultiplayerState');
    }

    public function Stop()
    {
        if (playerManager)
        {
            playerManager.Shutdown();
            delete playerManager;
            playerManager = NULL;
        }

        this.GotoState('Idle');
    }
}

// ===========================================================================
// IDLE STATE
// ===========================================================================

state Idle in W3mStateMachine
{
    event OnEnterState(previousStateName : name)
    {
        // Idle - do nothing
    }
}

// ===========================================================================
// MULTIPLAYER STATE - 33.3Hz tick loop
// ===========================================================================

state MultiplayerState in W3mStateMachine
{
    private var debugFrameCounter : int;

    event OnEnterState(previousStateName : name)
    {
        debugFrameCounter = 0;
        parent.playerManager = new RemotePlayerManager in parent;
        parent.playerManager.Initialize();

        this.EntryFunction();
    }

    event OnLeaveState(nextStateName : name)
    {
        if (parent.playerManager)
        {
            parent.playerManager.Shutdown();
            delete parent.playerManager;
            parent.playerManager = NULL;
        }
    }

    entry function EntryFunction()
    {
        this.RunMultiplayer();
    }

    latent function RunMultiplayer()
    {
        while (true)
        {
            RunMultiplayerFrame();
            Sleep(0.03); // 33.3Hz tick rate
        }
    }

    latent function RunMultiplayerFrame()
    {
        var playerStates : array<W3mPlayer>;
        var remoteCount : int;
        var i : int;
        var entity : CEntity;
        var actor : CActor;
        var player : W3mPlayer;
        var assignedSlot : int;
        var availableSlot : int;
        var slotsUsed : array<bool>;

        // Transmit local player state to server
        TransmitCurrentPlayerState();

        // Fetch remote player states from C++
        playerStates = W3mGetPlayerStates();

        // Update remote entities
        if (parent.playerManager)
        {
            parent.playerManager.UpdateRemotePlayers(playerStates);

            // Initialize slot tracking (2 slots max)
            slotsUsed.PushBack(false);
            slotsUsed.PushBack(false);

            // Smart HUD slot assignment - respects story companion NPCs
            remoteCount = Min(parent.playerManager.GetRemoteEntityCount(), playerStates.Size());
            for (i = 0; i < remoteCount; i += 1)
            {
                entity = parent.playerManager.GetRemoteEntity(i);
                if (entity && i < playerStates.Size())
                {
                    actor = (CActor)entity;
                    player = playerStates[i];

                    if (actor)
                    {
                        // Check if this player already has a slot assigned
                        assignedSlot = parent.playerManager.GetAssignedHUDSlot(i);

                        // If no slot assigned yet, find the next available one
                        if (assignedSlot == -1)
                        {
                            availableSlot = GetNextAvailableHUDSlot();

                            // Check if the slot is already used by another remote player this frame
                            if (availableSlot >= 0 && availableSlot < slotsUsed.Size() && !slotsUsed[availableSlot])
                            {
                                assignedSlot = availableSlot;
                                parent.playerManager.AssignHUDSlot(i, assignedSlot);
                                slotsUsed[availableSlot] = true;
                            }
                        }
                        else if (assignedSlot >= 0 && assignedSlot < slotsUsed.Size())
                        {
                            slotsUsed[assignedSlot] = true;
                        }

                        // Sync HUD or floating UI based on slot assignment
                        if (assignedSlot >= 0)
                        {
                            SyncRemotePlayerHUD(assignedSlot, actor, player.playerName);
                        }
                        else
                        {
                            // No HUD slot available - use floating health bar
                            UpdateFloatingUI(actor, player.playerName);
                        }
                    }
                }
            }

            // Debug HUD: Print remote player count every 100 frames
            debugFrameCounter += 1;
            if (debugFrameCounter >= 100)
            {
                debugFrameCounter = 0;
                DisplayFeedMessage("Active Remotes: " + IntToString(remoteCount));
            }
        }
    }
}

// ===========================================================================
// UTILITY FUNCTIONS
// ===========================================================================

function GetNextAvailableHUDSlot() : int
{
    var hud : CR4ScriptedHud;
    var module : CR4HudModuleCompanion;
    var slot0NPC : CNewNPC;
    var slot1NPC : CNewNPC;
    var witcher : W3PlayerWitcher;

    hud = (CR4ScriptedHud)theGame.GetHud();
    if (!hud)
    {
        return -1;
    }

    module = (CR4HudModuleCompanion)hud.GetHudModule("CompanionModule");
    if (!module)
    {
        return -1;
    }

    witcher = GetWitcherPlayer();
    if (!witcher)
    {
        return -1;
    }

    // Check slot 0: Get the NPC tag from the companion module
    slot0NPC = theGame.GetNPCByTag(witcher.GetCompanionNPCTag());

    // Check slot 1: Get the second companion NPC tag
    slot1NPC = theGame.GetNPCByTag(witcher.GetCompanionNPCTag2());

    // Slot 0 is free if no NPC exists OR if it's a multiplayer entity
    if (!slot0NPC || slot0NPC.HasTag('w3m_RemotePlayer'))
    {
        return 0;
    }

    // Slot 1 is free if no NPC exists OR if it's a multiplayer entity
    if (!slot1NPC || slot1NPC.HasTag('w3m_RemotePlayer'))
    {
        return 1;
    }

    // Both slots occupied by story NPCs - no HUD available
    return -1;
}

function UpdateFloatingUI(playerActor : CActor, playerName : string)
{
    var visualDebug : CVisualDebug;
    var actorPos : Vector;
    var healthPercent : float;
    var currentHealth : float;
    var maxHealth : float;
    var healthBarText : string;
    var nametagID : name;
    var healthBarID : name;
    var barColor : Color;

    if (!playerActor)
    {
        return;
    }

    visualDebug = thePlayer.GetVisualDebug();
    if (!visualDebug)
    {
        return;
    }

    // Get actor's world position and offset upward for floating text
    actorPos = playerActor.GetWorldPosition();

    // Get health stats
    playerActor.GetStats(BCS_Vitality, currentHealth, maxHealth);
    if (maxHealth > 0.0)
    {
        healthPercent = ClampF(currentHealth / maxHealth, 0.0, 1.0);
    }
    else
    {
        healthPercent = 1.0;
    }

    // Generate unique IDs for this player's UI elements
    nametagID = StringToName("w3mp_name_" + playerName);
    healthBarID = StringToName("w3mp_health_" + playerName);

    // Render player name above head (2.3m offset)
    visualDebug.AddText(
        nametagID,
        playerName,
        actorPos + Vector(0.0, 0.0, 2.3),
        true,  // absolutePos
        ,      // line (default)
        Color(255, 255, 255),  // White text
        true,  // background
        0.05   // timeout (refresh every frame at 33ms)
    );

    // Render health bar using ASCII representation (2.1m offset)
    healthBarText = BuildHealthBarText(healthPercent);

    // Color based on health percentage
    if (healthPercent > 0.5)
    {
        barColor = Color(0, 255, 0);  // Green
    }
    else if (healthPercent > 0.25)
    {
        barColor = Color(255, 255, 0);  // Yellow
    }
    else
    {
        barColor = Color(255, 0, 0);  // Red
    }

    visualDebug.AddText(
        healthBarID,
        healthBarText,
        actorPos + Vector(0.0, 0.0, 2.1),
        true,  // absolutePos
        ,      // line (default)
        barColor,
        true,  // background
        0.05   // timeout (refresh every frame at 33ms)
    );
}

function BuildHealthBarText(healthPercent : float) : string
{
    var barLength : int;
    var filledLength : int;
    var i : int;
    var result : string;

    barLength = 20;  // Total bar width in characters
    filledLength = RoundF(healthPercent * barLength);

    result = "[";
    for (i = 0; i < barLength; i += 1)
    {
        if (i < filledLength)
        {
            result += "|";
        }
        else
        {
            result += " ";
        }
    }
    result += "] " + RoundF(healthPercent * 100.0) + "%";

    return result;
}

function SyncRemotePlayerHUD(slotIndex : int, playerActor : CActor, playerName : string)
{
    var hud : CR4ScriptedHud;
    var module : CR4HudModuleCompanion;
    var npc : CNewNPC;
    var playerTag : name;

    // Enforce 2-player HUD limit (slots 0 and 1 only)
    if (slotIndex < 0 || slotIndex > 1)
    {
        return;
    }

    if (!playerActor)
    {
        return;
    }

    hud = (CR4ScriptedHud)theGame.GetHud();
    if (!hud)
    {
        return;
    }

    module = (CR4HudModuleCompanion)hud.GetHudModule("CompanionModule");
    if (!module)
    {
        return;
    }

    npc = (CNewNPC)playerActor;
    if (!npc)
    {
        return;
    }

    // Set display name on the actor so the HUD can retrieve it
    W3mSetNpcDisplayName(npc, playerName);

    // Get the actor's tag for the companion module
    playerTag = npc.GetTag();

    // Slot 0: Primary companion (left HUD slot)
    if (slotIndex == 0)
    {
        module.ShowCompanion(true, playerTag, "");
        module.UpdateVitality();
    }
    // Slot 1: Secondary companion (right HUD slot)
    else if (slotIndex == 1)
    {
        module.ShowCompanionSecond(playerTag, "");
        module.UpdateVitality2();
    }
}

function CleanupFloatingUI(playerName : string)
{
    var visualDebug : CVisualDebug;
    var nametagID : name;
    var healthBarID : name;

    visualDebug = thePlayer.GetVisualDebug();
    if (!visualDebug)
    {
        return;
    }

    // Remove floating UI elements for this player
    nametagID = StringToName("w3mp_name_" + playerName);
    healthBarID = StringToName("w3mp_health_" + playerName);

    visualDebug.RemoveText(nametagID);
    visualDebug.RemoveText(healthBarID);
}

function CleanupHUD()
{
    var hud : CR4ScriptedHud;
    var module : CR4HudModuleCompanion;
    var slot0NPC : CNewNPC;
    var slot1NPC : CNewNPC;
    var witcher : W3PlayerWitcher;

    hud = (CR4ScriptedHud)theGame.GetHud();
    if (!hud)
    {
        return;
    }

    module = (CR4HudModuleCompanion)hud.GetHudModule("CompanionModule");
    if (!module)
    {
        return;
    }

    witcher = GetWitcherPlayer();
    if (!witcher)
    {
        return;
    }

    // Only hide slots occupied by multiplayer players, leave story NPCs untouched
    slot0NPC = theGame.GetNPCByTag(witcher.GetCompanionNPCTag());
    if (slot0NPC && slot0NPC.HasTag('w3m_RemotePlayer'))
    {
        module.ShowCompanion(false, '', "");
    }

    slot1NPC = theGame.GetNPCByTag(witcher.GetCompanionNPCTag2());
    if (slot1NPC && slot1NPC.HasTag('w3m_RemotePlayer'))
    {
        module.ShowCompanionSecond('', "");
    }
}

function UpdateMultiplayerHUD(entity : CEntity, healthPercent : float, playerName : string)
{
    // Deprecated - kept for compatibility
    // Use SyncRemotePlayerHUD instead
}

function DisplayFeedMessage(msg : string)
{
    var hud : CR4ScriptedHud;

    hud = (CR4ScriptedHud)theGame.GetHud();
    if (hud)
    {
        hud.HudConsoleMsg(msg);
    }
}

function DisplayCenterMessage(msg : string)
{
    if (GetWitcherPlayer())
    {
        GetWitcherPlayer().DisplayHudMessage(msg);
    }
}

// ===========================================================================
// TRUE CO-OP SYSTEMS - Quest/Combat/Cutscene Sync
// ===========================================================================

// QUEST SYNC: Broadcast quest facts to all players
@wrapMethod(CR4Game)
function AddFactWrapper(factId : string, value : int, validFor : int)
{
    // Call original FactsAdd
    FactsAdd(factId, value, validFor);

    // Broadcast to multiplayer session
    W3mBroadcastFact(factId, value);

    // Debug: Show fact sync in HUD
    if (theGame.w3mStateMachine)
    {
        DisplayFeedMessage("[QUEST SYNC] Fact: " + factId + " = " + IntToString(value));
    }
}

// COMBAT SYNC: Intercept player attacks and broadcast to network
@addMethod(W3PlayerWitcher)
function W3mOnPerformAttack(attackType : name, targetActor : CActor) : bool
{
    var damage : float;
    var targetTag : name;
    var attackTypeInt : int;

    if (!targetActor)
    {
        return false;
    }

    // Get target's tag for network identification
    targetTag = targetActor.GetTag();

    // Map attack type to integer
    if (attackType == theGame.params.ATTACK_NAME_LIGHT)
    {
        attackTypeInt = 0; // Light attack
        damage = 50.0; // Placeholder damage
    }
    else if (attackType == theGame.params.ATTACK_NAME_HEAVY)
    {
        attackTypeInt = 1; // Heavy attack
        damage = 100.0; // Placeholder damage
    }
    else
    {
        attackTypeInt = 2; // Special attack
        damage = 75.0;
    }

    // Broadcast attack to network
    // Note: Using 0 as placeholder for local player GUID (will be handled by C++ layer)
    W3mBroadcastAttack(0, targetTag, damage, attackTypeInt);

    // Debug visualization
    DisplayFeedMessage("[COMBAT SYNC] Attack on " + NameToString(targetTag) + ": " + damage + " dmg");

    return true;
}

// COMBAT SYNC: Apply incoming network attacks to local NPCs
function W3mApplyNetworkAttack(attackerName : string, targetTag : name, damageAmount : float, attackType : int)
{
    var targetNPC : CNewNPC;
    var damageAction : W3DamageAction;
    var attackTypeName : name;

    // Find target NPC in local world
    targetNPC = theGame.GetNPCByTag(targetTag);
    if (!targetNPC)
    {
        W3mPrint("[W3MP] Cannot find NPC with tag: " + NameToString(targetTag));
        return;
    }

    // Convert attack type integer to name
    switch (attackType)
    {
        case 0: attackTypeName = theGame.params.ATTACK_NAME_LIGHT; break;
        case 1: attackTypeName = theGame.params.ATTACK_NAME_HEAVY; break;
        default: attackTypeName = 'attack_special'; break;
    }

    // Create damage action
    damageAction = new W3DamageAction in theGame.damageMgr;
    damageAction.Initialize(thePlayer, targetNPC, NULL, attackerName, EHRT_Heavy, CPS_Undefined, false, false, false, false);
    damageAction.AddDamage(theGame.params.DAMAGE_NAME_PHYSICAL, damageAmount);
    damageAction.SetHitAnimationPlayType(EAHA_ForceYes);
    damageAction.SetProcessBuffsIfNoDamage(true);

    // Apply damage
    theGame.damageMgr.ProcessAction(damageAction);

    // Cleanup
    delete damageAction;

    W3mPrint("[W3MP] Applied " + damageAmount + " damage to " + NameToString(targetTag));
}

// CUTSCENE SYNC: Trigger cutscene for all players
function W3mTriggerCutscene(cutscenePath : string, csPos : Vector, csRot : EulerAngles)
{
    var actorNames : array<string>;
    var actorEntities : array<CEntity>;

    // Add Geralt to cutscene
    actorNames.PushBack("geralt");
    actorEntities.PushBack(thePlayer);

    // Broadcast cutscene start to network
    W3mBroadcastCutscene(cutscenePath, csPos, csRot);

    // Play cutscene locally
    theGame.PlayCutsceneAsync(cutscenePath, actorNames, actorEntities, csPos, csRot);

    DisplayFeedMessage("[CUTSCENE SYNC] Playing: " + cutscenePath);
}

// CUTSCENE SYNC: Receive and play cutscene from network
function W3mReceiveCutscene(cutscenePath : string, csPos : Vector, csRot : EulerAngles)
{
    var actorNames : array<string>;
    var actorEntities : array<CEntity>;

    // Add Geralt to cutscene
    actorNames.PushBack("geralt");
    actorEntities.PushBack(thePlayer);

    // Play cutscene
    theGame.PlayCutsceneAsync(cutscenePath, actorNames, actorEntities, csPos, csRot);

    W3mPrint("[W3MP] Received cutscene: " + cutscenePath);
}

// ===========================================================================
// CONSOLE COMMANDS
// ===========================================================================

exec function SetName(playerName : string)
{
    if (playerName == "")
    {
        DisplayFeedMessage("Error: Name cannot be empty");
        return;
    }

    W3mUpdatePlayerName(playerName);
    DisplayFeedMessage("Name changed: " + playerName);
}

exec function W3mStop()
{
    if (theGame.w3mStateMachine)
    {
        theGame.w3mStateMachine.Stop();
        DisplayFeedMessage("Multiplayer stopped");
    }
}

exec function W3mRestart()
{
    if (theGame.w3mStateMachine)
    {
        theGame.w3mStateMachine.Stop();
        theGame.w3mStateMachine.Start();
        DisplayFeedMessage("Multiplayer restarted");
    }
}

// ===========================================================================
// TRUE CO-OP CONSOLE COMMANDS - For Testing
// ===========================================================================

exec function W3mTestQuest()
{
    AddFactWrapper("test_quest_fact", 1, -1);
}

exec function W3mTestCombat()
{
    var target : CActor;
    var targets : array<CActor>;

    // Find nearest NPC
    FindGameplayEntitiesInRange(targets, thePlayer, 10.0, 1, '', FLAG_OnlyAliveActors);

    if (targets.Size() > 0)
    {
        target = targets[0];
        thePlayer.W3mOnPerformAttack(theGame.params.ATTACK_NAME_LIGHT, target);
    }
    else
    {
        DisplayFeedMessage("No targets nearby");
    }
}

exec function W3mTestCutscene(optional cutscenePath : string)
{
    var pos : Vector;
    var rot : EulerAngles;

    if (cutscenePath == "")
    {
        cutscenePath = "movies\\cutscenes\\cs000_01.w2scene"; // Example path
    }

    pos = thePlayer.GetWorldPosition();
    rot = thePlayer.GetWorldRotation();

    W3mTriggerCutscene(cutscenePath, pos, rot);
}

exec function W3mTestLoopback(enable : bool)
{
    // Call C++ to set loopback mode
    W3mSetLoopback(enable);
    
    if (enable)
    {
        DisplayFeedMessage("Loopback mode ENABLED - All packets route back to local client");
    }
    else
    {
        DisplayFeedMessage("Loopback mode DISABLED - Normal network operation");
    }
}

exec function W3mTestAnimation()
{
    var explorationAction : int;
    
    // Test meditation animation (PEA_Meditation = 2)
    explorationAction = 2;
    W3mBroadcastAnimation("meditation", explorationAction);
    DisplayFeedMessage("Broadcasting meditation animation");
}

exec function W3mTestNPCDeath()
{
    var target : CActor;
    var targets : array<CActor>;
    var targetTag : name;
    
    // Find nearest NPC
    FindGameplayEntitiesInRange(targets, thePlayer, 10.0, 1, '', FLAG_OnlyAliveActors);
    
    if (targets.Size() > 0)
    {
        target = targets[0];
        targetTag = target.GetTag();
        
        // Kill locally and broadcast
        target.Kill('W3MP_Test', false, thePlayer);
        W3mBroadcastNPCDeath(targetTag);
        
        DisplayFeedMessage("Force killed NPC: " + NameToString(targetTag));
    }
    else
    {
        DisplayFeedMessage("No NPCs nearby");
    }
}

exec function W3mTestTime(hours : int)
{
    var newTime : GameTime;
    var currentTime : GameTime;
    
    currentTime = theGame.GetGameTime();
    newTime = GameTimeCreate(GameTimeDays(currentTime), hours, 0, 0);
    
    theGame.SetGameTime(newTime, false);
    DisplayFeedMessage("Time set to " + hours + ":00 - Testing loopback reconciliation");
}

exec function W3mTestWeather(weatherID : int)
{
    var weather : EWeatherEffect;
    
    weather = (EWeatherEffect)weatherID;
    RequestWeatherChange(weather);
    
    DisplayFeedMessage("Weather changed to ID " + weatherID + " - Testing loopback reconciliation");
}

exec function W3mTestAchievement(achievementID : string)
{
    // Broadcast achievement unlock (will trigger local unlock via loopback if enabled)
    W3mBroadcastAchievement(achievementID);
    
    DisplayFeedMessage("Broadcasting achievement: " + achievementID);
}

exec function W3mUnlockLocal(achievementID : string)
{
    var achievementName : name;
    
    // Direct local unlock without broadcasting (for testing)
    achievementName = StringToName(achievementID);
    theGame.UnlockAchievement(achievementName);
    
    DisplayFeedMessage("Locally unlocked: " + achievementID);
}

exec function W3mTestLoot(itemName : string, quantity : int)
{
    // Broadcast loot as if it's a relic (for testing)
    W3mBroadcastLoot(itemName, quantity, true);
    
    DisplayFeedMessage("Broadcasting loot: " + itemName + " x" + quantity);
}

exec function W3mTestGold(amount : int)
{
    // Broadcast Crowns (instant gold sync)
    W3mBroadcastLoot("Crowns", amount, false);
    
    DisplayFeedMessage("Broadcasting instant gold: " + amount + " crowns");
}

exec function W3mTestScaling()
{
    var nearestNPC : CNewNPC;
    var npcs : array<CNewNPC>;
    var partyCount : int;
    
    // Find nearest NPC
    FindGameplayEntitiesInRange(npcs, thePlayer, 20.0, 1, '', FLAG_OnlyAliveActors);
    
    if (npcs.Size() > 0)
    {
        nearestNPC = npcs[0];
        partyCount = 3; // Simulate 3-player party
        
        // Apply scaling via C++ bridge
        W3mApplyPartyScaling(nearestNPC, partyCount);
        
        DisplayFeedMessage("Applied party scaling to nearest NPC (simulating " + partyCount + " players)");
    }
    else
    {
        DisplayFeedMessage("No NPCs nearby to scale");
    }
}

exec function W3mTestQuestLock(lock : bool)
{
    var sceneID : int;
    
    sceneID = 12345; // Test scene ID
    
    W3mBroadcastQuestLock(lock, sceneID);
    
    if (lock)
    {
        DisplayFeedMessage("Broadcasting quest LOCK (testing spectator mode + teleport + ghosting)");
    }
    else
    {
        DisplayFeedMessage("Broadcasting quest UNLOCK (restoring movement)");
    }
}

exec function W3mShareSession()
{
    // Copy session IP to clipboard for easy sharing
    W3mCopyIP();
    
    DisplayFeedMessage("[SESSION] IP copied to clipboard - Share with friends to join!");
}

// Party Scaling: NPC Health & Damage Multiplier (called from C++)
function W3mScaleNPCHealth(npc : CNewNPC, healthMultiplier : float, partyCount : int)
{
    var currentMaxHealth : float;
    var newMaxHealth : float;
    var isBoss : bool;
    var damageBonus : float;
    
    if (!npc)
    {
        return;
    }
    
    // Get current max health
    currentMaxHealth = npc.GetMaxHealth();
    
    // Calculate new max health
    newMaxHealth = currentMaxHealth * healthMultiplier;
    
    // Apply health scaling via ability
    // Note: Direct SetMaxHealth doesn't exist, so we add abilities with vitality bonuses
    // For each 0.5x multiplier, add a scaling ability
    if (healthMultiplier >= 1.5)
    {
        npc.AddAbility('W3MP_PartyScaling_Health', true);
    }
    if (healthMultiplier >= 2.0)
    {
        npc.AddAbility('W3MP_PartyScaling_Health', true);
    }
    if (healthMultiplier >= 2.5)
    {
        npc.AddAbility('W3MP_PartyScaling_Health', true);
    }
    
    // Restore health to full after scaling
    npc.SetHealthPerc(100.0);
    
    // Boss exception: increase damage by 20% per additional player
    isBoss = npc.HasTag('Boss') || npc.HasTag('boss');
    if (isBoss && partyCount > 1)
    {
        damageBonus = (partyCount - 1) * 0.2; // 20% per player
        
        // Add damage scaling abilities
        if (damageBonus >= 0.2)
        {
            npc.AddAbility('W3MP_BossScaling_Damage', true);
        }
        if (damageBonus >= 0.4)
        {
            npc.AddAbility('W3MP_BossScaling_Damage', true);
        }
        if (damageBonus >= 0.6)
        {
            npc.AddAbility('W3MP_BossScaling_Damage', true);
        }
        
        W3mPrint("[W3MP] Boss scaled: " + healthMultiplier + "x health, +" + (damageBonus * 100) + "% damage");
    }
    else
    {
        W3mPrint("[W3MP] NPC scaled: " + healthMultiplier + "x health");
    }
}

// Quest Lock: Freeze/Unfreeze Player with Teleportation & Ghosting (called from C++)
function W3mApplyQuestLock(isLocked : bool, sceneID : int, playerName : string, initiatorPosition : Vector, foundInitiator : bool, playerGuid : Uint64)
{
    var localPosition : Vector;
    var distance : float;
    var teleportOffset : Vector;
    
    if (!thePlayer)
    {
        return;
    }
    
    if (isLocked)
    {
        // QUEST TELEPORT: Check distance to initiator
        if (foundInitiator)
        {
            localPosition = thePlayer.GetWorldPosition();
            distance = VecDistance(localPosition, initiatorPosition);
            
            // If distance > 30m, teleport to initiator with offset
            if (distance > 30.0)
            {
                teleportOffset = initiatorPosition;
                teleportOffset.X += 2.0; // 2m offset to avoid overlap
                teleportOffset.Y += 2.0;
                
                thePlayer.Teleport(teleportOffset);
                
                DisplayFeedMessage("[QUEST TELEPORT] Teleported to " + playerName + "'s location (" + RoundMath(distance) + "m away)");
                W3mPrint("[W3MP] Teleported to quest initiator: " + distance + "m");
            }
        }
        
        // Freeze player movement
        thePlayer.BlockAction(EIAB_Movement, 'W3MP_QuestLock');
        thePlayer.BlockAction(EIAB_Sprint, 'W3MP_QuestLock');
        thePlayer.BlockAction(EIAB_Dodge, 'W3MP_QuestLock');
        thePlayer.BlockAction(EIAB_Roll, 'W3MP_QuestLock');
        thePlayer.BlockAction(EIAB_Jump, 'W3MP_QuestLock');
        
        // PLAYER GHOSTING: Set transparency to 50% (dithering effect)
        thePlayer.SetAppearance('dithered_50');
        
        // Hide HUD elements
        theGame.GetGuiManager().SetBackgroundTexture('');
        
        DisplayFeedMessage("[QUEST SPECTATOR] " + playerName + " started a scene - You are now in spectator mode");
        W3mPrint("[W3MP] Quest locked by " + playerName + " (scene " + sceneID + ")");
    }
    else
    {
        // Unfreeze player movement
        thePlayer.UnblockAction(EIAB_Movement, 'W3MP_QuestLock');
        thePlayer.UnblockAction(EIAB_Sprint, 'W3MP_QuestLock');
        thePlayer.UnblockAction(EIAB_Dodge, 'W3MP_QuestLock');
        thePlayer.UnblockAction(EIAB_Roll, 'W3MP_QuestLock');
        thePlayer.UnblockAction(EIAB_Jump, 'W3MP_QuestLock');
        
        // Restore normal appearance (remove ghosting)
        thePlayer.SetAppearance('default');
        
        DisplayFeedMessage("[QUEST SPECTATOR] Scene ended - Movement restored");
        W3mPrint("[W3MP] Quest unlocked (scene " + sceneID + ")");
    }
}

// Hook into player loot events to auto-broadcast relic/boss loot
function W3mOnItemLooted(itemName : name, quantity : int, containerTag : name)
{
    var itemQuality : int;
    var isBossContainer : bool;
    var isRelicOrBoss : bool;
    var itemNameStr : string;
    var containerTagStr : string;
    
    if (!thePlayer || !thePlayer.inv)
    {
        return;
    }
    
    itemNameStr = NameToString(itemName);
    containerTagStr = NameToString(containerTag);
    
    // Check if item is Crowns (instant gold)
    if (itemNameStr == "Crowns")
    {
        W3mBroadcastLoot(itemNameStr, quantity, false);
        return;
    }
    
    // Get item quality (4 = Relic)
    itemQuality = thePlayer.inv.GetItemQualityFromName(itemName);
    
    // Check if container has Boss tag
    isBossContainer = (StrContains(containerTagStr, "boss") || StrContains(containerTagStr, "Boss"));
    
    // Broadcast if Relic quality or Boss container
    isRelicOrBoss = (itemQuality >= 4 || isBossContainer);
    
    if (isRelicOrBoss)
    {
        W3mBroadcastLoot(itemNameStr, quantity, true);
        W3mPrint("[W3MP] Broadcasting relic/boss loot: " + itemNameStr + " (quality=" + itemQuality + ")");
    }
}
