@echo off
title CTR Turbocharged - Multiplayer Server
cd /d "%~dp0"

echo ================================================================
echo   CRASH TEAM RACING: TURBOCHARGED - MULTIPLAYER MASTER SERVER  
echo ================================================================
echo.

where node >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERRO] Node.js nao foi encontrado no sistema!
    echo Por favor, instale o Node.js v18+ baixando de: https://nodejs.org
    echo.
    pause
    exit /b 1
)

echo [*] Verificando versao do Node.js:
node -v
echo.
echo [*] Iniciando Lobby e Relay Server...
echo.

node src/server.js
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERRO] O servidor foi encerrado inesperadamente.
    pause
)
