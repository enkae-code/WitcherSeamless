@echo off
setlocal enabledelayedexpansion

:: ===========================================================================
:: WITCHERSEAMLESS - CLEAN & DEPLOY SCRIPT
:: ===========================================================================
:: Removes old script cache and deploys fresh WitcherScript files
:: ===========================================================================

title WitcherSeamless - Clean & Deploy

echo [CLEAN] Starting script cleanup and deployment...

:: Game path
set "GAME_PATH=C:\Program Files (x86)\Steam\steamapps\common\The Witcher 3"

:: Project path
set "PROJECT_PATH=%~dp0"
set "PROJECT_PATH=%PROJECT_PATH:~0,-1%"

echo [CLEAN] Game Path: !GAME_PATH!
echo [CLEAN] Project Path: !PROJECT_PATH!

:: ---------------------------------------------------------------------------
:: STEP 1: DELETE OLD MULTIPLAYER SCRIPTS FOLDER
:: ---------------------------------------------------------------------------

echo.
echo [CLEAN] Deleting old multiplayer scripts folder...

set "OLD_SCRIPTS=!GAME_PATH!\content\content0\scripts\game\multiplayer"

if exist "!OLD_SCRIPTS!" (
    rmdir /s /q "!OLD_SCRIPTS!"
    echo [CLEAN] Deleted: !OLD_SCRIPTS!
) else (
    echo [CLEAN] Folder does not exist (already clean): !OLD_SCRIPTS!
)

:: ---------------------------------------------------------------------------
:: STEP 2: DELETE COMPILED SCRIPT CACHE (scripts.blob)
:: ---------------------------------------------------------------------------

echo.
echo [CLEAN] Deleting compiled script cache...

set "CACHE_FILE=!GAME_PATH!\content\content0\scripts\game\scripts.blob"

if exist "!CACHE_FILE!" (
    del /f /q "!CACHE_FILE!"
    echo [CLEAN] Deleted cache: !CACHE_FILE!
) else (
    echo [CLEAN] Cache file does not exist: !CACHE_FILE!
)

:: ---------------------------------------------------------------------------
:: STEP 3: RE-CREATE MULTIPLAYER SCRIPTS FOLDER
:: ---------------------------------------------------------------------------

echo.
echo [DEPLOY] Creating fresh multiplayer scripts folder...

set "SCRIPTS_TARGET=!GAME_PATH!\content\content0\scripts\game\multiplayer"

mkdir "!SCRIPTS_TARGET!"

if exist "!SCRIPTS_TARGET!" (
    echo [DEPLOY] Created: !SCRIPTS_TARGET!
) else (
    echo [ERROR] Failed to create folder: !SCRIPTS_TARGET!
    pause
    exit /b 1
)

:: ---------------------------------------------------------------------------
:: STEP 4: DEPLOY FRESH WITCHERSCRIPT FILES
:: ---------------------------------------------------------------------------

echo.
echo [DEPLOY] Copying fresh WitcherScript files...

copy /y "!PROJECT_PATH!\data\scripts\local\*.ws" "!SCRIPTS_TARGET!\" >nul

if !errorlevel! neq 0 (
    echo [ERROR] Failed to copy WitcherScript files!
    pause
    exit /b 1
)

echo [DEPLOY] WitcherScript files deployed successfully!

:: ---------------------------------------------------------------------------
:: STEP 5: VERIFY DEPLOYMENT
:: ---------------------------------------------------------------------------

echo.
echo [VERIFY] Verifying deployment...

if exist "!SCRIPTS_TARGET!\W3mDebugMonitor.ws" (
    echo [VERIFY] Found: W3mDebugMonitor.ws
) else (
    echo [ERROR] Missing: W3mDebugMonitor.ws
    pause
    exit /b 1
)

if exist "!SCRIPTS_TARGET!\w3mp_final.ws" (
    echo [VERIFY] Found: w3mp_final.ws
) else (
    echo [ERROR] Missing: w3mp_final.ws
    pause
    exit /b 1
)

if exist "!SCRIPTS_TARGET!\w3mp_refactored.ws" (
    echo [VERIFY] Found: w3mp_refactored.ws
) else (
    echo [ERROR] Missing: w3mp_refactored.ws
    pause
    exit /b 1
)

:: ---------------------------------------------------------------------------
:: STEP 6: READ LINE 32 OF W3mDebugMonitor.ws
:: ---------------------------------------------------------------------------

echo.
echo [VERIFY] Checking line 32 of W3mDebugMonitor.ws...

findstr /n "^" "!SCRIPTS_TARGET!\W3mDebugMonitor.ws" | findstr /b "32:"

echo.
echo [SUCCESS] Clean and deploy completed!
echo.
echo [INFO] Deployed files:
echo [INFO] - W3mDebugMonitor.ws (Live Monitor overlay)
echo [INFO] - w3mp_final.ws (Production features with Safety Pillars)
echo [INFO] - w3mp_refactored.ws (Core multiplayer logic)
echo.
echo [IMPORTANT] The game will recompile scripts.blob on next launch
echo [IMPORTANT] This may take 10-30 seconds - DO NOT interrupt the process
echo.
echo Press any key to exit...
pause >nul

exit /b 0
