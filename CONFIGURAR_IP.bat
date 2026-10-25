@echo off
setlocal enabledelayedexpansion
title Configurador de IP - CTR Multiplayer
cd /d "%~dp0"

echo ========================================================
echo     CRASH TEAM RACING - TURBOCHARGED (MULTIPLAYER)
echo ========================================================
echo.
echo IP Atual configurado:
if exist "multiplayer_ip.txt" (
    type multiplayer_ip.txt
) else if exist "build-msvc-x86\Release\multiplayer_ip.txt" (
    type build-msvc-x86\Release\multiplayer_ip.txt
)
echo.
echo --------------------------------------------------------
echo Se voce for o HOST (Criar a sala):
echo   - Nao precisa alterar nada! Basta abrir o jogo.
echo.
echo Se voce for ENTRAR na sala de outro amigo (CLIENT):
echo   - Digite o IP do Host (ex: 192.168.1.15 ou IP da VPN):
echo --------------------------------------------------------
echo.
set /p NOVO_IP="Digite o IP do Host (ou aperte ENTER para manter): "

if not "!NOVO_IP!"=="" (
    echo # Configuracao de IP para Conexao Multiplayer > "%~dp0multiplayer_ip.txt"
    echo !NOVO_IP! >> "%~dp0multiplayer_ip.txt"
    if exist "%~dp0build-msvc-x86\Release" (
        copy /y "%~dp0multiplayer_ip.txt" "%~dp0build-msvc-x86\Release\multiplayer_ip.txt" >nul
    )
    if exist "%~dp0dist\CTR_Turbocharged_Multiplayer" (
        copy /y "%~dp0multiplayer_ip.txt" "%~dp0dist\CTR_Turbocharged_Multiplayer\multiplayer_ip.txt" >nul
    )
    echo.
    echo [OK] IP atualizado com sucesso para: !NOVO_IP!
) else (
    echo.
    echo [INFO] IP mantido sem alteracoes.
)
echo.
pause
