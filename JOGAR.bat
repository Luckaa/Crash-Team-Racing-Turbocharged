@echo off
setlocal
title Crash Team Racing - Turbocharged
cd /d "%~dp0"

if exist "build-msvc-x86\Release\Crash Team Racing - Turbocharged.exe" (
    cd /d "%~dp0build-msvc-x86\Release"
    start "" "Crash Team Racing - Turbocharged.exe"
    exit /b 0
)

if exist "dist\CTR_Turbocharged_Multiplayer\Crash Team Racing - Turbocharged.exe" (
    cd /d "%~dp0dist\CTR_Turbocharged_Multiplayer"
    start "" "Crash Team Racing - Turbocharged.exe"
    exit /b 0
)

echo [AVISO] Executavel compilado nao foi encontrado em build-msvc-x86\Release.
echo Tentando compilar o jogo automaticamente...
echo.
pause
call "%~dp0build-msvc.bat"
if exist "build-msvc-x86\Release\Crash Team Racing - Turbocharged.exe" (
    cd /d "%~dp0build-msvc-x86\Release"
    start "" "Crash Team Racing - Turbocharged.exe"
)
