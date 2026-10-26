@echo off
rem AI play: starts the game with the agent control channel and lets tools\agent_play\driver.py
rem play it through the LLM endpoint configured in .env (RECVX_LLM_BASE_URL/_MODEL/_API_KEY).
rem Extra arguments go to driver.py (e.g. --steps 50, --no-tools, --model name).
setlocal
cd /d "%~dp0"
python "%~dp0tools\check_no_original_core.py" "%~dp0."
if errorlevel 1 exit /b 1
set "recvx_play=%~dp0build\play"
set "recvx_exe="
set "recvx_data=%~dp0data"
set "recvx_mods=%~dp0mods"
if exist "%~dp0build\packages\windows-x64-v0.5\recvx-runtime.exe" (
    set "recvx_exe=%~dp0build\packages\windows-x64-v0.5\recvx-runtime.exe"
    set "recvx_data=%~dp0build\packages\windows-x64-v0.5\data"
    set "recvx_mods=%~dp0build\packages\windows-x64-v0.5\mods"
) else if exist "%~dp0build\windows-native\Release\recvx-runtime.exe" (
    set "recvx_exe=%~dp0build\windows-native\Release\recvx-runtime.exe"
) else if exist "%~dp0build\windows-native\recvx-runtime.exe" (
    set "recvx_exe=%~dp0build\windows-native\recvx-runtime.exe"
) else if exist "%~dp0build\windows-x64\recvx-runtime.exe" (
    set "recvx_exe=%~dp0build\windows-x64\recvx-runtime.exe"
)
if not defined recvx_exe (
    echo Build fehlt. Siehe STARTEN.md: build.ps1 --platform windows
    pause
    exit /b 1
)
python "%~dp0tools\check_no_original_core.py" "%~dp0." --binary "%recvx_exe%"
if errorlevel 1 exit /b 1
if not exist "%~dp0.env" (
    echo Hinweis: keine .env; driver.py nutzt http://localhost:8080/v1 ohne API-Key.
    echo Vorlage: RECVX_LLM_BASE_URL=..., RECVX_LLM_MODEL=..., RECVX_LLM_API_KEY=... ^(siehe docs\AI_PLAY.md^).
)
rem The runtime replays the control file from the start: every session begins with a fresh one.
if not exist "%recvx_play%" mkdir "%recvx_play%"
if exist "%recvx_play%\auto.json" del /q "%recvx_play%\auto.json"
if exist "%recvx_play%\stream.log.lock" del /q "%recvx_play%\stream.log.lock"
(echo 1 autodump 5 auto& echo 2 run auto 18)> "%recvx_play%\live.txt"
start "RECVX Native" "%recvx_exe%" --data "%recvx_data%" --mods "%recvx_mods%" --controls "%~dp0controls.ini" --debug-config "%~dp0debug.ini" --video-config "%~dp0video.ini" --control "%recvx_play%\live.txt" --state "%recvx_play%\live_state.json" --snapshots "%recvx_play%\snap" --enemy-ai on --enemy-grab on
set /a recvx_wait=0
:wait_metrics
if exist "%recvx_play%\auto.json" goto drive
set /a recvx_wait+=1
if %recvx_wait% gtr 60 (
    echo Keine Metriken nach 60 s ^(build\play\auto.json fehlt^).
    pause
    exit /b 1
)
timeout /t 1 /nobreak >nul
goto wait_metrics
:drive
python "%~dp0tools\agent_play\driver.py" --dir "%recvx_play%" %*
set "recvx_exit=%errorlevel%"
if not "%recvx_exit%" == "0" (
    echo driver.py endete mit Status %recvx_exit% ^(Log: build\play\driver.log^). Das Spiel bleibt offen.
    pause
)
exit /b %recvx_exit%
