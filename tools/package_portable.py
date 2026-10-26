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

    # Clean target folder
    if STANDALONE_DIR.exists():
        shutil.rmtree(STANDALONE_DIR)
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
    shutil.copytree(assets_src, assets_dst)

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

    # 7. Create COMO_JOGAR_ONLINE.txt
    help_txt = STANDALONE_DIR / "COMO_JOGAR_ONLINE.txt"
    help_txt.write_text(
        "========================================================================\r\n"
        "   CRASH TEAM RACING - TURBOCHARGED (PC NATIVE MULTIPLAYER)\r\n"
        "========================================================================\r\n"
        "\r\n"
        "Este pacote e 100%% PORTATIL: Nao precisa instalar nada!\r\n"
        "Funciona em qualquer Windows 64-bit / 32-bit.\r\n"
        "\r\n"
        "------------------------------------------------------------------------\r\n"
        "1. COMO JOGAR:\r\n"
        "------------------------------------------------------------------------\r\n"
        "Basta dar 2 cliques em \"JOGAR.bat\" ou \"Crash Team Racing - Turbocharged.exe\".\r\n"
        "\r\n"
        "------------------------------------------------------------------------\r\n"
        "2. COMO JOGAR MULTIPLAYER (2 a 4 Jogadores):\r\n"
        "------------------------------------------------------------------------\r\n"
        "O jogo possui modo Multiplayer nativo de 2, 3 e 4 jogadores (com bots completando ate 8)!\r\n"
        "\r\n"
        "A) O JOGADOR QUE FOR CRIAR A SALA (HOST):\r\n"
        "   1. Abra o jogo (\"JOGAR.bat\").\r\n"
        "   2. No menu principal, va na opcao MULTIPLAYER e escolha CRIAR SALA.\r\n"
        "   3. Aguarde os outros jogadores entrarem na sala (aparecera 2/4, 3/4, 4/4).\r\n"
        "   4. Aperte o botao de iniciar (Start / X) para abrir a escolha de pista.\r\n"
        "   * DICA: Se for a primeira vez, execute 'LIBERAR_FIREWALL.bat' para\r\n"
        "     garantir que o Windows Defender nao bloqueie a conexao.\r\n"
        "\r\n"
        "B) OS JOGADORES QUE FOREM ENTRAR (CLIENTS):\r\n"
        "   - NA MESMA CASA / MESMO WI-FI:\r\n"
        "     O jogo possui busca automatica na rede local!\r\n"
        "     Basta abrir o jogo (\"ENTRAR_NA_PARTIDA.bat\") e em MULTIPLAYER escolher BUSCAR SALA.\r\n"
        "     (Se o seu roteador bloquear broadcast, abra 'VER_MEU_IP_WIFI.bat' no PC do Host,\r\n"
        "      pegue o IP local dele ex: 192.168.x.x e coloque no 'CONFIGURAR_IP.bat' do Client).\r\n"
        "\r\n"
        "   - EM CASAS DIFERENTES (PELA INTERNET):\r\n"
        "     1. De 2 cliques em \"CONFIGURAR_IP.bat\".\r\n"
        "     2. Digite o IP do jogador Host (ex: IP do Radmin VPN 26.x.x.x ou IP publico / Playit.gg).\r\n"
        "     3. Abra o jogo e va em MULTIPLAYER -> BUSCAR SALA.\r\n"
        "\r\n"
        "------------------------------------------------------------------------\r\n"
        "3. CONTROLES PADRAO (TECLADO):\r\n"
        "------------------------------------------------------------------------\r\n"
        "- Acelerar (X): Tecla C\r\n"
        "- Freio / Re (Quadrado): Tecla X\r\n"
        "- Usar Item (Circulo): Tecla V\r\n"
        "- Pular / Drift (L1/R1): Teclas Shift / Barra de Espaco\r\n"
        "- Direcionais: Setas do Teclado\r\n"
        "- Pause: Enter\r\n"
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
