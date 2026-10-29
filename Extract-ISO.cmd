@echo off
rem Prepares data\ from the disc image(s) in iso\ (see README, "GitHub Actions"). Needs Python 3.12+ and FFmpeg.
cd /d "%~dp0"
where py >nul 2>nul
if %errorlevel%==0 (
    py -3 "%~dp0tools\extract_iso.py" %*
) else (
    python "%~dp0tools\extract_iso.py" %*
)
set code=%errorlevel%
if not "%RECVX_NO_PAUSE%"=="1" pause
exit /b %code%
