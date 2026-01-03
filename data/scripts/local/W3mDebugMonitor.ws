// ===========================================================================
// WITCHERSEAMLESS MULTIPLAYER - LIVE MONITOR OVERLAY
// ===========================================================================
// Production-Grade Telemetry Display
// CDPR Native UI - Top-Right HUD Overlay
// ===========================================================================

// ---------------------------------------------------------------------------
// NETWORK STATS STRUCTURE
// ---------------------------------------------------------------------------

struct W3mNetworkStats
{
	var sessionState : string;
	var rttMs : int;
	var packetsPerSecond : int;
	var xorActive : bool;
	var handshakeComplete : bool;
	var connectedPlayers : int;
}

// ---------------------------------------------------------------------------
// C++ BRIDGE IMPORT
// ---------------------------------------------------------------------------

import function W3mGetNetworkStats() : W3mNetworkStats;

// ---------------------------------------------------------------------------
// GLOBAL MONITOR STATE - Extend CR4Player
// ---------------------------------------------------------------------------

@addField(CR4Player)
var w3mMonitorEnabled : bool;

// ---------------------------------------------------------------------------
// MONITOR TOGGLE COMMAND
// ---------------------------------------------------------------------------

exec function W3mToggleMonitor()
{
	if (!thePlayer)
	{
		return;
	}

	thePlayer.w3mMonitorEnabled = !thePlayer.w3mMonitorEnabled;

	if (thePlayer.w3mMonitorEnabled)
	{
		W3mNativeUI_ShowMessage("[LIVE MONITOR] Enabled - Top-right overlay active", 0x00FF00);
	}
	else
	{
		W3mNativeUI_ShowMessage("[LIVE MONITOR] Disabled", 0xFFFFFF);
	}
}

// ---------------------------------------------------------------------------
// LIVE MONITOR TICK - CALLED EVERY FRAME
// ---------------------------------------------------------------------------

function W3mUpdateMonitor()
{
	var stats : W3mNetworkStats;
	var visualDebug : CScriptedRenderComponent;
	var posX : float;
	var posY : float;
	var lineHeight : float;
	var statusColor : string;

	if (!thePlayer || !thePlayer.w3mMonitorEnabled)
	{
		return;
	}

	// Fetch network telemetry from C++
	stats = W3mGetNetworkStats();

	// Get visual debug component
	visualDebug = thePlayer.GetVisualDebug();
	if (!visualDebug)
	{
		return;
	}

	// Top-right corner positioning
	posX = 0.85;
	posY = 0.05;
	lineHeight = 0.03;

	// Determine status color (Green = OK, Yellow = Warning, Red = Error)
	if (stats.rttMs < 100 && stats.packetsPerSecond > 0)
	{
		statusColor = "00FF00"; // Green (OK)
	}
	else if (stats.rttMs < 200)
	{
		statusColor = "FFFF00"; // Yellow (Warning)
	}
	else
	{
		statusColor = "FF0000"; // Red (High Latency)
	}

	// HEADER: WITCHERSEAMLESS TELEMETRY (White 0xFFFFFF)
	visualDebug.AddText('W3MP_Monitor_Header', "=== WITCHERSEAMLESS TELEMETRY ===", posX, posY, true, , Color(255, 255, 255));

	// LINE 1: Session State
	posY += lineHeight;
	visualDebug.AddText('W3MP_Monitor_State', "Session: " + stats.sessionState, posX, posY, true, , Color(255, 255, 255));

	// LINE 2: RTT (Ping) with dynamic color
	posY += lineHeight;
	if (statusColor == "00FF00")
	{
		visualDebug.AddText('W3MP_Monitor_RTT', "RTT: " + stats.rttMs + "ms", posX, posY, true, , Color(0, 255, 0));
	}
	else if (statusColor == "FFFF00")
	{
		visualDebug.AddText('W3MP_Monitor_RTT', "RTT: " + stats.rttMs + "ms", posX, posY, true, , Color(255, 255, 0));
	}
	else
	{
		visualDebug.AddText('W3MP_Monitor_RTT', "RTT: " + stats.rttMs + "ms", posX, posY, true, , Color(255, 0, 0));
	}

	// LINE 3: Packets/Sec
	posY += lineHeight;
	visualDebug.AddText('W3MP_Monitor_Packets', "Packets/Sec: " + stats.packetsPerSecond, posX, posY, true, , Color(255, 255, 255));

	// LINE 4: XOR ACTIVE status
	posY += lineHeight;
	if (stats.xorActive)
	{
		visualDebug.AddText('W3MP_Monitor_XOR', "XOR: ACTIVE", posX, posY, true, , Color(0, 255, 0)); // Green
	}
	else
	{
		visualDebug.AddText('W3MP_Monitor_XOR', "XOR: INACTIVE", posX, posY, true, , Color(255, 0, 0)); // Red
	}

	// LINE 5: HANDSHAKE status
	posY += lineHeight;
	if (stats.handshakeComplete)
	{
		visualDebug.AddText('W3MP_Monitor_Handshake', "HANDSHAKE: OK", posX, posY, true, , Color(0, 255, 0)); // Green
	}
	else
	{
		visualDebug.AddText('W3MP_Monitor_Handshake', "HANDSHAKE: PENDING", posX, posY, true, , Color(255, 255, 0)); // Yellow
	}

	// LINE 6: Connected Players
	posY += lineHeight;
	visualDebug.AddText('W3MP_Monitor_Players', "Players: " + stats.connectedPlayers, posX, posY, true, , Color(255, 255, 255));
}

// ---------------------------------------------------------------------------
// AUTO-INIT: Hook into existing multiplayer tick
// ---------------------------------------------------------------------------

event OnW3mMultiplayerTick()
{
	W3mUpdateMonitor();
}

// ---------------------------------------------------------------------------
// AUTO-SHOW UI: Enable monitor on player spawn
// ---------------------------------------------------------------------------

@addMethod(CR4Player)
event OnSpawned(spawnData : SEntitySpawnData)
{
	// Auto-enable monitor when player spawns
	w3mMonitorEnabled = true;
	W3mNativeUI_ShowMessage("[W3M] Live Monitor auto-enabled - Press F2 to toggle", 0x00FF00);
}
