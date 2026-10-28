@echo off
title Parar Servidor CTR
cd /d "%~dp0"

echo ================================================================
echo   ENCERRANDO SERVIDOR CTR TURBOCHARGED...
echo ================================================================

for /f "tokens=5" %%a in ('netstat -aon ^| findstr :8080 ^| findstr LISTENING') do (
    echo [*] Finalizando processo PID: %%a (Porta 8080)
    taskkill /f /pid %%a >nul 2>nul
)

for /f "tokens=5" %%a in ('netstat -aon ^| findstr :31850') do (
    echo [*] Finalizando processo PID: %%a (Porta 31850)
    taskkill /f /pid %%a >nul 2>nul
)

echo [OK] Servidor encerrado com sucesso.
timeout /t 2 >nul
