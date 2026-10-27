@echo off
title CTR Turbocharged - Teste Automatizado Multiplayer
cd /d "%~dp0"

if not "%~1"=="" (
    python tools\test_multiplayer.py %*
    goto :end
)

:menu
cls
echo ========================================================
echo   CRASH TEAM RACING: TURBOCHARGED
echo   SUITE DE TESTES AUTOMATIZADOS MULTIPLAYER
echo ========================================================
echo.
echo Escolha a opcao de teste:
echo   [1] Smoke Test (2P, 3P e 4P em Arcade e VS - passa sinal verde)
echo   [2] Testar 1 corrida em CADA UMA das 18 pistas (18 testes)
echo   [3] Escolher uma pista especifica (ex: Crash Cove, Polar Pass, etc.)
echo   [4] Partida Completa (Corrida longa: 1500 frames / voltas completas)
echo   [5] Matriz Completa (Todas as 18 pistas x 2P/3P/4P x Arcade/VS = 108 testes)
echo   [6] Teste Unitario Nativo (Pneus, Divisoes por Zero e Sub-sistemas C)
echo   [7] Sair
echo.
set /p OPTION="Digite a opcao desejada [1-7]: "

if "%OPTION%"=="1" (
    python tools\test_multiplayer.py --smoke
) else if "%OPTION%"=="2" (
    python tools\test_multiplayer.py --all-tracks
) else if "%OPTION%"=="3" (
    echo.
    set /p TRACK="Digite o nome ou ID da pista (ex: cove, polar, tiger, 3): "
    set /p PLAYERS="Digite a quantidade de jogadores (2, 3, 4 ou all): "
    set /p MODE="Digite o modo (arcade, vs ou all): "
    python tools\test_multiplayer.py --map %TRACK% --players %PLAYERS% --mode %MODE%
) else if "%OPTION%"=="4" (
    echo.
    set /p TRACK="Digite o nome ou ID da pista (ou pressione Enter para Crash Cove): "
    if "%TRACK%"=="" set TRACK=cove
    set /p PLAYERS="Digite a quantidade de jogadores [2, 3 ou 4]: "
    if "%PLAYERS%"=="" set PLAYERS=2
    set /p MODE="Digite o modo [arcade ou vs]: "
    if "%MODE%"=="" set MODE=arcade
    python tools\test_multiplayer.py --map %TRACK% --players %PLAYERS% --mode %MODE% --full-race
) else if "%OPTION%"=="5" (
    python tools\test_multiplayer.py --full-matrix
) else if "%OPTION%"=="6" (
    echo.
    echo Executando testes unitarios nativos de seguranca...
    .\build-msvc-x86\Release\ctr_native_tires_test.exe
    echo.
    .\build-msvc-x86\Release\ctr_native_banner_test.exe
) else (
    goto :end
)

:end
echo.
pause
