#!/usr/bin/env python
"""
Comando di avvio:
    python run_batch_simulations.py

Opzioni disponibili:
    --start_at ID       Inizia le simulazioni dalla scena specificata (es. --start_at 28)
    --scenes ID,ID,...  Specifica una lista precisa di scene da eseguire (es. --scenes 19,29)
    --data PATH         Cambia il percorso del dataset (default: data/V2X-Sim/V2X-Sim-det/test)
    --resume PATH       Cambia il checkpoint del modello (default: disco/no_rsu/epoch_100.pth)

Esempio Ripresa:
    python run_batch_simulations.py --start_at 27

Descrizione:
    Lo script esegue automaticamente le 4 casistiche (Konro ON/OFF x OMNeT ON/OFF) per ogni scena.
    Valida i risultati (check zero-val) e genera i grafici comparativi nella cartella ./results_scene/
"""

import subprocess
import os
import sys
import json
import argparse
import shutil
import time
import signal
import threading
from pathlib import Path

# --- CONFIGURAZIONE LOGICA ---
DEFAULT_SCENES = [5, 8, 19, 27, 28, 29, 91, 92, 96, 97]
BASE_DIR = Path(__file__).parent.absolute()
# Python executable del virtual environment coperception
# Aggiornare il percorso se conda è installato in una directory diversa (es. anaconda3)
PYTHON_EXEC = Path.home() / "miniconda3" / "envs" / "coperception" / "bin" / "python"
RESULTS_ROOT = BASE_DIR / "results_scene"
LOGS_DIR = BASE_DIR / "logs" / "ab"
CHECKPOINT = BASE_DIR / "data" / "V2X-Sim" / "V2X-Sim-2.0" / "checkpoints" / "det" / "disco" / "no_rsu" / "epoch_100.pth"
DATASET_DIR = BASE_DIR / "data" / "V2X-Sim" / "V2X-Sim-det" / "test"

# I file json temporanei per raccogliere la storia durante la singola scena
HIST_WITH_KONRO_ON = LOGS_DIR / "temp_with_konro_with_omnet.json"
HIST_NO_KONRO_ON = LOGS_DIR / "temp_without_konro_with_omnet.json"
HIST_WITH_KONRO_OFF = LOGS_DIR / "temp_with_konro_without_omnet.json"
HIST_NO_KONRO_OFF = LOGS_DIR / "temp_without_konro_without_omnet.json"

def _drain_stdout(proc, prefix=None):
    """Drena lo stdout di un processo in background per evitare il blocco del pipe buffer.
    Se prefix è None, i messaggi vengono soppressi (solo drain silenzioso).
    """
    try:
        for line in iter(proc.stdout.readline, ""):
            if prefix is not None:
                stripped = line.strip()
                if stripped:
                    print(f"{prefix} {stripped}")
    except (ValueError, OSError):
        pass  # Processo terminato, pipe chiuso


def run_command(cmd_list):
    """Esegue un comando mostrando l'output in real-time."""
    print(f"\n[EXEC] {' '.join(cmd_list)}")
    process = subprocess.Popen(
        cmd_list,
        stdout=sys.stdout,
        stderr=sys.stderr,
        text=True,
        bufsize=1
    )
    exit_code = process.wait()
    return exit_code

def validate_json(path):
    """Verifica che l'ultima run nel file JSON non sia vuota o fallita."""
    if not os.path.exists(path):
        return False, "File non trovato"
    
    try:
        with open(path, "r") as f:
            data = json.load(f)
            runs = data.get("runs", [])
            if not runs:
                return False, "Nessuna run trovata nel JSON"
            
            last_run = runs[-1]
            if not last_run.get("run_completed_scene", False):
                return False, "Run marcata come non completata"
            
            # Controllo metriche (Network tx o Proxy recall)
            net_tx = last_run.get("network", {}).get("tx_total", 0)
            proxy_mean = last_run.get("proxy", {}).get("proxy_mean", 0.0)
            
            # Se la run non ha trasmesso nulla o ha recall zero assoluto, qualcosa non va
            if net_tx == 0 and not last_run["run"]["network_disable"]:
                return False, "Zero pacchetti trasmessi in simulazione OMNeT"
            
            # Se rsu/altri agenti non hanno prodotto detection
            if proxy_mean <= 0.0:
                print(f"[WARNING] Recall media a 0.0 per la scena attuale.")
                
            return True, "OK"
    except Exception as e:
        return False, f"Errore parsing JSON: {e}"

def start_omnet():
    """Avvia OMNeT++ e attende che sia pronto."""
    omnet_dir = BASE_DIR / "omnet_sim"
    cmd = "bash -c 'source ~/omnetpp-6.0.3/setenv && ./omnet_sim -n \".:../../../inet/src\" -u Cmdenv'"
    print(f"\n[START] Avvio OMNeT++ Server...")
    process = subprocess.Popen(
        cmd,
        cwd=str(omnet_dir),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        shell=True,
        text=True,
        preexec_fn=os.setsid
    )
    
    # Attende il messaggio di pronto
    for line in iter(process.stdout.readline, ""):
        print(f"[OMNeT] {line.strip()}")
        if "Waiting for Python connection on port 5555" in line:
            print("[OMNeT] Server pronto.")
            # Avvia thread daemon silenzioso: drena lo stdout senza stamparlo
            threading.Thread(target=_drain_stdout, args=(process,), daemon=True).start()
            return process
    return process

def start_konro():
    """Avvia Konro RM con sudo e attende che sia pronto."""
    konro_dir = BASE_DIR / "konro" / "build" / "rm"
    cmd = "echo 'Alberto03' | sudo -S ./konro"
    print(f"\n[START] Avvio Konro Resource Manager...")
    process = subprocess.Popen(
        cmd,
        cwd=str(konro_dir),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        shell=True,
        text=True,
        preexec_fn=os.setsid
    )
    
    # Attende il messaggio di pronto
    for line in iter(process.stdout.readline, ""):
        print(f"[Konro] {line.strip()}")
        if "KONROHTTP server starting" in line:
            print("[Konro] Manager pronto.")
            # Avvia thread daemon silenzioso: drena lo stdout senza stamparlo
            threading.Thread(target=_drain_stdout, args=(process,), daemon=True).start()
            return process
    return process

def stop_service(proc, name):
    """Termina un processo e il suo gruppo."""
    if proc:
        print(f"[STOP] Termino {name}...")
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            # Diamo tempo per chiudere, poi kill -9 se necessario
            time.sleep(1)
            if proc.poll() is None:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except ProcessLookupError:
            pass
        proc.wait()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--start_at", type=int, help="ID della scena da cui partire")
    parser.add_argument("--scenes", type=str, help="Lista di scene separate da virgola (es. 19,29)")
    parser.add_argument("--data", default=str(DATASET_DIR), help="Percorso dataset")
    parser.add_argument("--resume", default=str(CHECKPOINT), help="Percorso checkpoint")
    args = parser.parse_args()

    # Determina la lista delle scene
    if args.scenes:
        scenes_to_run = [int(x.strip()) for x in args.scenes.split(",")]
    else:
        scenes_to_run = DEFAULT_SCENES

    if args.start_at:
        if args.start_at in scenes_to_run:
            idx = scenes_to_run.index(args.start_at)
            scenes_to_run = scenes_to_run[idx:]
        else:
            print(f"Errore: Scena {args.start_at} non trovata nella lista disponibile.")
            sys.exit(1)

    # Assicura che le cartelle esistano
    RESULTS_ROOT.mkdir(parents=True, exist_ok=True)
    LOGS_DIR.mkdir(parents=True, exist_ok=True)

    print(f"=== AVVIO BATCH SIMULATION ===")
    print(f"Scene da elaborare: {scenes_to_run}")
    print("I servizi OMNeT++ e Konro verranno avviati e chiusi automaticamente per ogni run.\n")

    for scene in scenes_to_run:
        print(f"\n************************************************************")
        print(f"*** ELABORAZIONE SCENA {scene} ***")
        print(f"************************************************************")
        
        scene_results_dir = RESULTS_ROOT / f"results_scene_{scene}"
        scene_results_dir.mkdir(exist_ok=True)

        # Configurazione test (Base Command)
        base_cmd = [
            str(PYTHON_EXEC), str(BASE_DIR / "tools" / "det" / "test_codet_network.py"),
            "--com", "disco",
            "--data", args.data,
            "--resume", args.resume,
            "--logpath", str(BASE_DIR / "logs"),
            "--scene_id", str(scene),
            "--network_host", "127.0.0.1",
            "--network_port", "5555"
        ]

        # --- CASO 1: KONRO ON / OMNeT ON ---
        omnet_proc = start_omnet()
        konro_proc = start_konro()
        
        cmd1 = base_cmd + [
            "--konro_enable", "--konro_target", "0.80", "--konro_interval", "1", "--konro_agent_id", "1",
            "--summary_out", str(HIST_WITH_KONRO_ON)
        ]
        res1 = run_command(cmd1)
        
        stop_service(konro_proc, "Konro")
        stop_service(omnet_proc, "OMNeT")

        if res1 != 0:
            print(f"ERRORE critico nella scena {scene} (Konro ON/OMNeT ON). Fermo.")
            sys.exit(1)
            
        valid, msg = validate_json(HIST_WITH_KONRO_ON)
        if not valid:
            print(f"Validazione fallita per Scena {scene} (Konro ON/OMNeT ON): {msg}")
            sys.exit(1)

        # --- CASO 2: KONRO ON / OMNeT OFF ---
        konro_proc = start_konro()
        cmd2 = base_cmd + [
            "--konro_enable", "--konro_target", "0.80", "--konro_interval", "1", "--network_disable",
            "--summary_out", str(HIST_WITH_KONRO_OFF)
        ]
        res2 = run_command(cmd2)
        stop_service(konro_proc, "Konro")
        if res2 != 0:
            print(f"ERRORE critico nella scena {scene} (Konro ON/OMNeT OFF). Fermo.")
            sys.exit(1)
        valid, msg = validate_json(HIST_WITH_KONRO_OFF)
        if not valid:
            print(f"Validazione fallita per Scena {scene} (Konro ON/OMNeT OFF): {msg}")
            sys.exit(1)
        # --- CASO 3: KONRO OFF / OMNeT ON ---
        omnet_proc = start_omnet()
        
        cmd3 = base_cmd + [
            "--summary_out", str(HIST_NO_KONRO_ON)
        ]
        res3 = run_command(cmd3)
        
        stop_service(omnet_proc, "OMNeT")

        if res3 != 0:
            print(f"ERRORE critico nella scena {scene} (Konro OFF/OMNeT ON). Fermo.")
            sys.exit(1)
            
        valid, msg = validate_json(HIST_NO_KONRO_ON)
        if not valid:
            print(f"Validazione fallita per Scena {scene} (Konro OFF/OMNeT ON): {msg}")
            sys.exit(1)

        # --- CASO 4: KONRO OFF / OMNeT OFF ---
        cmd4 = base_cmd + [
            "--network_disable",
            "--summary_out", str(HIST_NO_KONRO_OFF)
        ]
        if run_command(cmd4) != 0:
            print(f"ERRORE critico nella scena {scene} (Konro OFF/OMNeT OFF). Fermo.")
            sys.exit(1)
        valid, msg = validate_json(HIST_NO_KONRO_OFF)
        if not valid:
            print(f"Validazione fallita per Scena {scene} (Konro OFF/OMNeT OFF): {msg}")
            sys.exit(1)
        # --- GENERAZIONE GRAFICI ---
        print(f"\n[PLOT] Generazione grafici comparativi per scena {scene}...")
        
        # 1. GRAFICO: IMPATTO RETE (Ideale vs Degradata)
        # (omnet OFF + Konro OFF) VS (omnet ON + Konro OFF)
        plot_impact_cmd = [
            str(PYTHON_EXEC), str(BASE_DIR / "tools" / "det" / "plot_comparison.py"),
            "--file-a", str(HIST_NO_KONRO_OFF),
            "--file-b", str(HIST_NO_KONRO_ON),
            "--idx-file-a", "-1",
            "--idx-file-b", "-1",
            "--label-a", "Ideal (No Network)",
            "--label-b", "Degraded (OMNeT)",
            "--out", str(scene_results_dir / f"network_impact_details_s{scene}.png"),
            "--out-overlay", str(scene_results_dir / f"network_impact_overlay_s{scene}.png"),
            "--overlay-pu-source", "none"
        ]
        run_command(plot_impact_cmd)

        # 2. GRAFICO: EFFICACIA KONRO (Degradata vs Corretta)
        # (omnet ON + Konro OFF) VS (omnet ON + Konro ON)
        plot_konro_cmd = [
            str(PYTHON_EXEC), str(BASE_DIR / "tools" / "det" / "plot_comparison.py"),
            "--file-a", str(HIST_NO_KONRO_ON),
            "--file-b", str(HIST_WITH_KONRO_ON),
            "--idx-file-a", "-1",
            "--idx-file-b", "-1",
            "--label-a", "Without Konro",
            "--label-b", "With Konro",
            "--out", str(scene_results_dir / f"konro_efficacy_details_s{scene}.png"),
            "--out-overlay", str(scene_results_dir / f"konro_efficacy_overlay_s{scene}.png")
        ]
        run_command(plot_konro_cmd)

        # 3. Grafico Comparazione Metriche Globali (Konro ON/OFF con OMNeT attivo)
        plot_comp_cmd = [
            str(PYTHON_EXEC), str(BASE_DIR / "tools" / "det" / "compare_konro_runs.py"),
            "--with-konro", str(HIST_WITH_KONRO_ON),
            "--with-konro-run-index", "-1",
            "--without-konro", str(HIST_NO_KONRO_ON),
            "--without-konro-run-index", "-1",
            "--out", str(scene_results_dir / f"global_comparison_s{scene}.json"),
            "--plot-out", str(scene_results_dir / f"global_metrics_s{scene}.png")
        ]
        run_command(plot_comp_cmd)


        print(f"\n[COMPLETATA] Scena {scene}. Grafici salvati in: {scene_results_dir}")

    print("\n============================================================")
    print("TUTTE LE SIMULAZIONI COMPLETATE CON SUCCESSO.")
    print("============================================================")

if __name__ == "__main__":
    main()
