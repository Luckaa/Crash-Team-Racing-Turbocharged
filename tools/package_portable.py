import os
import shutil
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD_RELEASE = ROOT / "build-msvc-x86" / "Release"
DIST_DIR = ROOT / "dist"
STANDALONE_DIR = DIST_DIR / "CTR_Turbocharged_Multiplayer"
ZIP_PATH = ROOT / "CTR_Turbocharged_Multiplayer.zip"

def create_package():
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

    # 3. Copy multiplayer_ip.txt
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
        "O jogo possui modo Multiplayer nativo de 2, 3 e 4 jogadores (sem bots)!\r\n"
        "\r\n"
        "A) O JOGADOR QUE FOR CRIAR A SALA (HOST):\r\n"
        "   1. Abra o jogo (\"JOGAR.bat\").\r\n"
        "   2. No menu principal, va na opcao MULTIPLAYER e escolha CRIAR SALA.\r\n"
        "   3. Aguarde os outros jogadores entrarem na sala (aparecera 2/4, 3/4, 4/4).\r\n"
        "   4. Aperte o botao de iniciar (Start / X) para abrir a escolha de pista.\r\n"
        "\r\n"
        "B) OS JOGADORES QUE FOREM ENTRAR (CLIENTS):\r\n"
        "   1. De 2 cliques em \"CONFIGURAR_IP.bat\".\r\n"
        "   2. Digite o IP do jogador Host e aperte Enter:\r\n"
        "      - OPCAO 1 (Mesma Rede / Wi-Fi da casa - Sem nenhum programa):\r\n"
        "        Coloque o IPv4 local do Host (ex: 192.168.1.15).\r\n"
        "      - OPCAO 2 (Pela Internet direto - Sem nenhum programa instalado):\r\n"
        "        O Host abre a porta 31847 UDP no roteador e passa o IP do site meuip.com.br.\r\n"
        "      - OPCAO 3 (Pela Internet via Playit.gg ou Tunel):\r\n"
        "        Coloque o endereco gerado pelo Playit (ex: 147.185.221.16:12345).\r\n"
        "      - OPCAO 4 (Via VPN virtual - Radmin / Tailscale / ZeroTier):\r\n"
        "        Coloque o IP do Host fornecido pelo programa (ex: 26.x.x.x).\r\n"
        "      - OPCAO 5 (No mesmo PC para testar - 2 janelas):\r\n"
        "        Coloque 127.0.0.1.\r\n"
        "   3. Abra o jogo (\"JOGAR.bat\").\r\n"
        "   4. No menu, va em MULTIPLAYER e escolha BUSCAR SALA.\r\n"
        "   5. Pronto! Voce sera conectado automaticamente a sala do Host.\r\n"
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
        "- Suporta Controles USB (PlayStation, Xbox, Genéricos) nativamente via Plug and Play!\r\n"
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
    create_package()
