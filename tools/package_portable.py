import os
import shutil
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD_RELEASE = ROOT / "build-msvc-x86" / "Release"
DIST_DIR = ROOT / "dist"
STANDALONE_DIR = DIST_DIR / "CTR_Turbocharged_Multiplayer"
ZIP_PATH = ROOT / "CTR_Turbocharged_Multiplayer.zip"

def create_package(custom_host_ip=None):
    print(f"[*] Packaging CTR Turbocharged from: {BUILD_RELEASE}")
    
    exe_path = BUILD_RELEASE / "Crash Team Racing - Turbocharged.exe"
    if not exe_path.exists():
        raise FileNotFoundError(f"Executable not found at {exe_path}")

    # Ensure target folder exists
    STANDALONE_DIR.mkdir(parents=True, exist_ok=True)

    # 1. Copy Executable
    print("[+] Copying Crash Team Racing - Turbocharged.exe...")
    shutil.copy2(exe_path, STANDALONE_DIR / "Crash Team Racing - Turbocharged.exe")

    # 2. Copy config.ini if exists
    config_src = BUILD_RELEASE / "config.ini"
    if config_src.exists():
        print("[+] Copying config.ini...")
        shutil.copy2(config_src, STANDALONE_DIR / "config.ini")

    # 3. Handle multiplayer_ip.txt
    if custom_host_ip:
        ip_content = (
            "# Configuracao de IP para Conexao Multiplayer (Crash Team Racing - Turbocharged)\r\n"
            "# IP do Host pre-configurado:\r\n"
            f"{custom_host_ip}\r\n"
        )
        (STANDALONE_DIR / "multiplayer_ip.txt").write_text(ip_content, encoding="utf-8")
        print(f"[+] Configured multiplayer_ip.txt with Host IP: {custom_host_ip}")
    else:
        ip_src = ROOT / "multiplayer_ip.txt"
        if not ip_src.exists():
            ip_src = BUILD_RELEASE / "multiplayer_ip.txt"
        if ip_src.exists():
            print("[+] Copying multiplayer_ip.txt...")
            shutil.copy2(ip_src, STANDALONE_DIR / "multiplayer_ip.txt")

    # 4. Copy assets directory
    assets_src = BUILD_RELEASE / "assets"
    if not assets_src.exists():
        assets_src = ROOT / "assets"
    
    assets_dst = STANDALONE_DIR / "assets"
    print(f"[+] Copying assets from {assets_src} (including ctr-u.bin)...")
    shutil.copytree(assets_src, assets_dst, dirs_exist_ok=True)

    # 5. Create JOGAR.bat inside standalone folder
    jogar_bat = STANDALONE_DIR / "JOGAR.bat"
    jogar_bat.write_text(
        "@echo off\r\n"
        "title Crash Team Racing - Turbocharged\r\n"
        "cd /d \"%~dp0\"\r\n"
        "start \"\" \"Crash Team Racing - Turbocharged.exe\"\r\n",
        encoding="utf-8"
    )

    # 5B. Create ENTRAR_NA_PARTIDA.bat inside standalone folder
    entrar_bat = STANDALONE_DIR / "ENTRAR_NA_PARTIDA.bat"
    entrar_bat.write_text(
        "@echo off\r\n"
        "title Crash Team Racing - Turbocharged (Entrar na Partida)\r\n"
        "cd /d \"%~dp0\"\r\n"
        "echo ========================================================\r\n"
        "echo     CONECTANDO A PARTIDA MULTIPLAYER DO SEU AMIGO\r\n"
        "echo ========================================================\r\n"
        "echo.\r\n"
        "echo Host configurado:\r\n"
        "type multiplayer_ip.txt 2>nul\r\n"
        "echo.\r\n"
        "echo [INSTRUCOES]\r\n"
        "echo 1. O jogo vai abrir em instantes.\r\n"
        "echo 2. No menu principal, va em MULTIPLAYER e escolha BUSCAR SALA.\r\n"
        "echo 3. Aguarde o Host iniciar a corrida!\r\n"
        "echo.\r\n"
        "start \"\" \"Crash Team Racing - Turbocharged.exe\"\r\n",
        encoding="utf-8"
    )

    # 5C. Create CONECTAR_REDE_LOCAL.bat inside standalone folder (LAN / Wi-Fi)
    lan_bat = STANDALONE_DIR / "CONECTAR_REDE_LOCAL.bat"
    lan_bat.write_text(
        "@echo off\r\n"
        "title Crash Team Racing - Turbocharged (Rede Local / Wi-Fi)\r\n"
        "cd /d \"%~dp0\"\r\n"
        "echo 192.168.100.8 > multiplayer_ip.txt\r\n"
        "echo ========================================================\r\n"
        "echo   CONECTANDO VIA REDE LOCAL (MESMO WI-FI OU CABO)\r\n"
        "echo   IP do Host: 192.168.100.8\r\n"
        "echo ========================================================\r\n"
        "echo.\r\n"
        "echo No menu principal: MULTIPLAYER -^> BUSCAR SALA.\r\n"
        "echo.\r\n"
        "start \"\" \"Crash Team Racing - Turbocharged.exe\"\r\n",
        encoding="utf-8"
    )

    # 5D. Create CONECTAR_RADMIN_VPN.bat inside standalone folder
    vpn_bat = STANDALONE_DIR / "CONECTAR_RADMIN_VPN.bat"
    vpn_bat.write_text(
        "@echo off\r\n"
        "title Crash Team Racing - Turbocharged (Radmin VPN)\r\n"
        "cd /d \"%~dp0\"\r\n"
        "echo 26.235.216.50 > multiplayer_ip.txt\r\n"
        "echo ========================================================\r\n"
        "echo   CONECTANDO VIA RADMIN VPN\r\n"
        "echo   IP do Host: 26.235.216.50\r\n"
        "echo ========================================================\r\n"
        "echo.\r\n"
        "echo No menu principal: MULTIPLAYER -^> BUSCAR SALA.\r\n"
        "echo.\r\n"
        "start \"\" \"Crash Team Racing - Turbocharged.exe\"\r\n",
        encoding="utf-8"
    )

    # 6. Create CONFIGURAR_IP.bat inside standalone folder
    config_ip_bat = STANDALONE_DIR / "CONFIGURAR_IP.bat"
    config_ip_bat.write_text(
        "@echo off\r\n"
        "setlocal enabledelayedexpansion\r\n"
        "title Configurador de IP - CTR Multiplayer\r\n"
        "cd /d \"%~dp0\"\r\n"
        "\r\n"
        "echo ========================================================\r\n"
        "echo     CRASH TEAM RACING - TURBOCHARGED (MULTIPLAYER)\r\n"
        "echo ========================================================\r\n"
        "echo.\r\n"
        "echo IP Atual configurado:\r\n"
        "type multiplayer_ip.txt 2>nul\r\n"
        "echo.\r\n"
        "echo --------------------------------------------------------\r\n"
        "echo Se voce for o HOST (Criar a sala):\r\n"
        "echo   - Nao precisa alterar nada! Basta abrir o jogo.\r\n"
        "echo.\r\n"
        "echo Se voce for ENTRAR na sala de outro amigo (CLIENT):\r\n"
        "echo   - Digite o IP do Host (ex: 192.168.1.15 ou IP da VPN):\r\n"
        "echo --------------------------------------------------------\r\n"
        "echo.\r\n"
        "set /p NOVO_IP=\"Digite o IP do Host (ou aperte ENTER para manter): \"\r\n"
        "\r\n"
        "if not \"!NOVO_IP!\"==\"\" (\r\n"
        "    echo # Configuracao de IP para Conexao Multiplayer > multiplayer_ip.txt\r\n"
        "    echo !NOVO_IP! >> multiplayer_ip.txt\r\n"
        "    echo.\r\n"
        "    echo [OK] IP atualizado com sucesso para: !NOVO_IP!\r\n"
        ") else (\r\n"
        "    echo.\r\n"
        "    echo [INFO] IP mantido sem alteracoes.\r\n"
        ")\r\n"
        "echo.\r\n"
        "pause\r\n",
        encoding="utf-8"
    )

    # 6B. Copy LIBERAR_FIREWALL.bat & VER_MEU_IP_WIFI.bat
    firewall_src = ROOT / "LIBERAR_FIREWALL.bat"
    if firewall_src.exists():
        shutil.copy2(firewall_src, STANDALONE_DIR / "LIBERAR_FIREWALL.bat")
    
    wifi_src = ROOT / "VER_MEU_IP_WIFI.bat"
    if wifi_src.exists():
        shutil.copy2(wifi_src, STANDALONE_DIR / "VER_MEU_IP_WIFI.bat")

    # 6C. Create online_server.txt
    online_server_src = ROOT / "online_server.txt"
    if not online_server_src.exists():
        online_server_src = BUILD_RELEASE / "online_server.txt"
    if online_server_src.exists():
        shutil.copy2(online_server_src, STANDALONE_DIR / "online_server.txt")
    else:
        (STANDALONE_DIR / "online_server.txt").write_text(
            "# URL do Servidor Central (Lobby e Relay) do CTR Turbocharged\r\n"
            "# Exemplo local: http://127.0.0.1:8080\r\n"
            "# Exemplo pela Internet (Playit.gg / Ngrok / VPS): https://meu-servidor.playit.gg\r\n"
            "http://127.0.0.1:8080\r\n",
            encoding="utf-8"
        )

    # 6D. Create CONFIGURAR_SERVIDOR_ONLINE.bat
    cfg_server_bat = STANDALONE_DIR / "CONFIGURAR_SERVIDOR_ONLINE.bat"
    cfg_server_bat.write_text(
        "@echo off\r\n"
        "title Configurar Servidor Online - CTR Turbocharged\r\n"
        "cd /d \"%~dp0\"\r\n"
        "echo ========================================================\r\n"
        "echo   CONFIGURAR SERVIDOR ONLINE (LOBBY & RELAY)\r\n"
        "echo ========================================================\r\n"
        "echo.\r\n"
        "echo Digite a URL do servidor (exemplo: http://192.168.1.100:8080 ou http://playit.gg...):\r\n"
        "set /p SERVER_URL=\"URL: \"\r\n"
        "if not \"%SERVER_URL%\"==\"\" (\r\n"
        "    echo %SERVER_URL%> online_server.txt\r\n"
        "    echo [OK] Servidor configurado para: %SERVER_URL%\r\n"
        ")\r\n"
        "echo.\r\n"
        "pause\r\n",
        encoding="utf-8"
    )

    # 7. Create COMO_JOGAR_ONLINE.txt
    help_txt = STANDALONE_DIR / "COMO_JOGAR_ONLINE.txt"
    help_txt.write_text(
        "========================================================================\r\n"
        "   CRASH TEAM RACING - TURBOCHARGED (PC ONLINE & LOCAL MULTIPLAYER)\r\n"
        "========================================================================\r\n"
        "\r\n"
        "Este pacote e 100%% PORTATIL: Nao precisa instalar nada!\r\n"
        "Funciona em qualquer Windows 64-bit / 32-bit.\r\n"
        "\r\n"
        "------------------------------------------------------------------------\r\n"
        "1. COMO INICIAR O JOGO:\r\n"
        "------------------------------------------------------------------------\r\n"
        "Basta dar 2 cliques em \"JOGAR.bat\" ou \"Crash Team Racing - Turbocharged.exe\".\r\n"
        "\r\n"
        "------------------------------------------------------------------------\r\n"
        "2. MODO ONLINE (SISTEMA DE SALAS E CODIGOS - SEM VPN):\r\n"
        "------------------------------------------------------------------------\r\n"
        "O jogo agora possui Lobby Online com Salas Publicas e Codigos Unicos!\r\n"
        "\r\n"
        "COMO FUNCIONA:\r\n"
        "  1. O Host inicia o servidor no PC executando \"INICIAR_SERVIDOR.bat\" dentro da pasta 'server'.\r\n"
        "  2. Para jogar pela internet sem mexer no roteador, use o Playit.gg ou ngrok apontado para a porta 8080 (HTTP) e 31850 (UDP).\r\n"
        "  3. No menu principal do jogo, va em MULTIPLAYER -> ONLINE:\r\n"
        "     - CRIAR SALA: cria uma sala nova e gera um codigo de 4 letras (ex: XK82).\r\n"
        "     - SALAS ONLINE: exibe a lista real de salas disponiveis com vagas (ex: 2/4).\r\n"
        "     - BUSCAR POR CODIGO: digite o codigo de 4 letras para entrar diretamente!\r\n"
        "  4. Quando os jogadores entrarem na sala, o Host clica em [INICIAR JOGO] e a corrida comeca automaticamente!\r\n"
        "\r\n"
        "------------------------------------------------------------------------\r\n"
        "3. MODO LOCAL (REDE WI-FI OU CABO NA MESMA CASA):\r\n"
        "------------------------------------------------------------------------\r\n"
        "100%% AUTOMATICO! Nao precisa de servidor nem de conexao com a internet!\r\n"
        "  - Host escolhe: MULTIPLAYER -> LOCAL -> CRIAR SALA\r\n"
        "  - Demais escolhem: MULTIPLAYER -> LOCAL -> ENTRAR NA SALA\r\n"
        "\r\n"
        "------------------------------------------------------------------------\r\n"
        "4. CONTROLES PADRAO (TECLADO):\r\n"
        "------------------------------------------------------------------------\r\n"
        "- Acelerar (X): Tecla C ou Enter\r\n"
        "- Freio / Re (Quadrado): Tecla X\r\n"
        "- Usar Item (Circulo): Tecla V\r\n"
        "- Pular / Drift (L1/R1): Teclas Shift / Barra de Espaco\r\n"
        "- Voltar / Cancelar (Triangulo): Tecla Esc ou Tecla D\r\n"
        "- Direcionais: Setas do Teclado\r\n"
        "- Suporta Controles USB (PlayStation, Xbox, Genericos) nativamente via Plug and Play!\r\n"
        "========================================================================\r\n",
        encoding="utf-8"
    )

    # 8. Create ZIP archive
    print(f"[+] Creating zip archive: {ZIP_PATH} ...")
    if ZIP_PATH.exists():
        ZIP_PATH.unlink()

    # Use ZIP_DEFLATED with compression level 6 (standard) or ZIP_STORED for bin if desired
    with zipfile.ZipFile(ZIP_PATH, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=4) as zf:
        for root, dirs, files in os.walk(STANDALONE_DIR):
            for file in files:
                file_path = Path(root) / file
                archive_name = file_path.relative_to(DIST_DIR)
                print(f"  -> Adding {archive_name}")
                zf.write(file_path, arcname=str(archive_name))

    print(f"\n[SUCCESS] Package created successfully!")
    print(f"  - Standalone Folder: {STANDALONE_DIR}")
    print(f"  - Zip Archive:       {ZIP_PATH} ({ZIP_PATH.stat().st_size / (1024*1024):.2f} MB)")

if __name__ == "__main__":
    import sys
    custom_ip = sys.argv[1] if len(sys.argv) > 1 else "26.235.216.50"
    create_package(custom_ip)
