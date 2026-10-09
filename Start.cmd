@echo off
setlocal
python "%~dp0tools\check_no_original_core.py" "%~dp0."
if errorlevel 1 exit /b 1
if exist "%~dp0build\packages\windows-x64-v0.5\recvx-runtime.exe" (
    python "%~dp0tools\check_no_original_core.py" "%~dp0." --binary "%~dp0build\packages\windows-x64-v0.5\recvx-runtime.exe"
    if errorlevel 1 exit /b 1
    call "%~dp0build\packages\windows-x64-v0.5\Start.cmd" %*
    goto result
)
if exist "%~dp0build\windows-native\Release\recvx-runtime.exe" (
    python "%~dp0tools\check_no_original_core.py" "%~dp0." --binary "%~dp0build\windows-native\Release\recvx-runtime.exe"
    if errorlevel 1 exit /b 1
    "%~dp0build\windows-native\Release\recvx-runtime.exe" --data "%~dp0data" --mods "%~dp0mods" --controls "%~dp0controls.ini" --debug-config "%~dp0debug.ini" %*
    goto result
)
if exist "%~dp0build\windows-native\recvx-runtime.exe" (
    python "%~dp0tools\check_no_original_core.py" "%~dp0." --binary "%~dp0build\windows-native\recvx-runtime.exe"
    if errorlevel 1 exit /b 1
    "%~dp0build\windows-native\recvx-runtime.exe" --data "%~dp0data" --mods "%~dp0mods" --controls "%~dp0controls.ini" --debug-config "%~dp0debug.ini" %*
    goto result
)
if exist "%~dp0build\windows-x64\recvx-runtime.exe" (
    python "%~dp0tools\check_no_original_core.py" "%~dp0." --binary "%~dp0build\windows-x64\recvx-runtime.exe"
    if errorlevel 1 exit /b 1
    "%~dp0build\windows-x64\recvx-runtime.exe" --data "%~dp0data" --mods "%~dp0mods" --controls "%~dp0controls.ini" --debug-config "%~dp0debug.ini" %*
    goto result
)
echo Build fehlt. Siehe STARTEN.md: build.ps1 --platform windows
pause
exit /b 1
:result
set "recvx_exit=%errorlevel%"
if not "%recvx_exit%" == "0" pause
exit /b %recvx_exit%
