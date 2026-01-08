// ===========================================================================
// WITCHERSEAMLESS MULTIPLAYER - COMBAT & WORLD SYNCHRONIZATION
// ===========================================================================
// WitcherScript integration for full world/combat synchronization
// Production-Ready Implementation
// ===========================================================================

// ---------------------------------------------------------------------------
// NATIVE BRIDGE FUNCTIONS - C++ INTEGRATION
// ---------------------------------------------------------------------------

// Combat Broadcasting
import function W3mBroadcastAttack(attackerGuid : int, targetTag : string, damageAmount : float, attackType : int);

// Party Scaling
import function W3mApplyPartyScaling(npc : CEntity, partyCount : int);
import function W3mSetPartyCount(partyCount : int);
import function W3mGetPartyCount() : int;
import function W3mCalculateHealthMultiplier(partyCount : int) : float;
import function W3mCalculateDamageMultiplier(partyCount : int, isBoss : bool) : float;
import function W3mApplyScalingToNPC(npc : CEntity, npcGuid : int, isBoss : bool);
import function W3mClearScalingCache();

// World State Synchronization (Heartbeat)
import function W3mUpdateCrowns(totalCrowns : int);
import function W3mUpdateGameTime(gameTimeSeconds : int);
import function W3mUpdateWeather(weatherId : int);

// ---------------------------------------------------------------------------
// COMBAT ATTACK INTERCEPTION
// ---------------------------------------------------------------------------
// Hook into W3DamageAction to broadcast attacks to all clients

@wrapMethod(W3DamageAction) function ProcessAction()
{
    var attacker : CActor;
    var target : CActor;
    var damageAmount : float;
    var attackType : int;

    // Call original ProcessAction
    wrappedMethod();

    // Extract damage data
    attacker = (CActor)this.attacker;
    target = (CActor)this.victim;

    if (!attacker || !target)
    {
        return;
    }

    // Only broadcast player attacks (not NPC-to-NPC)
    if (attacker == thePlayer)
    {
        damageAmount = this.processedDmg.vitalityDamage + this.processedDmg.essenceDamage;
        attackType = (int)this.GetBuffSourceName();

        // Broadcast attack to all clients
        W3mBroadcastAttack(
            (int)thePlayer.GetGUID(),
            target.GetReadableName(),
            damageAmount,
            attackType
        );
    }
}

// ---------------------------------------------------------------------------
// NPC SPAWN SCALING - PARTY SIZE MULTIPLIER
// ---------------------------------------------------------------------------
// Intercept NPC spawning to apply party-based health scaling

@addMethod(CNewNPC) function OnSpawned(spawnData : SEntitySpawnData)
{
    var partyCount : int;
    var isBoss : bool;
    var npcGuid : int;

    partyCount = W3mGetPartyCount();

    if (partyCount > 1)
    {
        // Determine if this is a boss NPC
        isBoss = this.HasAbility('Boss') || this.HasAbility('LargeBoss');

        // Get NPC GUID for cache tracking
        npcGuid = (int)this.GetGUID();

        // Apply scaling via C++ manager
        W3mApplyScalingToNPC((CEntity)this, npcGuid, isBoss);

        // Apply health multiplier
        if (partyCount > 1)
        {
            var healthMult : float;
            var maxHealth : float;
            var currentHealth : float;

            healthMult = W3mCalculateHealthMultiplier(partyCount);
            maxHealth = this.GetStatMax(BCS_Vitality);
            currentHealth = this.GetStat(BCS_Vitality);

            this.SetStatMax(BCS_Vitality, maxHealth * healthMult);
            this.SetStat(BCS_Vitality, currentHealth * healthMult);
        }
    }
}

// ---------------------------------------------------------------------------
// WORLD STATE HEARTBEAT - 1-SECOND UPDATE LOOP
// ---------------------------------------------------------------------------
// Continuously update C++ layer with current world state for heartbeat packets

@addMethod(CR4Player) function W3mWorldStateLoop(dt : float, id : int)
{
    var totalCrowns : int;
    var gameTime : int;
    var weatherId : int;

    // Update crown count for shared economy
    totalCrowns = (int)this.inv.GetMoney();
    W3mUpdateCrowns(totalCrowns);

    // Update game time (convert to seconds)
    gameTime = (int)theGame.GetGameTime().GetSeconds();
    W3mUpdateGameTime(gameTime);

    // Update weather ID
    weatherId = (int)theGame.GetCommonMapManager().GetCurrentWeatherID();
    W3mUpdateWeather(weatherId);
}

// ---------------------------------------------------------------------------
// PLAYER INITIALIZATION - START HEARTBEAT LOOP
// ---------------------------------------------------------------------------

@wrapMethod(CR4Player) function OnSpawned(spawnData : SEntitySpawnData)
{
    wrappedMethod(spawnData);

    // Start world state heartbeat (1-second updates to C++)
    this.AddTimer('W3mWorldStateLoop', 1.0, true);

    // Initialize party count (will be updated by host)
    W3mSetPartyCount(1);

    this.DisplayHudMessage("W3M Combat Sync Active");
}

// ---------------------------------------------------------------------------
// CROWN SYNCHRONIZATION - INSTANT GOLD SYSTEM
// ---------------------------------------------------------------------------
// Intercept money additions to broadcast to all players

@wrapMethod(CInventoryComponent) function AddMoney(amount : int)
{
    wrappedMethod(amount);

    // Update cached crown count immediately
    var totalCrowns : int;
    totalCrowns = (int)this.GetMoney();
    W3mUpdateCrowns(totalCrowns);
}

@wrapMethod(CInventoryComponent) function RemoveMoney(amount : int) : bool
{
    var result : bool;
    result = wrappedMethod(amount);

    // Update cached crown count immediately
    var totalCrowns : int;
    totalCrowns = (int)this.GetMoney();
    W3mUpdateCrowns(totalCrowns);

    return result;
}

// ---------------------------------------------------------------------------
// PARTY COUNT SYNCHRONIZATION
// ---------------------------------------------------------------------------
// Update party count when players join/leave

@addMethod(CR4Player) function W3mOnPlayerJoined()
{
    var currentPartyCount : int;
    currentPartyCount = W3mGetPartyCount();
    W3mSetPartyCount(currentPartyCount + 1);

    // Clear scaling cache to recalculate for new party size
    W3mClearScalingCache();

    this.DisplayHudMessage("Player joined - Party size: " + (currentPartyCount + 1));
}

@addMethod(CR4Player) function W3mOnPlayerLeft()
{
    var currentPartyCount : int;
    currentPartyCount = W3mGetPartyCount();

    if (currentPartyCount > 1)
    {
        W3mSetPartyCount(currentPartyCount - 1);

        // Clear scaling cache to recalculate for new party size
        W3mClearScalingCache();

        this.DisplayHudMessage("Player left - Party size: " + (currentPartyCount - 1));
    }
}

// ---------------------------------------------------------------------------
// CONSOLE COMMANDS - TESTING & DEBUG
// ---------------------------------------------------------------------------

exec function W3mTestScaling()
{
    var targetNPC : CActor;
    var partyCount : int;

    // Get nearest NPC
    targetNPC = thePlayer.GetDisplayTarget();

    if (!targetNPC)
    {
        thePlayer.DisplayHudMessage("No NPC targeted");
        return;
    }

    partyCount = W3mGetPartyCount();
    W3mApplyPartyScaling((CEntity)targetNPC, partyCount);

    thePlayer.DisplayHudMessage("Applied scaling to " + targetNPC.GetReadableName());
}

exec function W3mSetTestPartyCount(count : int)
{
    W3mSetPartyCount(count);
    thePlayer.DisplayHudMessage("Party count set to: " + count);
}

exec function W3mShowScalingInfo()
{
    var partyCount : int;
    var healthMult : float;
    var damageMult : float;

    partyCount = W3mGetPartyCount();
    healthMult = W3mCalculateHealthMultiplier(partyCount);
    damageMult = W3mCalculateDamageMultiplier(partyCount, true);

    thePlayer.DisplayHudMessage("Party: " + partyCount + " | Health: " + healthMult + "x | Boss Dmg: " + damageMult + "x");
}
