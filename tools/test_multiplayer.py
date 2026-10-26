import os
import sys
import time
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD_RELEASE = ROOT / "build-msvc-x86" / "Release"
EXE_PATH = BUILD_RELEASE / "Crash Team Racing - Turbocharged.exe"

TESTS = [
    {"name": "2 Players Arcade", "mode": "arcade", "players": 2, "level": 0, "laps": 3, "frames": 120, "port": 31851},
    {"name": "2 Players VS",     "mode": "vs",     "players": 2, "level": 0, "laps": 3, "frames": 120, "port": 31852},
    {"name": "3 Players Arcade", "mode": "arcade", "players": 3, "level": 0, "laps": 3, "frames": 120, "port": 31853},
    {"name": "3 Players VS",     "mode": "vs",     "players": 3, "level": 0, "laps": 3, "frames": 120, "port": 31854},
    {"name": "4 Players Arcade", "mode": "arcade", "players": 4, "level": 0, "laps": 3, "frames": 120, "port": 31855},
    {"name": "4 Players VS",     "mode": "vs",     "players": 4, "level": 0, "laps": 3, "frames": 120, "port": 31856},
]

def run_test(test_config, timeout_sec=50):
    name = test_config["name"]
    mode = test_config["mode"]
    players = test_config["players"]
    level = test_config["level"]
    laps = test_config["laps"]
    frames = test_config["frames"]
    port = test_config["port"]

    print("\n" + "=" * 70)
    print(f"[*] INICIANDO TESTE: {name}")
    print(f"    Modo: {mode.upper()} | Jogadores: {players} | Laps: {laps} | Frames: {frames} | Porta: {port}")
    print("=" * 70)

    processes = []
    
    # 1. Launch Host
    host_cmd = [
        str(EXE_PATH),
        "--mp-port", str(port),
        "--mp-ip", "127.0.0.1",
        "--mp-timeout", str(timeout_sec),
        "--mp-test-host", mode, str(players), str(level), str(laps), str(frames)
    ]
    print(f"  [>] Iniciando HOST...")
    host_proc = subprocess.Popen(host_cmd, cwd=str(BUILD_RELEASE))
    processes.append(("HOST", host_proc))

    # Give host 2 seconds to initialize socket & enter menu
    time.sleep(2.0)

    # 2. Launch Clients
    for c_idx in range(1, players):
        client_cmd = [
            str(EXE_PATH),
            "--mp-port", str(port),
            "--mp-ip", "127.0.0.1",
            "--mp-timeout", str(timeout_sec),
            "--mp-test-client", mode, str(c_idx), str(frames)
        ]
        print(f"  [>] Iniciando CLIENT {c_idx} (P{c_idx+1})...")
        c_proc = subprocess.Popen(client_cmd, cwd=str(BUILD_RELEASE))
        processes.append((f"CLIENT {c_idx}", c_proc))
        time.sleep(0.5)

    # 3. Wait for all processes to complete
    start_time = time.time()
    all_done = False
    
    while time.time() - start_time < timeout_sec:
        all_done = True
        for role, proc in processes:
            ret = proc.poll()
            if ret is None:
                all_done = False
                break
            elif ret != 0:
                print(f"  [!] ERRO: {role} terminou com codigo de erro {ret}!")
                # Terminate remaining
                for _, p in processes:
                    if p.poll() is None:
                        p.terminate()
                return False
        if all_done:
            break
        time.sleep(0.5)

    if not all_done:
        print(f"  [!] TIMEOUT ({timeout_sec}s)! Finalizando processos restantes...")
        for role, proc in processes:
            if proc.poll() is None:
                print(f"      Matando {role}...")
                proc.terminate()
                try:
                    proc.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    proc.kill()
        return False

    elapsed = time.time() - start_time
    print(f"  [+] {name} PASSOU COM SUCESSO! (Duracao: {elapsed:.1f}s)")
    return True

def main():
    if not EXE_PATH.exists():
        print(f"[!] Executavel nao encontrado: {EXE_PATH}")
        sys.exit(1)

    target_filter = sys.argv[1].lower() if len(sys.argv) > 1 else None

    tests_to_run = []
    for t in TESTS:
        if target_filter:
            # check if filter matches name, e.g. "2p", "3p", "4p", "arcade", "vs"
            matched = False
            for part in target_filter.split(","):
                part = part.strip()
                if part in t["name"].lower() or part == t["mode"].lower():
                    matched = True
                    break
            if matched:
                tests_to_run.append(t)
        else:
            tests_to_run.append(t)

    if not tests_to_run:
        print(f"[!] Nenhum teste encontrado com o filtro '{target_filter}'.")
        print("    Filtros disponiveis: 2p, 3p, 4p, arcade, vs (ou combinacoes)")
        sys.exit(1)

    print("=" * 70)
    print("   CRASH TEAM RACING: TURBOCHARGED - MULTIPLAYER TEST SUITE")
    print(f"   Total de testes a executar: {len(tests_to_run)}")
    print("=" * 70)

    results = []
    for t in tests_to_run:
        success = run_test(t)
        results.append((t["name"], success))
        time.sleep(1.0) # Grace period between tests for sockets to close

    print("\n" + "=" * 70)
    print("                    RESUMO DOS TESTES")
    print("=" * 70)
    total_passed = 0
    for name, success in results:
        status = "PASSED [OK]" if success else "FAILED [X]"
        if success:
            total_passed += 1
        print(f"  - {name:<25}: {status}")
    print("=" * 70)
    print(f"Resultado Final: {total_passed}/{len(results)} testes passaram.")
    
    if total_passed == len(results):
        print("\n>>> TODOS OS TESTES PASSARAM PERFEITAMENTE! <<<\n")
        sys.exit(0)
    else:
        print("\n>>> ALGUNS TESTES FALHARAM! <<<\n")
        sys.exit(1)

if __name__ == "__main__":
    main()
