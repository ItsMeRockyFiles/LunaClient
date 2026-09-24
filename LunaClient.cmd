@echo off
title LunaClient
color 0D

echo.
echo   ╔══════════════════════════════════════════╗
echo   ║           LunaClient v1.0.0              ║
echo   ╚══════════════════════════════════════════╝
echo.

:: Check if UI is built
if not exist "%~dp0ui\dist\index.html" (
    echo   [*] Building UI for the first time...
    echo.
    cd /d "%~dp0ui"
    call npm run build
    echo.
    echo   [✓] UI built successfully
    echo.
)

:: Start the server
echo   [*] Starting LunaClient...
echo.
cd /d "%~dp0server"
node index.js
