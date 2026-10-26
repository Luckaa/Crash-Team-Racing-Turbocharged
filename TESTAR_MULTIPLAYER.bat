@echo off
title CTR Turbocharged - Teste Automatizado Multiplayer
cd /d "%~dp0"

echo ========================================================
echo   CRASH TEAM RACING: TURBOCHARGED
echo   TESTE AUTOMATIZADO MULTIPLAYER (2, 3 e 4 Jogadores)
echo ========================================================
echo.
echo Este script executara testes completos de rede local:
echo   1. 2 Jogadores Arcade (com Bots)
echo   2. 2 Jogadores VS
echo   3. 3 Jogadores Arcade (com Bots)
echo   4. 3 Jogadores VS
echo   5. 4 Jogadores Arcade (com Bots)
echo   6. 4 Jogadores VS
echo.
echo As instancias serao abertas automaticamente, conectarao,
echo carregarao a pista e disputarao a corrida para validar que
echo nao ha crash, congelamento ou dessincronizacao.
echo.
pause

python tools\test_multiplayer.py

echo.
pause
