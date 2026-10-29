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

    # 0. Clean up any obsolete/confusing files from previous packaging
    junk_files = [
        "CONECTAR_RADMIN_VPN.bat",
        "CONECTAR_REDE_LOCAL.bat",
        "ENTRAR_NA_PARTIDA.bat",
        "CONFIGURAR_SERVIDOR_ONLINE.bat",
        "VER_MEU_IP_WIFI.bat",
        "LIBERAR_FIREWALL.bat",
        "CONFIGURAR_IP.bat",
        "COMO_JOGAR_ONLINE.txt",
        "online_server.txt",
    ]
    for junk in junk_files:
        p = STANDALONE_DIR / junk
        if p.exists():
            try:
                p.unlink()
                print(f"[-] Removed obsolete file: {junk}")
            except Exception:
                pass

    cache_dir = STANDALONE_DIR / ".cache"
    if cache_dir.exists():
        shutil.rmtree(cache_dir, ignore_errors=True)

    # 1. Copy Executable
    print("[+] Copying Crash Team Racing - Turbocharged.exe...")
    shutil.copy2(exe_path, STANDALONE_DIR / "Crash Team Racing - Turbocharged.exe")

    # 2. Copy config.ini if exists
    config_src = BUILD_RELEASE / "config.ini"
    if config_src.exists():
        print("[+] Copying config.ini...")
        shutil.copy2(config_src, STANDALONE_DIR / "config.ini")

    # 3. Handle multiplayer_ip.txt
    target_ip = custom_host_ip if custom_host_ip else "26.235.216.50"
    ip_content = (
        "# Configuracao de IP para Conexao Multiplayer\r\n"
        "# IP do Host pre-configurado:\r\n"
        f"{target_ip}\r\n"
    )
    (STANDALONE_DIR / "multiplayer_ip.txt").write_text(ip_content, encoding="utf-8")
    print(f"[+] Configured multiplayer_ip.txt with Host IP: {target_ip}")

    # 4. Copy assets directory
    assets_src = BUILD_RELEASE / "assets"
    if not assets_src.exists():
        assets_src = ROOT / "assets"
    
    assets_dst = STANDALONE_DIR / "assets"
    print(f"[+] Copying assets from {assets_src}...")
    shutil.copytree(assets_src, assets_dst, dirs_exist_ok=True)

    # 5. Create super-simple JOGAR.bat
    jogar_bat = STANDALONE_DIR / "JOGAR.bat"
    jogar_bat.write_text(
        "@echo off\r\n"
        "title Crash Team Racing - Turbocharged\r\n"
        "cd /d \"%~dp0\"\r\n"
        "echo ========================================================\r\n"
        "echo       Crash Team Racing - Turbocharged (Multiplayer)\r\n"
        "echo ========================================================\r\n"
        "echo.\r\n"
        "echo Iniciando o jogo...\r\n"
        "echo.\r\n"
        "echo DICA: No menu principal do jogo, va em:\r\n"
        "echo       MULTIPLAYER -^> BUSCAR SALA\r\n"
        "echo.\r\n"
        "echo Bom jogo!\r\n"
        "echo.\r\n"
        "start \"\" \"Crash Team Racing - Turbocharged.exe\"\r\n",
        encoding="utf-8"
    )

    # 6. Create clean and friendly LEIA-ME_PRIMEIRO.txt
    help_txt = STANDALONE_DIR / "LEIA-ME_PRIMEIRO.txt"
    help_txt.write_text(
        "========================================================================\r\n"
        "             CRASH TEAM RACING - TURBOCHARGED (MULTIPLAYER)\r\n"
        "========================================================================\r\n"
        "\r\n"
        "Ola! Este jogo e 100% portatil: voce NAO precisa instalar nada!\r\n"
        "Ja esta tudo pronto e configurado para jogar com seu amigo.\r\n"
        "\r\n"
        "------------------------------------------------------------------------\r\n"
        "   COMO JOGAR (APENAS 3 PASSOS SIMPLES):\r\n"
        "------------------------------------------------------------------------\r\n"
        "\r\n"
        " Passo 1: Abra o Radmin VPN e conecte na mesma rede do seu amigo.\r\n"
        "\r\n"
        " Passo 2: De dois cliques no arquivo:\r\n"
        "          -> \"JOGAR.bat\"  (ou no \"Crash Team Racing - Turbocharged.exe\")\r\n"
        "\r\n"
        " Passo 3: No menu principal do jogo, va em:\r\n"
        "          -> MULTIPLAYER\r\n"
        "          -> BUSCAR SALA\r\n"
        "\r\n"
        " Pronto! Voce ja vai entrar na sala do Host automaticamente!\r\n"
        "\r\n"
        "------------------------------------------------------------------------\r\n"
        "   CONTROLES NO TECLADO (ou conecte seu controle USB / Xbox / PS4):\r\n"
        "------------------------------------------------------------------------\r\n"
        " - Acelerar:          Tecla C ou Enter\r\n"
        " - Freio / Re:        Tecla X\r\n"
        " - Usar Item:         Tecla V\r\n"
        " - Pular / Drift:     Barra de Espaco ou Shift\r\n"
        " - Olhar para tras:   Tecla B\r\n"
        " - Voltar / Cancelar: Tecla Esc ou Tecla D\r\n"
        " - Direcao:           Setas do Teclado\r\n"
        "\r\n"
        " (Se plugar controle USB de PS4, Xbox ou Generico, funciona na hora!)\r\n"
        "\r\n"
        "------------------------------------------------------------------------\r\n"
        "   DUVIDAS OU AJUDA:\r\n"
        "------------------------------------------------------------------------\r\n"
        " - Se outro amigo for o Host e precisar trocar o IP, ou se precisar\r\n"
        "   liberar portas no Firewall do Windows, veja a pasta \"extras\".\r\n"
        "========================================================================\r\n",
        encoding="utf-8"
    )

    # 7. Create extras folder with optional tools so they don't clutter root
    extras_dir = STANDALONE_DIR / "extras"
    extras_dir.mkdir(parents=True, exist_ok=True)

    trocar_ip_bat = extras_dir / "Trocar_IP.bat"
    trocar_ip_bat.write_text(
        "@echo off\r\n"
        "setlocal enabledelayedexpansion\r\n"
        "title Trocar IP do Host - CTR Turbocharged\r\n"
        "cd /d \"%~dp0\\..\"\r\n"
        "\r\n"
        "echo ========================================================\r\n"
        "echo               CONFIGURAR NOVO IP DO HOST\r\n"
        "echo ========================================================\r\n"
        "echo.\r\n"
        "echo IP atualmente configurado:\r\n"
        "type multiplayer_ip.txt 2>nul\r\n"
        "echo.\r\n"
        "echo --------------------------------------------------------\r\n"
        "echo Se outro amigo for o Host (Criar a sala), digite o IP dele.\r\n"
        "echo Ou aperte ENTER para manter o IP atual.\r\n"
        "echo --------------------------------------------------------\r\n"
        "echo.\r\n"
        "set /p NOVO_IP=\"Digite o novo IP do Host: \"\r\n"
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

    firewall_src = ROOT / "LIBERAR_FIREWALL.bat"
    if firewall_src.exists():
        shutil.copy2(firewall_src, extras_dir / "Liberar_Firewall.bat")

    # 8. Create ZIP archive
    print(f"[+] Creating zip archive: {ZIP_PATH} ...")
    if ZIP_PATH.exists():
        ZIP_PATH.unlink()

    with zipfile.ZipFile(ZIP_PATH, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=4) as zf:
        for root, dirs, files in os.walk(STANDALONE_DIR):
            if ".cache" in dirs:
                dirs.remove(".cache")
            for file in files:
                file_path = Path(root) / file
                archive_name = file_path.relative_to(DIST_DIR)
                zf.write(file_path, arcname=str(archive_name))

    print(f"\n[SUCCESS] Package created successfully!")
    print(f"  - Standalone Folder: {STANDALONE_DIR}")
    print(f"  - Zip Archive:       {ZIP_PATH} ({ZIP_PATH.stat().st_size / (1024*1024):.2f} MB)")

if __name__ == "__main__":
    import sys
    custom_ip = sys.argv[1] if len(sys.argv) > 1 else "26.235.216.50"
    create_package(custom_ip)
