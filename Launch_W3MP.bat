@ECHO OFF
echo [DEPLOY] Syncing files...
copy /Y "C:\Developer\WitcherSeamless\legacy_archive\dxgi.dll" "C:\Program Files (x86)\Steam\steamapps\common\The Witcher 3\bin\x64_dx12\bink2w64.dll"
copy /Y "C:\Developer\WitcherSeamless\build\artifacts-release\scripting_experiments.dll" "C:\Program Files (x86)\Steam\steamapps\common\The Witcher 3\bin\x64_dx12\plugins\scripting_experiments.dll"

echo [DEPLOY] Deploying Master Script...
if not exist "C:\Program Files (x86)\Steam\steamapps\common\The Witcher 3\mods\modWitcherSeamless\content\scripts\local" mkdir "C:\Program Files (x86)\Steam\steamapps\common\The Witcher 3\mods\modWitcherSeamless\content\scripts\local"
copy /Y "C:\Developer\WitcherSeamless\data\scripts\local\W3M_Core.ws" "C:\Program Files (x86)\Steam\steamapps\common\The Witcher 3\mods\modWitcherSeamless\content\scripts\local\"

echo [RESET] Nuking Cache...
del /F /Q "C:\Program Files (x86)\Steam\steamapps\common\The Witcher 3\content\content0\scripts.blob"

echo [LAUNCH] Starting via Steam (Triggering User Properties)...
start steam://rungameid/292030
