@echo off
title Liberar Firewall - CTR Multiplayer
cd /d "%~dp0"
echo ========================================================
echo     CONFIGURANDO O WINDOWS DEFENDER FIREWALL
echo ========================================================
echo.
echo Solicitando permissao de Administrador para liberar
echo a conexao multiplayer do jogo...
echo.
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [AVISO] Solicitando elevacao de Administrador...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

netsh advfirewall firewall delete rule name="CTR Turbocharged UDP 31847" >nul 2>&1
netsh advfirewall firewall add rule name="CTR Turbocharged UDP 31847" dir=in action=allow protocol=UDP localport=31847 >nul
netsh advfirewall firewall add rule name="CTR Turbocharged UDP 31847" dir=out action=allow protocol=UDP localport=31847 >nul

netsh advfirewall firewall delete rule name="CTR Turbocharged Exe" >nul 2>&1
if exist "%~dp0Crash Team Racing - Turbocharged.exe" (
    netsh advfirewall firewall add rule name="CTR Turbocharged Exe" dir=in action=allow program="%~dp0Crash Team Racing - Turbocharged.exe" enable=yes >nul
    netsh advfirewall firewall add rule name="CTR Turbocharged Exe" dir=out action=allow program="%~dp0Crash Team Racing - Turbocharged.exe" enable=yes >nul
)
if exist "%~dp0build-msvc-x86\Release\Crash Team Racing - Turbocharged.exe" (
    netsh advfirewall firewall add rule name="CTR Turbocharged Exe Dev" dir=in action=allow program="%~dp0build-msvc-x86\Release\Crash Team Racing - Turbocharged.exe" enable=yes >nul
)

echo.
echo [SUCESSO!] As portas do multiplayer foram liberadas com sucesso no Windows Defender Firewall!
echo Agora os computadores na mesma rede Wi-Fi e pela internet conseguem se comunicar sem bloqueio.
echo.
pause
