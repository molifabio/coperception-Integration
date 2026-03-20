import argparse
import json
import os
import random
import socket
import statistics
import sys
import time
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional

import torch # type: ignore

import test_codet
from coperception.models.det.base.DetModelBase import DetModelBase
from coperception.utils.detection_util import cal_frame_stats
from konro_bridge import PerceptionProxyTracker


@dataclass
class NetworkPhase:
    name: str
    frames: int
    delay_floor_s: float
    drop_prob: float


@dataclass
class NetworkRuntimeStats:
    tx_total: int = 0
    delivered: int = 0
    dropped: int = 0
    live_packets: int = 0
    delayed_packets: int = 0
    stale_packets: int = 0
    underflow_packets: int = 0
    delays_s: List[float] = field(default_factory=list)
    phase_hits: Dict[str, int] = field(default_factory=dict)

    def record(self, *, delivered: bool, delay_s: float, frames_lag: int, stale: bool, underflow: bool, phase_name: str) -> None:
        self.tx_total += 1
        self.phase_hits[phase_name] = self.phase_hits.get(phase_name, 0) + 1
        if delivered:
            self.delivered += 1
            self.delays_s.append(max(0.0, delay_s))
            if frames_lag == 0:
                self.live_packets += 1
            else:
                self.delayed_packets += 1
            if stale:
                self.stale_packets += 1
            if underflow:
                self.underflow_packets += 1
        else:
            self.dropped += 1

    def to_summary(self) -> Dict[str, Any]:
        total = max(1, self.tx_total)
        delivery_ratio = self.delivered / total
        drop_ratio = self.dropped / total
        if self.delays_s:
            delays = sorted(self.delays_s)
            p50 = delays[len(delays) // 2]
            p95 = delays[min(len(delays) - 1, int(len(delays) * 0.95))]
            avg = statistics.fmean(delays)
            dmax = delays[-1]
        else:
            p50 = p95 = avg = dmax = 0.0
        return {
            "tx_total": self.tx_total,
            "delivered": self.delivered,
            "dropped": self.dropped,
            "delivery_ratio": delivery_ratio,
            "drop_ratio": drop_ratio,
            "live_packets": self.live_packets,
            "delayed_packets": self.delayed_packets,
            "stale_packets": self.stale_packets,
            "underflow_packets": self.underflow_packets,
            "latency_avg_s": avg,
            "latency_p50_s": p50,
            "latency_p95_s": p95,
            "latency_max_s": dmax,
            "phase_hits": dict(sorted(self.phase_hits.items())),
        }


def _parse_network_profile(profile: str) -> List[NetworkPhase]:
    """
    Parse profile format:
    "good:120:0.02:0.00,down:60:0.40:0.15,recover:120:0.08:0.02"
    Each phase = name:frames:delay_floor_seconds:drop_probability
    """
    parsed: List[NetworkPhase] = []
    for raw_phase in profile.split(","):
        item = raw_phase.strip()
        if not item:
            continue
        parts = [p.strip() for p in item.split(":")]
        if len(parts) != 4:
            raise ValueError(
                f"Invalid phase '{item}'. Expected name:frames:delay_floor_s:drop_prob"
            )
        name = parts[0]
        frames = max(1, int(parts[1]))
        delay_floor_s = max(0.0, float(parts[2]))
        drop_prob = min(1.0, max(0.0, float(parts[3])))
        parsed.append(NetworkPhase(name, frames, delay_floor_s, drop_prob))

    if not parsed:
        raise ValueError("Network profile is empty after parsing")
    return parsed


def _phase_for_frame(phases: List[NetworkPhase], frame_idx: int) -> NetworkPhase:
    total_cycle = sum(p.frames for p in phases)
    if total_cycle <= 0:
        return phases[0]
    pos = frame_idx % total_cycle
    acc = 0
    for phase in phases:
        acc += phase.frames
        if pos < acc:
            return phase
    return phases[-1]


class OmnetBridge:
    """Thin TCP client that exchanges JSON events with an OMNeT++ server (Asynchronous)."""

    def __init__(
        self,
        *,
        host: str,
        port: int,
        timeout: float,
        default_delay: float,
        fail_open: bool,
        enabled: bool = True,
    ) -> None:
        self.host = host
        self.port = port
        self.timeout = timeout
        self.default_delay = max(0.0, default_delay)
        self.fail_open = fail_open
        self.enabled = enabled
        self._sock: Optional[socket.socket] = None
        self._buffer = ""
        # Cache per i ritardi: Key="sender->receiver", Value=delay_s
        self.latest_delays: Dict[str, float] = {}
        
        if self.enabled:
            self._connect()

    # ------------------------------------------------------------------
    def update_state(self):
        """Legge dal socket in modo non bloccante e aggiorna i ritardi noti."""
        if not self.enabled or not self._sock:
            return

        try:
            while True:
                try:
                    chunk = self._sock.recv(4096)
                    if not chunk:
                        break # Connessione chiusa o errore
                    self._buffer += chunk.decode("utf-8")
                except BlockingIOError:
                    break
                except ConnectionResetError:
                    self._close()
                    return
        except Exception as e:
            print(f"[OmnetBridge] Error reading socket: {e}", file=sys.stderr)
            return

        # Processa le linee complete nel buffer
        while "\n" in self._buffer:
            line, self._buffer = self._buffer.split("\n", 1)
            line = line.strip()
            if not line:
                continue
            
            try:
                msg = json.loads(line)
                # Ci aspettiamo: {"type": "received", "id": "0->1", "delay": 0.05, "deliver": true}
                if msg.get("type") == "received":
                    msg_id = msg.get("id", "")
                    delay = float(msg.get("delay", 0.0))
                    # Aggiorniamo il ritardo noto per questa coppia
                    if msg_id:
                        self.latest_delays[msg_id] = delay
            except json.JSONDecodeError:
                pass

    def update_position(self, agent_id: int, x: float, y: float, z: float):
        """Invia un comando di aggiornamento posizione a OMNeT++."""
        if not self.enabled or not self._sock:
            return

        payload = {
            "type": "move",
            "id": str(agent_id),
            "x": str(x),
            "y": str(y),
            "z": str(z)
        }
        
        try:
            self._send_json(payload)
        except OSError:
            pass

    def transmit(
        self,
        *,
        topic: str,
        sender: int,
        receiver: int,
        size_bytes: int,
        metadata: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        if not self.enabled or not self._sock:
            return {"deliver": True, "delay_s": self.default_delay}

        # ID univoco per la coppia (usato per tracciare il ritardo)
        msg_id = f"{sender}->{receiver}"

        # Costruisce il comando "send" per OMNeT++
        # Protocollo: { "type": "send", "src": "0", "dst": "1", "size": 1000, "id": "0->1" }
        payload = {
            "type": "send",
            "src": str(sender),
            "dst": str(receiver),
            "size": str(size_bytes),
            "id": msg_id
        }

        try:
            self._send_json(payload)
        except OSError as exc:
            print(
                f"[OmnetBridge] connection issue ({exc}); fail_open={self.fail_open}",
                file=sys.stderr,
            )
            self._close()
            if self.fail_open:
                self.enabled = False
                return {"deliver": True, "delay_s": self.default_delay}
            return {"deliver": False, "delay_s": 0.0}

        # Ritorna l'ultimo ritardo conosciuto per questa coppia
        # Se non abbiamo ancora ricevuto nulla, usiamo default_delay
        if msg_id in self.latest_delays:
            delay = self.latest_delays[msg_id]
            return {"deliver": True, "delay_s": max(0.0, delay)}
        else:
            # Mai ricevuto nulla finora
            return {"deliver": True, "delay_s": self.default_delay}

    def close(self) -> None:
        self._close()

    # ------------------------------------------------------------------
    def _connect(self) -> None:
        try:
            sock = socket.create_connection((self.host, self.port), timeout=self.timeout)
            # Imposta il socket come non bloccante
            sock.setblocking(False)
            self._sock = sock
            self._buffer = ""
        except OSError as exc:
            print(
                f"[OmnetBridge] unable to connect to {self.host}:{self.port} ({exc})",
                file=sys.stderr,
            )
            if not self.fail_open:
                raise
            self.enabled = False

    def _send_json(self, msg: Dict[str, Any]) -> None:
        if not self._sock:
            raise ConnectionError("bridge socket unavailable")
        data = json.dumps(msg, separators=(",", ":")) + "\n"
        # sendall potrebbe bloccare se il buffer è pieno, ma con piccoli JSON è raro.
        # In modalità non bloccante, dovremmo gestire i byte inviati.
        # Per semplicità usiamo sendall che su socket non bloccanti può lanciare errore se buffer pieno.
        try:
            self._sock.sendall(data.encode("utf-8"))
        except BlockingIOError:
            # Buffer pieno, droppiamo il pacchetto o riproviamo?
            # Per questa simulazione, ignoriamo (best effort)
            pass

    def _close(self) -> None:
        if self._sock:
            try:
                self._sock.close()
            except OSError:
                pass
            self._sock = None

    def __del__(self) -> None:
        self._close()


def patch_feature_transformation(
    bridge: Optional[OmnetBridge],
    dataset_framerate: float = 5.0,
    phases: Optional[List[NetworkPhase]] = None,
    rng_seed: int = 123,
):
    # Se non c'è un bridge attivo, non fare nulla (nessuna patch)
    if bridge is None:
        return (lambda: None), NetworkRuntimeStats()

    stats = NetworkRuntimeStats()
    phases = phases or [NetworkPhase("steady", frames=10**9, delay_floor_s=0.0, drop_prob=0.0)]
    frame_counter = 0
    rng = random.Random(rng_seed)

    # Salva il metodo originale per poterlo chiamare o ripristinare dopo
    descriptor = DetModelBase.__dict__["feature_transformation"]

    # Estrae la funzione originale (gestisce staticmethod)
    original = descriptor.__func__ if isinstance(descriptor, staticmethod) else descriptor

    # Buffer per memorizzare la storia delle feature map
    feature_buffer: Dict[str, list] = {}
    
    # Lunghezza massima del buffer (in secondi simulati). 
    MAX_BUFFER_SEC = 2.0
    MAX_BUFFER_LEN = int(MAX_BUFFER_SEC * dataset_framerate)

    def _extract_position(tm_tensor, b, agent_id):
        """Extracts absolute position (x, y, z) of agent_id relative to agent 0 (ego).
        Assumes agent 0 is at (0,0,0) in the simulation world.
        """
        if tm_tensor is None:
            return (0.0, 0.0, 0.0)

        try:
            tm = tm_tensor.detach().cpu().numpy()
        except Exception:
            return (0.0, 0.0, 0.0)

        if tm.ndim == 6:
            tm = tm[:, 0]

        if tm.ndim == 5:
            try:
                # tm[b, 0, agent_id] is transform FROM agent_id TO agent 0
                # Wait, usually T_i_j means "Pose of j in i's frame".
                # So T[0, j] gives j's coordinates in 0's frame.
                m = tm[int(b), 0, int(agent_id)]
                if m.shape == (4, 4):
                    # Translation vector
                    return (float(m[0, 3]), float(m[1, 3]), float(m[2, 3]))
            except Exception:
                pass
        return (0.0, 0.0, 0.0)

    def wrapped(b, j, agent_idx, local_com_mat, all_warp, device, size, trans_matrices):
        """
        Intercetta il processo di trasformazione delle feature per iniettare una simulazione di rete realistica.

        ogni qual volta viene chiamata la funzione DetModelBase.feature_transformation viene prima chiamato questo wrapper.
        !! La funzione originale viene usata iterando su tutti gli agenti a parte il corrente (j) per creare una
        lista di features con coordinate trasformate nel sistema di riferimento dell'agente corrente.
        Questa lista di features viene poi fusa con la funzione di fusione specifica del modello.

        Invece di permettere la condivisione istantanea dei dati tra agenti, esegue i seguenti passaggi:
        1.  **Sincronizzazione**: Legge gli aggiornamenti asincroni dei ritardi da OMNeT++ e invia le posizioni correnti degli agenti.
        2.  **Buffering**: Memorizza la feature map corrente in un buffer storico per permettere il recupero di dati passati.
        3.  **Simulazione Rete**: Invia i metadati del pacchetto (mittente, destinatario, dimensione) a OMNeT++ per determinare stato di consegna e ritardo.
        4.  **Applicazione Ritardo **: Calcola quanto è vecchio il dato in base al ritardo di rete.
            - Se ritardo 0s -> Usa il dato corrente (Live).
            - Se ritardo 0.4s (@5Hz) -> Recupera il dato di 2 frame fa dal buffer.
            - Se pacchetto perso -> Sostituisce i dati con zeri.
        5.  **Esecuzione**: Chiama la trasformazione originale con i dati modificati (ritardati o persi).
        """
        
        nonlocal frame_counter

        # Aggiorna lo stato della rete (legge eventuali messaggi in arrivo) e aggiorna 
        # il buffer dei latest_delays con timestamp
        bridge.update_state()

        # Identifichiamo il frame in modo robusto: incrementa una volta per agente 0 nel batch 0.
        # Nota: DetModelBase salta i self-loop (j != agent_idx), quindi il caso j=0, agent_idx=0 non avviene mai.
        # Usiamo (j=1, agent_idx=0) come trigger per contare il frame, assumendo almeno 2 agenti.
        if int(b) == 0 and int(agent_idx) == 0 and int(j) == 1:
            frame_counter += 1
        current_phase = _phase_for_frame(phases, frame_counter)
        
        
        # Send position updates for sender (j) and receiver (agent_idx) to OMNeT++
        pos_j = _extract_position(trans_matrices, b, j)
        pos_i = _extract_position(trans_matrices, b, agent_idx)
        
        bridge.update_position(int(j), pos_j[0], pos_j[1], pos_j[2])
        bridge.update_position(int(agent_idx), pos_i[0], pos_i[1], pos_i[2])
        

        # Estrae il tensore (feature map) che l'agente j vuole inviare all'agente agent_idx
        current_payload = local_com_mat[b, j]
        
        # --- GESTIONE BUFFER ---
        # Chiave univoca per identificare la coda di questo specifico agente in questo batch
        buffer_key = f"b{b}_ag{j}"
        
        if buffer_key not in feature_buffer:
            feature_buffer[buffer_key] = []
        
        # Aggiungi il payload corrente alla testa della lista (il più recente è l'ultimo)
        # Cloniamo il tensore per evitare che venga sovrascritto in-place da operazioni successive
        feature_buffer[buffer_key].append(current_payload.clone().detach())
        
        # Mantieni il buffer di dimensione fissa
        if len(feature_buffer[buffer_key]) > MAX_BUFFER_LEN:
            feature_buffer[buffer_key].pop(0) # Rimuovi il più vecchio
            
        # -----------------------

        # Prepara i metadati per la richiesta al simulatore
        meta = {
            "batch": int(b),
            "sender": int(j),
            "receiver": int(agent_idx),
            "shape": list(current_payload.shape),
            "dtype": str(current_payload.dtype),
        }
            
        # Chiede al bridge OMNeT++ se il pacchetto può essere consegnato e con che ritardo basandosi sulle
        # precedenti condizioni di rete, inoltre invia il pacchetto corrente a omnet
        decision = bridge.transmit(
            topic="feature_tensor",  # Nota: in det è feature_tensor
            sender=int(j),
            receiver=int(agent_idx),
            size_bytes=int(current_payload.element_size() * current_payload.numel()), # Calcola dimensione in byte
            metadata=meta,
        )

        is_delivered = decision.get("deliver", True)
        sim_delay = decision.get("delay_s", 0.0)

        # Overlay time-varying profile: emula periodi di down/recovery sopra la latenza OMNeT++.
        if rng.random() < current_phase.drop_prob:
            is_delivered = False
            sim_delay = 0.0
        else:
            sim_delay = max(float(sim_delay), current_phase.delay_floor_s)

        # Calcolo del frame lag: Quanti frame indietro nel tempo dobbiamo andare
        # Lag = Ritardo (s) * Framerate (Hz)
        frames_lag = int(round(sim_delay * dataset_framerate))
        
        final_payload = None
        log_msg = ""

        underflow = False
        if not is_delivered:
            # Caso 1: Pacchetto perso -> Zeri
            final_payload = torch.zeros_like(current_payload)
            log_msg = f"[XXX] {j} -> {agent_idx} | PACKET LOST | phase={current_phase.name}"
        elif frames_lag == 0:
            # Caso 2: Ritardo trascurabile -> Usa dato corrente
            final_payload = current_payload
            log_msg = f"[OK]  {j} -> {agent_idx} | Delay: {sim_delay:.3f}s (Live) | phase={current_phase.name}"
        else:
            # Caso 3: Ritardo significativo -> Recupera dal buffer
            buffer = feature_buffer[buffer_key]
            # L'indice -1 è il corrente (lag 0), -2 è lag 1, ecc.
            # Quindi index = -1 - frames_lag
            target_idx = -1 - frames_lag
            
            # Controlla se abbiamo abbastanza storia
            if abs(target_idx) <= len(buffer):
                final_payload = buffer[target_idx]
                log_msg = f"[OLD] {j} -> {agent_idx} | Delay: {sim_delay:.3f}s -> Lag: {frames_lag} frames | phase={current_phase.name}"
            else:
                # Qui scegliamo di usare il più vecchio disponibile
                final_payload = buffer[0] 
                underflow = True
                log_msg = f"[OLD!] {j} -> {agent_idx} | Delay: {sim_delay:.3f}s -> Lag: {frames_lag} frames (Buffer Underflow, using oldest) | phase={current_phase.name}"

        print(log_msg)

        stats.record(
            delivered=bool(is_delivered),
            delay_s=float(sim_delay),
            frames_lag=int(frames_lag),
            stale=bool(is_delivered and frames_lag > 0),
            underflow=bool(underflow),
            phase_name=current_phase.name,
        )

        # Creiamo una copia della matrice di comunicazione per non sporcare quella vera per altri agenti
        # Passiamo una local_com_mat con il payload modificato solo nella posizione [b, j]
        local_com_mat_patched = local_com_mat.clone()
        local_com_mat_patched[b, j] = final_payload
        
        # Chiama la funzione originale con la matrice modificata
        return original(b, j, agent_idx, local_com_mat_patched, all_warp, device, size, trans_matrices)

    # Sostituisce il metodo statico originale con la versione wrappata (Monkey Patching)
    DetModelBase.feature_transformation = staticmethod(wrapped)

    def restore():
        # Funzione per ripristinare il metodo originale alla fine
        DetModelBase.feature_transformation = descriptor

    return restore, stats


def build_parser():
    parser = argparse.ArgumentParser(
        description="Inference runner with optional OMNeT++ coupling"
    )
    parser.add_argument(
        "-d",
        "--data",
        default=None,
        type=str,
        help="Path to the preprocessed BEV data",
    )
    parser.add_argument("--nepoch", default=100, type=int, help="Number of epochs")
    parser.add_argument("--nworker", default=1, type=int, help="Number of workers")
    parser.add_argument("--lr", default=0.001, type=float, help="Initial learning rate")
    parser.add_argument("--log", action="store_true", help="Whether to log")
    parser.add_argument("--logpath", default="", help="Output log directory")
    parser.add_argument(
        "--resume",
        default="",
        type=str,
        help="Path to the saved checkpoint",
    )
    parser.add_argument(
        "--resume_teacher",
        default="",
        type=str,
        help="Path to the teacher checkpoint (DiscoNet)",
    )
    parser.add_argument(
        "--layer",
        default=3,
        type=int,
        help="Layer index for communication",
    )
    parser.add_argument(
        "--warp_flag", default=0, type=int, help="Use pose info for When2com"
    )
    parser.add_argument(
        "--kd_flag",
        default=0,
        type=int,
        help="Enable knowledge distillation",
    )
    parser.add_argument("--kd_weight", default=100000, type=int, help="KD loss weight")
    parser.add_argument(
        "--gnn_iter_times",
        default=3,
        type=int,
        help="Message passing iterations for V2VNet",
    )
    parser.add_argument(
        "--visualization", type=int, default=0, help="Enable validation visualization"
    )
    parser.add_argument(
        "--com",
        default="",
        type=str,
        help="Detector to evaluate",
    )
    parser.add_argument("--inference", type=str, help="Inference rule for when2com")
    parser.add_argument("--tracking", action="store_true")
    parser.add_argument("--box_com", action="store_true")
    parser.add_argument("--rsu", default=0, type=int, help="0: no RSU, 1: RSU")
    parser.add_argument(
        "--num_agent", default=6, type=int, help="Total number of agents"
    )
    parser.add_argument(
        "--apply_late_fusion",
        default=0,
        type=int,
        help="1: apply late fusion. 0: no late fusion",
    )
    parser.add_argument(
        "--compress_level",
        default=0,
        type=int,
        help="Encoder channel compression level",
    )
    parser.add_argument(
        "--pose_noise",
        default=0,
        type=float,
        help="Pose noise magnitude in meters",
    )
    parser.add_argument(
        "--only_v2i",
        default=0,
        type=int,
        help="1: only v2i, 0: v2v and v2i",
    )
    parser.add_argument(
        "--scene_id",
        default=None,
        type=int,
        help="Specify a single scene ID to run (e.g. 29)",
    )

    # ------------------------------------------------------------------
    # Network coupling options 
    # ------------------------------------------------------------------
    parser.add_argument(
        "--network_host",
        default="127.0.0.1",
        type=str,
        help="Hostname of the OMNeT++ bridge",
    )
    parser.add_argument(
        "--network_port", default=5555, type=int, help="Port of the OMNeT++ bridge"
    )
    parser.add_argument(
        "--network_timeout",
        default=2.0,
        type=float,
        help="Socket timeout for the bridge (s)",
    )
    parser.add_argument(
        "--network_default_delay",
        default=0.0,
        type=float,
        help="Fallback latency in seconds when the bridge is unavailable",
    )
    parser.add_argument(
        "--network_fail_open",
        action="store_true",
        help="If set, continue with zero/drop decisions when the bridge disconnects",
    )
    parser.add_argument(
        "--network_disable",
        action="store_true",
        help="Run baseline inference without OMNeT++ coupling",
    )
    parser.add_argument(
        "--dataset_framerate",
        default=5.0,
        type=float,
        help="Framerate of the dataset in Hz (default: 5.0). Used to calculate frame lag from network delay.",
    )
    parser.add_argument(
        "--network_profile",
        default="steady:1000000:0.00:0.00",
        type=str,
        help=(
            "Time-varying network profile as comma-separated phases: "
            "name:frames:delay_floor_s:drop_prob "
            "(example: good:120:0.02:0.00,down:60:0.40:0.15,recover:120:0.08:0.02)."
        ),
    )
    parser.add_argument(
        "--network_profile_seed",
        default=123,
        type=int,
        help="Random seed for phase drop emulation (default: 123)",
    )
    parser.add_argument(
        "--summary_out",
        default="",
        type=str,
        help="Optional path to a JSON summary report for run-to-run comparison",
    )

    # ------------------------------------------------------------------
    # Konro integration options
    # ------------------------------------------------------------------
    parser.add_argument(
        "--konro_enable",
        action="store_true",
        help="Enable Konro resource manager feedback",
    )
    parser.add_argument(
        "--konro_target",
        default=0.85,
        type=float,
        help="Target quality for the proxy metric (default: 0.85)",
    )
    parser.add_argument(
        "--konro_ema_alpha",
        default=0.2,
        type=float,
        help="EMA smoothing factor for proxy metric (default: 0.2)",
    )
    parser.add_argument(
        "--konro_interval",
        default=1,
        type=int,
        help="Send feedback to Konro every N frames (default: 1)",
    )
    parser.add_argument(
        "--konro_iou_thr",
        default=0.5,
        type=float,
        help="IoU threshold for per-frame TP counting (default: 0.5)",
    )
    parser.add_argument(
        "--konro_agent_id",
        default=1,
        type=int,
        help="Agent ID to monitor for Konro feedback (default: 1)",
    )

    return parser


def patch_cal_local_mAP(
    tracker: PerceptionProxyTracker,
    iou_thr: float = 0.5,
    target_agent_id: int = 1,
    rsu: int = 0,
    num_agent_total: int = 6,
):
    """Monkey-patch cal_local_mAP to feed Konro tracker for one selected agent only."""
    # Get the original function from test_codet's namespace
    original_cal = test_codet.cal_local_mAP

    # test_codet evaluates agents in deterministic order.
    # If RSU is disabled, evaluation index k=0 corresponds to original agent 1.
    eval_indices = list(range(1, num_agent_total)) if rsu else list(range(num_agent_total - 1))
    if not eval_indices:
        eval_indices = [0]

    target_eval_idx = target_agent_id if rsu else (target_agent_id - 1)
    if target_eval_idx not in eval_indices:
        raise ValueError(
            f"Invalid --konro_agent_id={target_agent_id} for num_agent={num_agent_total}, rsu={rsu}"
        )

    call_count = 0

    def wrapped(config, data, det_results, annotations):
        nonlocal call_count
        current_eval_idx = eval_indices[call_count % len(eval_indices)]
        call_count += 1

        # Compute stats only for the selected agent and skip empty samples.
        if current_eval_idx == target_eval_idx:
            num_gts, num_dets, num_tp = cal_frame_stats(config, data, iou_thr=iou_thr)
            if not (num_gts == 0 and num_dets == 0):
                tracker.update(num_gts, num_dets, num_tp)
            else:
                print(
                    f"[Proxy] skipped empty step for agent {target_agent_id} "
                    f"(gts={num_gts}, dets={num_dets})"
                )

        # Call the original accumulation function
        return original_cal(config, data, det_results, annotations)

    # Patch in test_codet's module namespace (that's where it's looked up at call site)
    test_codet.cal_local_mAP = wrapped

    def restore():
        test_codet.cal_local_mAP = original_cal

    return restore


def main():
    parser = build_parser()
    args = parser.parse_args()

    bridge = None
    restore_hook = lambda: None
    restore_konro_hook = lambda: None
    tracker = None
    network_stats = NetworkRuntimeStats()
    run_successful = False

    try:
        phases = _parse_network_profile(args.network_profile)

        # Inizializza il bridge solo se non è disabilitato da riga di comando
        if not args.network_disable:
            bridge = OmnetBridge(
                host=args.network_host,
                port=args.network_port,
                timeout=args.network_timeout,
                default_delay=args.network_default_delay,
                fail_open=args.network_fail_open,
                enabled=True,
            )
        
        # Applica la patch al modello per intercettare le comunicazioni
        restore_hook, network_stats = patch_feature_transformation(
            bridge, 
            dataset_framerate=args.dataset_framerate,
            phases=phases,
            rng_seed=args.network_profile_seed,
        )

        # Inizializza il tracker Konro e patch cal_local_mAP
        tracker = PerceptionProxyTracker(
            target_quality=args.konro_target,
            ema_alpha=args.konro_ema_alpha,
            feedback_interval=args.konro_interval,
            konro_enabled=args.konro_enable,
        )
        if args.konro_enable:
            tracker.register()
        restore_konro_hook = patch_cal_local_mAP(
            tracker,
            iou_thr=args.konro_iou_thr,
            target_agent_id=args.konro_agent_id,
            rsu=args.rsu,
            num_agent_total=args.num_agent,
        )
        
        # Imposta la strategia di sharing per multiprocessing (necessario per PyTorch con molti dati)
        torch.multiprocessing.set_sharing_strategy("file_system")
        print(args)
        
        # Esegue il test originale
        test_codet.main(args)
        run_successful = True
    except Exception:
        import traceback
        traceback.print_exc()
        run_successful = False
    finally:
        # Assicura che la patch venga rimossa e il bridge chiuso anche in caso di errore
        restore_konro_hook()
        restore_hook()
        network_summary = network_stats.to_summary()
        proxy_summary = tracker.summary() if tracker is not None else {}

        print("\n=== Simulation Summary ===")
        print(
            "[Network] "
            f"tx={network_summary.get('tx_total', 0)} "
            f"delivery={network_summary.get('delivery_ratio', 0.0):.3f} "
            f"drop={network_summary.get('drop_ratio', 0.0):.3f} "
            f"avg={network_summary.get('latency_avg_s', 0.0):.3f}s "
            f"p95={network_summary.get('latency_p95_s', 0.0):.3f}s "
            f"stale={network_summary.get('stale_packets', 0)}"
        )
        if tracker is not None:
            print(f"\n[Konro] Final proxy EMA: {tracker.current_ema:.4f} over {tracker.frame_count} frames")
            print(
                "[Proxy] "
                f"mean={proxy_summary.get('proxy_mean', 0.0):.4f} "
                f"ema={proxy_summary.get('proxy_ema', 0.0):.4f} "
                f"below_target_ratio={proxy_summary.get('below_target_ratio', 0.0):.3f} "
                f"feedback_events={proxy_summary.get('feedback_events', 0)}"
            )

        if args.summary_out:
            # Scrivi il JSON solo se la run ha avuto successo o se abbiamo comunque dati parziali significativi
            has_data = network_summary.get('tx_total', 0) > 0
            
            if run_successful or has_data:
                payload = {
                    "run": {
                        "konro_enable": bool(args.konro_enable),
                        "network_disable": bool(args.network_disable),
                        "network_profile": args.network_profile,
                        "network_profile_seed": args.network_profile_seed,
                        "dataset_framerate": args.dataset_framerate,
                    },
                    "network": network_summary,
                    "proxy": proxy_summary,
                }
                out_dir = os.path.dirname(args.summary_out)
                if out_dir:
                    os.makedirs(out_dir, exist_ok=True)
                with open(args.summary_out, "w", encoding="utf-8") as f:
                    json.dump(payload, f, indent=2)
                print(f"[Summary] Report written to {args.summary_out}")
            else:
                print(f"[Summary] Test FAILED early. Skipping JSON report (empty) to avoid confusion -> {args.summary_out}")

        if bridge is not None:
            bridge.close()


if __name__ == "__main__":
    main()
