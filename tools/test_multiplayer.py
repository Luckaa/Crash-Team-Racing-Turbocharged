import os
import sys
import time
import glob
import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD_RELEASE = ROOT / "build-msvc-x86" / "Release"
EXE_PATH = BUILD_RELEASE / "Crash Team Racing - Turbocharged.exe"

# All 18 Race Tracks in CTR
TRACKS = [
    {"id": 0,  "name": "Dingo Canyon",    "keys": ["dingo", "canyon", "dingocanyon", "0"]},
    {"id": 1,  "name": "Dragon Mines",    "keys": ["dragon", "mines", "dragonmines", "1"]},
    {"id": 2,  "name": "Blizzard Bluff",  "keys": ["blizzard", "bluff", "blizzardbluff", "2"]},
    {"id": 3,  "name": "Crash Cove",      "keys": ["cove", "crashcove", "banner", "3"]},
    {"id": 4,  "name": "Tiger Temple",    "keys": ["tiger", "temple", "tigertemple", "4"]},
    {"id": 5,  "name": "Papu's Pyramid",  "keys": ["papu", "pyramid", "papuspyramid", "5"]},
    {"id": 6,  "name": "Roo's Tubes",     "keys": ["roo", "tubes", "roostubes", "6"]},
    {"id": 7,  "name": "Hot Air Skyway",  "keys": ["skyway", "hotair", "hotairskyway", "7"]},
    {"id": 8,  "name": "Sewer Speedway",  "keys": ["sewer", "speedway", "sewerspeedway", "8"]},
    {"id": 9,  "name": "Mystery Caves",   "keys": ["mystery", "caves", "mysterycaves", "9"]},
    {"id": 10, "name": "Cortex Castle",   "keys": ["cortex", "castle", "cortexcastle", "10"]},
    {"id": 11, "name": "N. Gin Labs",     "keys": ["ngin", "labs", "nginlabs", "11"]},
    {"id": 12, "name": "Polar Pass",      "keys": ["polar", "pass", "polarpass", "12"]},
    {"id": 13, "name": "Oxide Station",   "keys": ["oxide", "station", "oxidestation", "13"]},
    {"id": 14, "name": "Coco Park",       "keys": ["coco", "park", "cocopark", "14"]},
    {"id": 15, "name": "Tiny Arena",      "keys": ["tiny", "arena", "tinyarena", "15"]},
    {"id": 16, "name": "Slide Coliseum",  "keys": ["slide", "coliseum", "slidecoliseum", "16"]},
    {"id": 17, "name": "Turbo Track",     "keys": ["turbo", "track", "turbotrack", "17"]},
]

TRACK_BY_ID = {t["id"]: t for t in TRACKS}

SMOKE_SUITE = [
    # 2 Players
    {"track_id": 3,  "mode": "arcade", "players": 2, "name": "2P Arcade - Crash Cove (Banner Test)"},
    {"track_id": 3,  "mode": "vs",     "players": 2, "name": "2P VS - Crash Cove (Banner Test)"},
    {"track_id": 0,  "mode": "vs",     "players": 2, "name": "2P VS - Dingo Canyon"},
    # 3 Players
    {"track_id": 4,  "mode": "arcade", "players": 3, "name": "3P Arcade - Tiger Temple (Flame Jets)"},
    {"track_id": 2,  "mode": "vs",     "players": 3, "name": "3P VS - Blizzard Bluff (Snowballs)"},
    # 4 Players
    {"track_id": 12, "mode": "arcade", "players": 4, "name": "4P Arcade - Polar Pass (Seals)"},
    {"track_id": 10, "mode": "vs",     "players": 4, "name": "4P VS - Cortex Castle (Spiders)"},
    {"track_id": 7,  "mode": "arcade", "players": 4, "name": "4P Arcade - Hot Air Skyway"},
]

def find_track(query):
    query_str = str(query).strip().lower()
    for t in TRACKS:
        if str(t["id"]) == query_str or t["name"].lower() == query_str:
            return t
        for k in t["keys"]:
            if k == query_str:
                return t
    for t in TRACKS:
        if query_str in t["name"].lower():
            return t
    return None

def print_log_tail(prefix="TEST CLIENT", max_lines=25):
    """Prints the tail of the most recent log file matching prefix."""
    try:
        logs = list(BUILD_RELEASE.glob(f"CTR Turbocharged [{prefix}*.log"))
        if not logs:
            return
        latest_log = max(logs, key=lambda p: p.stat().st_mtime)
        print(f"\n      --- Ultimas linhas do log: {latest_log.name} ---")
        with open(latest_log, "r", encoding="utf-8", errors="replace") as f:
            lines = f.readlines()
            for line in lines[-max_lines:]:
                print(f"      {line.rstrip()}")
        print("      --------------------------------------------------")
    except Exception as e:
        print(f"      [!] Falha ao ler log: {e}")

def run_test(test_config, timeout_sec=50, verbose=True):
    name = test_config["name"]
    mode = test_config["mode"]
    players = test_config["players"]
    level = test_config["level"]
    laps = test_config.get("laps", 3)
    frames = test_config.get("frames", 60)
    port = test_config.get("port", 31850)

    if verbose:
        print("\n" + "=" * 70)
        print(f"[*] INICIANDO TESTE: {name}")
        print(f"    Pista: {TRACK_BY_ID.get(level, {}).get('name', f'ID {level}')} (LEV={level})")
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
    if verbose:
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
        if verbose:
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
                print_log_tail(role)
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
                print_log_tail(role)
                proc.terminate()
                try:
                    proc.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    proc.kill()
        return False

    elapsed = time.time() - start_time
    if verbose:
        print(f"  [+] {name} PASSOU COM SUCESSO! (Duracao: {elapsed:.1f}s)")
    return True

def generate_tests(tracks, modes, players_list, frames=250, laps=3, base_port=31850):
    tests = []
    port = base_port
    for t in tracks:
        for m in modes:
            for p in players_list:
                name = f"{p}P {m.upper()} - {t['name']}"
                tests.append({
                    "name": name,
                    "mode": m,
                    "players": p,
                    "level": t["id"],
                    "laps": laps,
                    "frames": frames,
                    "port": port
                })
                port += 1
                if port > 32200:
                    port = base_port
    return tests

def main():
    if not EXE_PATH.exists():
        print(f"[!] Executavel nao encontrado: {EXE_PATH}")
        sys.exit(1)

    parser = argparse.ArgumentParser(description="Suite de Testes Multiplayer do CTR Turbocharged")
    parser.add_argument("filter", nargs="?", default=None,
                        help="Filtro rapido de teste (ex: smoke, all, cove, tiger, 2p, 3p, 4p, arcade, vs)")
    parser.add_argument("--map", "-m", default=None,
                        help="Nome ou ID da pista (ex: 'cove', '3', 'tiger', 'all')")
    parser.add_argument("--mode", default=None, choices=["arcade", "vs", "all"],
                        help="Modo de jogo: arcade, vs ou all")
    parser.add_argument("--players", "-p", default=None,
                        help="Quantidade de jogadores (ex: 2, 3, 4 ou 'all')")
    parser.add_argument("--frames", "-f", type=int, default=250,
                        help="Quantidade de frames ativos de simulacao para testar (padrao: 250, cobrindo semaforo verde em frame 180)")
    parser.add_argument("--timeout", "-t", type=int, default=60,
                        help="Timeout maximo em segundos por teste (padrao: 60)")
    parser.add_argument("--laps", "-l", type=int, default=3,
                        help="Numero de voltas da corrida (padrao: 3)")
    parser.add_argument("--full-race", action="store_true",
                        help="Executa corrida completa com 1500 frames de simulacao")
    parser.add_argument("--quick", action="store_true",
                        help="Executa versao rapida de 60 frames (apenas smoke de conexao/carregamento)")
    parser.add_argument("--all-tracks", action="store_true",
                        help="Executa 1 teste para cada uma das 18 pistas")
    parser.add_argument("--smoke", action="store_true",
                        help="Executa suite 'smoke' cobrindo 2P, 3P, 4P em Arcade e VS em pistas chave")
    parser.add_argument("--full-matrix", action="store_true",
                        help="Executa matriz completa (todas 18 pistas x 2P, 3P, 4P x Arcade e VS = 108 testes)")

    args = parser.parse_args()

    if args.quick:
        args.frames = 60
    elif args.full_race:
        args.frames = 1500

    # Calculate safe timeout based on frames requested
    calculated_timeout = max(args.timeout, int(args.frames / 20) + 45)
    args.timeout = calculated_timeout

    # Determine what tests to build
    raw_filter = args.filter.lower() if args.filter else ""
    
    # Check if smoke requested
    if args.smoke or raw_filter == "smoke":
        tests_to_run = []
        port = 31850
        for s in SMOKE_SUITE:
            t = TRACK_BY_ID[s["track_id"]]
            tests_to_run.append({
                "name": s["name"],
                "mode": s["mode"],
                "players": s["players"],
                "level": t["id"],
                "laps": args.laps,
                "frames": args.frames,
                "port": port
            })
            port += 1
    elif args.full_matrix or raw_filter in ("full", "full-matrix", "matrix"):
        tests_to_run = generate_tests(TRACKS, ["arcade", "vs"], [2, 3, 4], frames=args.frames, laps=args.laps)
    elif args.all_tracks or raw_filter == "all-tracks":
        # 1 test per track cycling modes and player counts
        tests_to_run = []
        port = 31850
        cycle_modes = ["arcade", "vs"]
        cycle_players = [2, 3, 4]
        for i, t in enumerate(TRACKS):
            m = cycle_modes[i % len(cycle_modes)]
            p = cycle_players[i % len(cycle_players)]
            tests_to_run.append({
                "name": f"{p}P {m.upper()} - {t['name']}",
                "mode": m,
                "players": p,
                "level": t["id"],
                "laps": args.laps,
                "frames": args.frames,
                "port": port
            })
            port += 1
    else:
        # Determine tracks
        selected_tracks = TRACKS
        if args.map:
            if args.map.lower() != "all":
                t = find_track(args.map)
                if not t:
                    print(f"[!] Pista '{args.map}' nao encontrada.")
                    sys.exit(1)
                selected_tracks = [t]
        elif raw_filter and raw_filter not in ("all", "2p", "3p", "4p", "arcade", "vs"):
            # Check if raw_filter matches a track
            t = find_track(raw_filter)
            if t:
                selected_tracks = [t]

        # Determine modes
        selected_modes = ["arcade", "vs"]
        if args.mode:
            selected_modes = ["arcade", "vs"] if args.mode == "all" else [args.mode]
        elif "arcade" in raw_filter:
            selected_modes = ["arcade"]
        elif "vs" in raw_filter:
            selected_modes = ["vs"]

        # Determine player counts
        selected_players = [2, 3, 4]
        if args.players:
            if args.players.lower() != "all":
                selected_players = [int(x.strip()) for x in args.players.split(",") if x.strip().isdigit()]
        elif "2p" in raw_filter:
            selected_players = [2]
        elif "3p" in raw_filter:
            selected_players = [3]
        elif "4p" in raw_filter:
            selected_players = [4]

        tests_to_run = generate_tests(selected_tracks, selected_modes, selected_players, frames=args.frames, laps=args.laps)

    if not tests_to_run:
        print("[!] Nenhum teste selecionado.")
        sys.exit(1)

    print("=" * 70)
    print("   CRASH TEAM RACING: TURBOCHARGED - MULTIPLAYER TEST SUITE")
    print(f"   Total de testes a executar: {len(tests_to_run)}")
    print(f"   Frames por teste: {args.frames} | Timeout por teste: {args.timeout}s")
    print("=" * 70)

    results = []
    start_all = time.time()
    for idx, t in enumerate(tests_to_run, 1):
        print(f"\n>>> [{idx}/{len(tests_to_run)}] Executando: {t['name']}")
        success = run_test(t, timeout_sec=args.timeout, verbose=True)
        results.append((t["name"], success))
        time.sleep(1.0) # Grace period for sockets to be released

    total_time = time.time() - start_all

    print("\n" + "=" * 70)
    print("                    RESUMO DOS TESTES")
    print("=" * 70)
    total_passed = 0
    for name, success in results:
        status = "PASSED [OK]" if success else "FAILED [X]"
        if success:
            total_passed += 1
        print(f"  - {name:<45}: {status}")
    print("=" * 70)
    print(f"Resultado Final: {total_passed}/{len(results)} testes passaram em {total_time:.1f}s.")
    
    if total_passed == len(results):
        print("\n>>> TODOS OS TESTES PASSARAM PERFEITAMENTE! <<<\n")
        sys.exit(0)
    else:
        print("\n>>> ALGUNS TESTES FALHARAM! <<<\n")
        sys.exit(1)

if __name__ == "__main__":
    main()
