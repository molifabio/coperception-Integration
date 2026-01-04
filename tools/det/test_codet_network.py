import argparse
import json
import socket
import sys
import time
from typing import Any, Dict, Optional

import torch # type: ignore

import test_codet
from coperception.models.det.base.DetModelBase import DetModelBase


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
        # Cache per i ritardi: Key="sender->receiver", Value=(delay_s, timestamp_received)
        self.latest_delays: Dict[str, tuple[float, float]] = {}
        
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
                    # Aggiorniamo il ritardo noto per questa coppia con il timestamp corrente
                    if msg_id:
                        self.latest_delays[msg_id] = (delay, time.time())
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
        # Implementiamo un timeout: se non riceviamo aggiornamenti da > 2.0s, consideriamo il link perso
        # nelle nostre simulazione inoltre non succede mai che un veicolo non trasmetta per 
        # più di un secondo, ma se ciò dovesse accadere, consideriamo il link scaduto,
        # il primo pacchetto verrà considerato perso e si riprenderà alla ricezione del successivo.
        LINK_TIMEOUT = 2.0
        
        if msg_id in self.latest_delays:
            delay, last_time = self.latest_delays[msg_id]
            if time.time() - last_time > LINK_TIMEOUT:
                # Link scaduto (Packet Loss simulato per timeout)
                return {"deliver": False, "delay_s": 0.0}
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


def patch_feature_transformation(bridge: Optional[OmnetBridge], dataset_framerate: float = 5.0):
    # Se non c'è un bridge attivo, non fare nulla (nessuna patch)
    if bridge is None:
        return lambda: None

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
        
        # Aggiorna lo stato della rete (legge eventuali messaggi in arrivo) e aggiorna 
        # il buffer dei latest_delays con timestamp
        bridge.update_state()
        
        
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

        # Calcolo del frame lag: Quanti frame indietro nel tempo dobbiamo andare
        # Lag = Ritardo (s) * Framerate (Hz)
        frames_lag = int(round(sim_delay * dataset_framerate))
        
        final_payload = None
        log_msg = ""

        if not is_delivered:
            # Caso 1: Pacchetto perso -> Zeri
            final_payload = torch.zeros_like(current_payload)
            log_msg = f"[XXX] {j} -> {agent_idx} | PACKET LOST"
        elif frames_lag == 0:
            # Caso 2: Ritardo trascurabile -> Usa dato corrente
            final_payload = current_payload
            log_msg = f"[OK]  {j} -> {agent_idx} | Delay: {sim_delay:.3f}s (Live)"
        else:
            # Caso 3: Ritardo significativo -> Recupera dal buffer
            buffer = feature_buffer[buffer_key]
            # L'indice -1 è il corrente (lag 0), -2 è lag 1, ecc.
            # Quindi index = -1 - frames_lag
            target_idx = -1 - frames_lag
            
            # Controlla se abbiamo abbastanza storia
            if abs(target_idx) <= len(buffer):
                final_payload = buffer[target_idx]
                log_msg = f"[OLD] {j} -> {agent_idx} | Delay: {sim_delay:.3f}s -> Lag: {frames_lag} frames"
            else:
                # Qui scegliamo di usare il più vecchio disponibile
                final_payload = buffer[0] 
                log_msg = f"[OLD!] {j} -> {agent_idx} | Delay: {sim_delay:.3f}s -> Lag: {frames_lag} frames (Buffer Underflow, using oldest)"

        print(log_msg)

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

    return restore


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

    return parser


def main():
    parser = build_parser()
    args = parser.parse_args()

    bridge = None
    restore_hook = lambda: None

    try:
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
        restore_hook = patch_feature_transformation(
            bridge, 
            dataset_framerate=args.dataset_framerate
        )
        
        # Imposta la strategia di sharing per multiprocessing (necessario per PyTorch con molti dati)
        torch.multiprocessing.set_sharing_strategy("file_system")
        print(args)
        
        # Esegue il test originale
        test_codet.main(args)
    finally:
        # Assicura che la patch venga rimossa e il bridge chiuso anche in caso di errore
        restore_hook()
        if bridge is not None:
            bridge.close()


if __name__ == "__main__":
    main()
