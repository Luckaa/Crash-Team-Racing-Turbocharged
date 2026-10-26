@echo off
title Meu IP Local / Wi-Fi - CTR Multiplayer
cd /d "%~dp0"
echo ========================================================
echo          SEUS ENDERECOS IP PARA MULTIPLAYER
echo ========================================================
echo.
echo Seus IPs locais encontrados nesta maquina:
echo.
powershell -NoProfile -Command "Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.InterfaceAlias -notlike '*Loopback*' } | ForEach-Object { Write-Host ('   - ' + $_.InterfaceAlias + ': ' + $_.IPAddress) -ForegroundColor Cyan }"
echo.
echo --------------------------------------------------------
echo [DICA PARA JOGAR NA MESMA CASA / MESMO WI-FI]:
echo Se voce for o HOST, o outro computador deve colocar o seu
echo IP mostrado acima (geralmente comeca com 192.168.x.x)
echo usando o arquivo 'CONFIGURAR_IP.bat'.
echo.
echo [OBSERVACAO]:
echo Na versao mais recente, computadores no mesmo Wi-Fi
echo tambem se conectam automaticamente via busca local!
echo --------------------------------------------------------
echo.
pause
