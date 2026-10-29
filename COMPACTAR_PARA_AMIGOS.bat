@echo off
setlocal enabledelayedexpansion
title Compactar CTR Turbocharged para Amigos
cd /d "%~dp0"

echo ========================================================
echo   COMPACTAR CTR TURBOCHARGED PARA ENVIAR AOS AMIGOS
echo ========================================================
echo.
echo Selecione qual IP do seu computador sera pre-configurado
echo no arquivo ZIP para que seus amigos se conectem a voce:
echo.
echo [1] IP Radmin VPN:      26.235.216.50 (Recomendado se usam Radmin)
echo [2] IP Rede Local/LAN:  192.168.100.8 (Mesmo Wi-Fi ou Cabo)
echo [3] IP Internet Direta: 189.31.159.94 (Com porta 31847 aberta no roteador)
echo [4] Digitar outro IP ou Dominio / Porta (ex: Playit.gg)
echo.
set /p OPCAO="Escolha uma opcao [1-4] (Padrao 1): "

if "%OPCAO%"=="" set OPCAO=1
if "%OPCAO%"=="1" set HOST_IP=26.235.216.50
if "%OPCAO%"=="2" set HOST_IP=192.168.100.8
if "%OPCAO%"=="3" set HOST_IP=189.31.159.94
if "%OPCAO%"=="4" (
    set /p HOST_IP="Digite o IP ou endereco do Host: "
)

echo.
echo [*] Gerando pacote ZIP super simplificado para o Host: !HOST_IP! ...
python tools\package_portable.py !HOST_IP!
echo.
echo ========================================================
echo [CONCLUIDO COM SUCESSO!]
echo.
echo O arquivo ZIP que voce deve mandar para os seus amigos:
echo %CD%\CTR_Turbocharged_Multiplayer.zip
echo.
echo O que seus amigos precisam fazer:
echo   1. Baixar e descompactar o arquivo .zip
echo   2. Entrar na sua sala no Radmin VPN
echo   3. Dar dois cliques em "JOGAR.bat"
echo   4. No jogo: MULTIPLAYER -^> BUSCAR SALA
echo ========================================================
echo.
echo Pressione qualquer tecla para abrir a pasta do ZIP...
pause >nul
explorer.exe /select,"%~dp0CTR_Turbocharged_Multiplayer.zip"
