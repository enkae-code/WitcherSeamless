// WITCHERSEAMLESS CORE
struct W3mNetworkStats { var sessionState : string; var rttMs : int; var connectedPlayers : int; }
import function W3mGetNetworkStats() : W3mNetworkStats;
@addField(CR4Player) var w3mMonitorEnabled : bool;

function W3mUpdateMonitor() {
    var stats : W3mNetworkStats;
    var vis : CScriptedRenderComponent;
    if (!thePlayer || !thePlayer.w3mMonitorEnabled) return;
    stats = W3mGetNetworkStats();
    vis = thePlayer.GetVisualDebug();
    if (!vis) return;
    vis.AddText('W3M', "Seamless: " + stats.sessionState, 0.85, 0.05, true, 0, Color(255, 255, 255));
}

@addMethod(CR4Player) function W3mLoop(dt : float, id : int) { W3mUpdateMonitor(); }

@addMethod(CR4Player) function OnSpawned(spawnData : SEntitySpawnData) {
    this.w3mMonitorEnabled = true;
    this.AddTimer('W3mLoop', 0.0, true);
    this.DisplayHudMessage("W3M Online");
}
