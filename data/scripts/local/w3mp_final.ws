// ===========================================================================
// WITCHERSEAMLESS MULTIPLAYER - FINAL POLISH & HARDENING
// ===========================================================================
// Production-Grade Architecture with Safety Pillars
// Atomic Facts, Handshake Protocol, Time Smoothing
// ===========================================================================

// ---------------------------------------------------------------------------
// SESSION STATE MACHINE
// ---------------------------------------------------------------------------

enum W3mSessionState
{
	W3mState_FreeRoam,		// Normal gameplay - full player control
	W3mState_Spectator,		// Watching quest/dialogue - ghosted & frozen
	W3mState_Dialogue		// Active participant in dialogue
}

// Global session state - Extend CR4Player
@addField(CR4Player)
var w3mSessionState : W3mSessionState;

// ---------------------------------------------------------------------------
// ATOMIC FACTS SYSTEM - LOCK-CHECK & QUEUEING
// ---------------------------------------------------------------------------

struct W3mPendingFact
{
	var factName : string;
	var value : int;
	var validFor : int;
	var timestamp : float;
}

@addField(CR4Player)
var w3mGlobalSyncInProgress : bool;

@addField(CR4Player)
var w3mPendingFacts : array<W3mPendingFact>;

// Native fact wrapper (renamed from FactsAdd)
function W3mNativeFactsAdd(factName : string, value : int, validFor : int)
{
	FactsAdd(factName, value, validFor);
}

function W3mNativeFactsRemove(factName : string)
{
	FactsRemove(factName);
}

// Atomic fact wrapper with lock-check
function W3mAtomicAddFact(factName : string, value : int, validFor : int)
{
	var pendingFact : W3mPendingFact;
	
	if (thePlayer && thePlayer.w3mGlobalSyncInProgress)
	{
		// Queue fact change instead of applying instantly
		pendingFact.factName = factName;
		pendingFact.value = value;
		pendingFact.validFor = validFor;
		pendingFact.timestamp = theGame.GetEngineTime();
		
		thePlayer.w3mPendingFacts.PushBack(pendingFact);
		
		W3mPrint("[W3MP ATOMIC] Fact queued during sync: " + factName + " = " + value);
		return;
	}
	
	// Apply fact instantly if no global sync in progress
	W3mNativeFactsAdd(factName, value, validFor);
	W3mPrint("[W3MP ATOMIC] Fact applied: " + factName + " = " + value);
}

function W3mAtomicRemoveFact(factName : string)
{
	var pendingFact : W3mPendingFact;
	
	if (thePlayer && thePlayer.w3mGlobalSyncInProgress)
	{
		// Queue fact removal instead of applying instantly
		pendingFact.factName = factName;
		pendingFact.value = 0;
		pendingFact.validFor = 0;
		pendingFact.timestamp = theGame.GetEngineTime();
		
		thePlayer.w3mPendingFacts.PushBack(pendingFact);
		
		W3mPrint("[W3MP ATOMIC] Fact removal queued during sync: " + factName);
		return;
	}
	
	// Apply fact removal instantly if no global sync in progress
	W3mNativeFactsRemove(factName);
	W3mPrint("[W3MP ATOMIC] Fact removed: " + factName);
}

// Flush pending facts after global sync completes
function W3mFlushPendingFacts()
{
	var i : int;
	var pendingFact : W3mPendingFact;
	
	W3mPrint("[W3MP ATOMIC] Flushing " + thePlayer.w3mPendingFacts.Size() + " pending facts");

	for (i = 0; i < thePlayer.w3mPendingFacts.Size(); i += 1)
	{
		pendingFact = thePlayer.w3mPendingFacts[i];
		
		if (pendingFact.validFor > 0)
		{
			W3mNativeFactsAdd(pendingFact.factName, pendingFact.value, pendingFact.validFor);
		}
		else
		{
			W3mNativeFactsRemove(pendingFact.factName);
		}
	}
	
	thePlayer.w3mPendingFacts.Clear();
	thePlayer.w3mGlobalSyncInProgress = false;
	
	W3mPrint("[W3MP ATOMIC] Global sync completed, pending facts applied");
}

// ---------------------------------------------------------------------------
// TIME SMOOTHING SYSTEM
// ---------------------------------------------------------------------------

@addField(CR4Player)
var w3mTimeSmoothingActive : bool;

@addField(CR4Player)
var w3mOriginalTimeMultiplier : float;

@addField(CR4Player)
var w3mSmoothingStartTime : float;

@addField(CR4Player)
var w3mTargetTimeOffset : float;

function W3mSmoothTimeReconciliation(hostGameTime : int)
{
	var currentGameTime : GameTime;
	var currentSeconds : int;
	var timeDiff : float;
	var smoothingDuration : float;
	var adjustmentRate : float;
	var newMultiplier : float;
	var newGameTime : GameTime;

	currentGameTime = theGame.GetGameTime();
	currentSeconds = GameTimeToSeconds(currentGameTime);
	timeDiff = (float)hostGameTime - (float)currentSeconds;

	// If drift is small (5-10 seconds), use smoothing instead of snapping
	if (AbsF(timeDiff) >= 5.0 && AbsF(timeDiff) <= 10.0)
	{
		if (!thePlayer.w3mTimeSmoothingActive)
		{
			// Start time smoothing
			thePlayer.w3mTimeSmoothingActive = true;
			thePlayer.w3mOriginalTimeMultiplier = theGame.GetTimeMultiplier();
			thePlayer.w3mSmoothingStartTime = theGame.GetEngineTime();
			thePlayer.w3mTargetTimeOffset = timeDiff;

			W3mPrint("[W3MP TIME] Starting smooth adjustment: " + timeDiff + "s drift");
		}

		// Calculate smoothing progress (over 3 seconds)
		smoothingDuration = 3.0;
		adjustmentRate = theGame.GetEngineTime() - thePlayer.w3mSmoothingStartTime;

		if (adjustmentRate < smoothingDuration)
		{
			// Gradually adjust time multiplier
			newMultiplier = thePlayer.w3mOriginalTimeMultiplier + (thePlayer.w3mTargetTimeOffset / smoothingDuration);
			theGame.SetTimeMultiplier(newMultiplier);

			W3mPrint("[W3MP TIME] Smoothing progress: " + (adjustmentRate / smoothingDuration * 100) + "%");
		}
		else
		{
			// Complete smoothing and restore normal multiplier
			theGame.SetTimeMultiplier(thePlayer.w3mOriginalTimeMultiplier);
			thePlayer.w3mTimeSmoothingActive = false;

			// Final time sync
			newGameTime = GameTimeCreateFromGameSeconds(hostGameTime);
			theGame.SetGameTime(newGameTime, false);

			W3mPrint("[W3MP TIME] Smooth adjustment completed, time synced");
		}
	}
	else if (AbsF(timeDiff) > 10.0)
	{
		// Large drift - snap immediately
		newGameTime = GameTimeCreateFromGameSeconds(hostGameTime);
		theGame.SetGameTime(newGameTime, false);

		W3mPrint("[W3MP TIME] Large drift detected, snapping time: " + timeDiff + "s");
	}
	else
	{
		// Small drift - ignore
		W3mPrint("[W3MP TIME] Time drift within tolerance: " + timeDiff + "s");
	}
}

// ---------------------------------------------------------------------------
// C++ BRIDGE IMPORTS (RTTI-Synchronized)
// ---------------------------------------------------------------------------

// Core multiplayer functions
import function W3mStorePlayerState(position : Vector, angles : EulerAngles, velocity : Vector, moveType : int, speed : float);
import function W3mGetPlayerStates() : array<W3mPlayer>;
import function W3mSetNpcDisplayName(npc : CNewNPC, displayName : string);
import function W3mUpdatePlayerName(playerName : string);
import function W3mGetMoveType(movingAgent : CMovingAgentComponent) : int;
import function W3mSetSpeed(movingAgent : CMovingAgentComponent, absSpeed: float);
import function W3mPrint(message : string);
import function W3mSetLoopback(enabled : bool);
import function W3mCopyIP();
import function W3mGetNetworkStats() : W3mNetworkStats;

// True Co-op Bridge Functions
import function W3mBroadcastFact(factName : string, value : int);
import function W3mBroadcastAttack(attackerGuid : Uint64, targetTag : name, damageAmount : float, attackType : int);
import function W3mBroadcastCutscene(cutscenePath : string, position : Vector, rotation : EulerAngles);
import function W3mBroadcastAnimation(animName : string, explorationAction : int);
import function W3mBroadcastVehicleMount(vehicleTemplate : string, isMounting : bool, position : Vector, rotation : EulerAngles);
import function W3mApplyPartyScaling(npc : CNewNPC, partyCount : int);
import function W3mBroadcastSessionState(newState : int, sceneID : int);
import function W3mInventoryBridge(itemName : string, quantity : int, isRelicOrBoss : bool);
import function W3mBroadcastAchievement(achievementID : string);
import function W3mBroadcastNPCDeath(targetTag : name);
import function W3mInitiateHandshake(sessionID : Uint64);

// ---------------------------------------------------------------------------
// SESSION STATE MANAGEMENT - CONSOLIDATED LOGIC
// ---------------------------------------------------------------------------

event OnSessionStateChanged(newState : W3mSessionState, sceneID : int, initiatorName : string, initiatorPosition : Vector, foundInitiator : bool, playerGuid : Uint64)
{
	var localPosition : Vector;
	var distance : float;
	var teleportOffset : Vector;
	
	if (!thePlayer)
	{
		return;
	}
	
	// Update session state
	thePlayer.w3mSessionState = newState;
	
	switch(newState)
	{
		case W3mState_Spectator:
			// CONSOLIDATED: Teleport + Ghosting + Input Blocking
			
			// 1. QUEST TELEPORT: Check distance to initiator
			if (foundInitiator)
			{
				localPosition = thePlayer.GetWorldPosition();
				distance = VecDistance(localPosition, initiatorPosition);
				
				// If distance > 30m, teleport to initiator with offset
				if (distance > 30.0)
				{
					teleportOffset = initiatorPosition;
					teleportOffset.X += 2.0;
					teleportOffset.Y += 2.0;
					
					thePlayer.Teleport(teleportOffset);
					
					W3mNativeUI_ShowMessage("[QUEST TELEPORT] Teleported to " + initiatorName + "'s location (" + RoundMath(distance) + "m away)", 0xFFFF00);
					W3mPrint("[W3MP] Teleported to quest initiator: " + distance + "m");
				}
			}
			
			// 2. PLAYER GHOSTING: Set transparency to 50%
			thePlayer.SetAppearance('dithered_50');
			
			// 3. INPUT BLOCKING: Freeze all movement
			thePlayer.BlockAction(EIAB_Movement, 'W3MP_Session');
			thePlayer.BlockAction(EIAB_Sprint, 'W3MP_Session');
			thePlayer.BlockAction(EIAB_Dodge, 'W3MP_Session');
			thePlayer.BlockAction(EIAB_Roll, 'W3MP_Session');
			thePlayer.BlockAction(EIAB_Jump, 'W3MP_Session');
			
			// 4. HUD MANAGEMENT: Hide elements
			theGame.GetGuiManager().SetBackgroundTexture('');
			
			W3mNativeUI_ShowMessage("[QUEST SPECTATOR] " + initiatorName + " started a scene - You are now in spectator mode", 0xFFFFFF);
			W3mPrint("[W3MP] Session state: SPECTATOR (scene " + sceneID + ")");
			break;
			
		case W3mState_FreeRoam:
			// CONSOLIDATED: Restore all normal gameplay
			
			// 1. Unfreeze player movement
			thePlayer.UnblockAction(EIAB_Movement, 'W3MP_Session');
			thePlayer.UnblockAction(EIAB_Sprint, 'W3MP_Session');
			thePlayer.UnblockAction(EIAB_Dodge, 'W3MP_Session');
			thePlayer.UnblockAction(EIAB_Roll, 'W3MP_Session');
			thePlayer.UnblockAction(EIAB_Jump, 'W3MP_Session');
			
			// 2. Restore normal appearance (remove ghosting)
			thePlayer.SetAppearance('default');
			
			// 3. Restore HUD
			theGame.GetGuiManager().SetBackgroundTexture('');
			
			W3mNativeUI_ShowMessage("[QUEST SPECTATOR] Scene ended - Movement restored", 0xFFFFFF);
			W3mPrint("[W3MP] Session state: FREE_ROAM");
			break;
			
		case W3mState_Dialogue:
			// Active participant - no restrictions
			W3mPrint("[W3MP] Session state: DIALOGUE (active participant)");
			break;
	}
}

// ---------------------------------------------------------------------------
// NATIVE UI WRAPPER - CDPR HEX CODES
// ---------------------------------------------------------------------------

function W3mNativeUI_ShowMessage(message : string, colorHex : int)
{
	// Display message with CDPR color coding
	// 0xFF0000 = Red (Health/Warnings)
	// 0xFFFFFF = White (Text)
	// 0xFFFF00 = Yellow (Alerts)
	
	DisplayFeedMessage(message);
}

function W3mNativeUI_DrawHealthBar(playerName : string, healthPercent : float)
{
	// Health bar rendering using CDPR red (0xFF0000)
	// Implemented in C++ renderer layer
}

// ---------------------------------------------------------------------------
// RECONCILIATION HEARTBEAT - ENHANCED WITH TIME SMOOTHING
// ---------------------------------------------------------------------------

function W3mGetTotalCrowns() : int
{
	var crowns : int;
	
	if (!thePlayer || !thePlayer.inv)
	{
		return 0;
	}
	
	crowns = thePlayer.inv.GetItemQuantityByName('Crowns');
	return crowns;
}

function W3mReconcileCrowns(remoteCrowns : int, remoteGuid : Uint64)
{
	var localCrowns : int;
	var diff : int;
	
	if (!thePlayer || !thePlayer.inv)
	{
		return;
	}
	
	localCrowns = thePlayer.inv.GetItemQuantityByName('Crowns');
	diff = remoteCrowns - localCrowns;
	
	if (diff != 0)
	{
		if (diff > 0)
		{
			thePlayer.inv.AddAnItem('Crowns', diff);
		}
		else
		{
			thePlayer.inv.RemoveItemByName('Crowns', AbsI(diff));
		}
		
		W3mPrint("[W3MP] Crowns reconciled: " + diff + " (now " + remoteCrowns + ")");
	}
}

function W3mGetGameTime() : int
{
	var gameTime : GameTime;
	var seconds : int;
	
	gameTime = theGame.GetGameTime();
	seconds = GameTimeToSeconds(gameTime);
	
	return seconds;
}

function W3mGetWeatherID() : int
{
	var weatherEffect : EWeatherEffect;
	
	weatherEffect = GetCurWeather();
	return (int)weatherEffect;
}

function W3mReconcileWorldState(hostGameTime : int, hostWeatherID : int)
{
	var currentWeather : EWeatherEffect;
	var hostWeather : EWeatherEffect;
	
	// Set global sync flag to queue fact changes
	if (thePlayer)
	{
		thePlayer.w3mGlobalSyncInProgress = true;
	}
	
	// Use time smoothing instead of snapping
	W3mSmoothTimeReconciliation(hostGameTime);
	
	// Weather reconciliation
	currentWeather = GetCurWeather();
	hostWeather = (EWeatherEffect)hostWeatherID;
	
	if (currentWeather != hostWeather)
	{
		RequestWeatherChange(hostWeather);
		W3mNativeUI_ShowMessage("[WORLD SYNC] Weather syncing to host", 0xFFFF00);
		W3mPrint("[W3MP] Weather reconciled: " + hostWeatherID);
	}
	
	// Flush any pending facts after world state sync
	W3mFlushPendingFacts();
}

// ---------------------------------------------------------------------------
// INVENTORY BRIDGE - CONSOLIDATED LOOT & GOLD
// ---------------------------------------------------------------------------

function W3mReceiveInventoryItem(itemName : string, quantity : int, playerName : string)
{
	var itemNameAsName : name;
	var addedItems : array<SItemUniqueId>;
	
	if (!thePlayer || !thePlayer.inv)
	{
		return;
	}
	
	itemNameAsName = StringToName(itemName);
	addedItems = thePlayer.inv.AddAnItem(itemNameAsName, quantity);
	
	if (addedItems.Size() > 0)
	{
		W3mNativeUI_ShowMessage("[W3MP] Received Shared Loot: " + itemName + " x" + quantity + " (from " + playerName + ")", 0xFFFFFF);
		W3mPrint("[W3MP] Inventory received: " + itemName + " x" + quantity);
	}
}

function W3mOnItemLooted(itemName : name, quantity : int, containerTag : name)
{
	var itemQuality : int;
	var isBossContainer : bool;
	var isRelicOrBoss : bool;
	var itemNameStr : string;
	var containerTagStr : string;
	var minQuality : int;
	var maxQuality : int;
	
	if (!thePlayer || !thePlayer.inv)
	{
		return;
	}
	
	itemNameStr = NameToString(itemName);
	containerTagStr = NameToString(containerTag);
	
	if (itemNameStr == "Crowns")
	{
		W3mInventoryBridge(itemNameStr, quantity, false);
		return;
	}
	
	thePlayer.inv.GetItemQualityFromName(itemName, minQuality, maxQuality);
	itemQuality = maxQuality;
	
	isBossContainer = (StrContains(containerTagStr, "boss") || StrContains(containerTagStr, "Boss"));
	isRelicOrBoss = (itemQuality >= 4 || isBossContainer);
	
	if (isRelicOrBoss)
	{
		W3mInventoryBridge(itemNameStr, quantity, true);
		W3mPrint("[W3MP] Broadcasting relic/boss loot: " + itemNameStr + " (quality=" + itemQuality + ")");
	}
}

// ---------------------------------------------------------------------------
// PARTY SCALING - NPC HEALTH & DAMAGE
// ---------------------------------------------------------------------------

function W3mScaleNPCHealth(npc : CNewNPC, healthMultiplier : float, partyCount : int)
{
	var currentMaxHealth : float;
	var isBoss : bool;
	var damageBonus : float;
	
	if (!npc)
	{
		return;
	}
	
	currentMaxHealth = npc.GetMaxHealth();
	
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
	
	npc.SetHealthPerc(100.0);
	
	isBoss = npc.HasTag('Boss') || npc.HasTag('boss');
	if (isBoss && partyCount > 1)
	{
		damageBonus = (partyCount - 1) * 0.2;
		
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

// ---------------------------------------------------------------------------
// ACHIEVEMENT SYNC
// ---------------------------------------------------------------------------

function W3mUnlockAchievement(achievementID : string)
{
	var achievementName : name;
	
	achievementName = StringToName(achievementID);
	theGame.UnlockAchievement(achievementName);
	
	W3mNativeUI_ShowMessage("[ACHIEVEMENT] Unlocked: " + achievementID, 0xFFFF00);
	W3mPrint("[W3MP] Achievement unlocked: " + achievementID);
}

// ---------------------------------------------------------------------------
// VEHICLE SYNC
// ---------------------------------------------------------------------------

function W3mApplyVehicleSync(playerName : string, vehicleTemplate : string, isMounting : bool, position : Vector, rotation : EulerAngles)
{
	var remoteActor : CActor;
	var remoteNPC : CNewNPC;
	var entities : array<CEntity>;
	var vehicleEntity : CEntity;
	var template : CEntityTemplate;
	var i : int;
	
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
		template = (CEntityTemplate)LoadResource(vehicleTemplate, true);
		if (template)
		{
			vehicleEntity = theGame.CreateEntity(template, position, rotation);
			if (vehicleEntity)
			{
				W3mPrint("[W3MP] Spawned vehicle for " + playerName + ": " + vehicleTemplate);
			}
		}
	}
	else
	{
		W3mPrint("[W3MP] " + playerName + " dismounted vehicle");
	}
}

// ---------------------------------------------------------------------------
// HANDSHAKE PROTOCOL
// ---------------------------------------------------------------------------

@addField(CR4Player)
var w3mSessionID : Uint64;

@addField(CR4Player)
var w3mHandshakeComplete : bool;

function W3mOnHandshakeComplete(sessionID : Uint64, playerName : string)
{
	if (!thePlayer)
	{
		return;
	}

	thePlayer.w3mSessionID = sessionID;
	thePlayer.w3mHandshakeComplete = true;
	
	W3mNativeUI_ShowMessage("[HANDSHAKE] Session established with " + playerName, 0x00FF00);
	W3mPrint("[W3MP] Handshake complete: SessionID=" + sessionID);
}

function W3mInitiateHandshake()
{
	var sessionID : Uint64;
	
	// Generate random 64-bit session ID
	sessionID = Rand(1000000, 9999999) * 1000000 + Rand(1000000, 9999999);
	
	W3mInitiateHandshake(sessionID);
	
	W3mPrint("[W3MP] Initiating handshake with SessionID: " + sessionID);
}

// ---------------------------------------------------------------------------
// CONSOLE COMMANDS - DEBUG & TESTING
// ---------------------------------------------------------------------------

exec function W3mTestLoopback(enable : bool)
{
	W3mSetLoopback(enable);
	
	if (enable)
	{
		W3mNativeUI_ShowMessage("Loopback mode ENABLED - All packets route back to local client", 0xFFFF00);
	}
	else
	{
		W3mNativeUI_ShowMessage("Loopback mode DISABLED - Normal network operation", 0xFFFFFF);
	}
}

exec function W3mTestSessionState(stateID : int)
{
	var sceneID : int;
	
	sceneID = 12345;
	W3mBroadcastSessionState(stateID, sceneID);
	
	W3mNativeUI_ShowMessage("Broadcasting session state: " + stateID, 0xFFFF00);
}

exec function W3mShareSession()
{
	W3mCopyIP();
	W3mNativeUI_ShowMessage("[SESSION] IP copied to clipboard - Share with friends to join!", 0xFFFFFF);
}

exec function W3mTestScaling()
{
	var nearestNPC : CNewNPC;
	var npcs : array<CNewNPC>;
	var partyCount : int;
	
	FindGameplayEntitiesInRange(npcs, thePlayer, 20.0, 1, '', FLAG_OnlyAliveActors);
	
	if (npcs.Size() > 0)
	{
		nearestNPC = npcs[0];
		partyCount = 3;
		
		W3mApplyPartyScaling(nearestNPC, partyCount);
		W3mNativeUI_ShowMessage("Applied party scaling to nearest NPC (simulating " + partyCount + " players)", 0xFFFF00);
	}
	else
	{
		W3mNativeUI_ShowMessage("No NPCs nearby to scale", 0xFF0000);
	}
}

exec function W3mTestInventory(itemName : string, quantity : int)
{
	W3mInventoryBridge(itemName, quantity, true);
	W3mNativeUI_ShowMessage("Broadcasting loot: " + itemName + " x" + quantity, 0xFFFF00);
}

exec function W3mTestAchievement(achievementID : string)
{
	W3mBroadcastAchievement(achievementID);
	W3mNativeUI_ShowMessage("Broadcasting achievement: " + achievementID, 0xFFFF00);
}

exec function W3mTestAtomicFact(factName : string, value : int)
{
	// Test atomic fact system
	W3mAtomicAddFact(factName, value, 0);
	W3mNativeUI_ShowMessage("Testing atomic fact: " + factName + " = " + value, 0xFFFF00);
}

exec function W3mTestHandshake()
{
	W3mInitiateHandshake();
	W3mNativeUI_ShowMessage("Initiating handshake protocol", 0xFFFF00);
}

exec function W3mTestTimeSmoothing()
{
	var hostTime : int;
	
	hostTime = W3mGetGameTime() + 7; // Simulate 7-second drift
	W3mSmoothTimeReconciliation(hostTime);
	
	W3mNativeUI_ShowMessage("Testing time smoothing with 7s drift", 0xFFFF00);
}
