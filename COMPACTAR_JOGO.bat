@echo off
title Gerador de Pacote Portatil - CTR Turbocharged
cd /d "%~dp0"
echo ========================================================
echo   Empacotando CTR Turbocharged (Standalone / Zip)
echo ========================================================
echo.
python "%~dp0tools\package_portable.py"
echo.
pause
