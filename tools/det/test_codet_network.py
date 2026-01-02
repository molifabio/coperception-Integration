import argparse
import json
import socket
import sys
import time
from typing import Any, Dict, Optional

import torch # type: ignore
import torch.nn.functional as F # type: ignore

import test_codet
from coperception.models.det.base.DetModelBase import DetModelBase


class OmnetBridge:
    """Thin TCP client that exchanges JSON events with an OMNeT++ server."""

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
        self._stream = None
        if self.enabled:
            self._connect()

    # ------------------------------------------------------------------
    def transmit(
        self,
        *,
        topic: str,
        sender: int,
        receiver: int,
        size_bytes: int,
        metadata: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        # Se il bridge è disabilitato o non connesso, ritorna successo immediato (comportamento di default)
        if not self.enabled or self._stream is None:
            return {"deliver": True, "delay_s": self.default_delay}

        # Costruisce il payload JSON da inviare al simulatore OMNeT++
        payload = {
            "topic": topic,
            "sender": sender,
            "receiver": receiver,
            "size_bytes": size_bytes,
            "timestamp": time.time(),
            "metadata": metadata or {},
        }

        try:
            # Invia la richiesta e attende la risposta sincrona
            self._send_json(payload)
            reply = self._recv_json()
        except OSError as exc:
            # Gestione errori di connessione
            print(
                f"[OmnetBridge] connection issue ({exc}); fail_open={self.fail_open}",
                file=sys.stderr,
            )
            self._close()
            # Se fail_open è True, continua a funzionare ignorando il simulatore
            if self.fail_open:
                self.enabled = False
                return {"deliver": True, "delay_s": self.default_delay}
            # Altrimenti, simula un fallimento della consegna
            return {"deliver": False, "delay_s": 0.0}

        # Estrae la decisione dal simulatore: consegnare o no? quanto ritardo?
        deliver = bool(reply.get("deliver", True))
        delay_s = float(reply.get("delay_s", reply.get("delay", self.default_delay)))
        return {"deliver": deliver, "delay_s": max(0.0, delay_s)}

    def close(self) -> None:
        self._close()

    # ------------------------------------------------------------------
    def _connect(self) -> None:
        try:
            sock = socket.create_connection((self.host, self.port), timeout=self.timeout)
            sock.settimeout(self.timeout)
            self._sock = sock
            self._stream = sock.makefile("rwb")
        except OSError as exc:
            print(
                f"[OmnetBridge] unable to connect to {self.host}:{self.port} ({exc})",
                file=sys.stderr,
            )
            if not self.fail_open:
                raise
            self.enabled = False

    def _send_json(self, msg: Dict[str, Any]) -> None:
        if not self._stream:
            raise ConnectionError("bridge stream unavailable")
        data = json.dumps(msg, separators=(",", ":")) + "\n"
        self._stream.write(data.encode("utf-8"))
        self._stream.flush()

    def _recv_json(self) -> Dict[str, Any]:
        if not self._stream:
            raise ConnectionError("bridge stream unavailable")
        line = self._stream.readline()
        if line == b"":
            raise ConnectionAbortedError("bridge connection closed")
        return json.loads(line.decode("utf-8"))

    def _close(self) -> None:
        if self._stream:
            try:
                self._stream.close()
            except OSError:
                pass
            self._stream = None
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
    original = descriptor.__func__ if isinstance(descriptor, staticmethod) else descriptor

    # Buffer per memorizzare la storia delle feature map: { (batch_idx, agent_id): [payload_t0, payload_t1, ...] }
    # Usiamo una lista semplice come coda.
    feature_buffer: Dict[str, list] = {}
    # Tracker per evitare duplicati nello stesso step temporale
    update_tracker: Dict[str, dict] = {}
    
    # Lunghezza massima del buffer (in secondi simulati). 
    # Es. 2 secondi di storia a 5Hz = 10 frame.
    # Questo buffer serve a simulare la "memoria" dei dati passati.
    # Se la rete ha un ritardo di 0.4s, dobbiamo poter recuperare il dato generato 0.4s fa.
    MAX_BUFFER_SEC = 2.0
    MAX_BUFFER_LEN = int(MAX_BUFFER_SEC * dataset_framerate)

    def _extract_distance_m(tm_tensor, b, sender, receiver):
        """ extraction of relative distance from trans_matrices.

        trans_matrices is typically shaped (B, N, N, 4, 4).
        However, test_codet.py might stack them into (B, K, N, N, 4, 4).
        We handle both cases to extract the translation vector norm.
        """

        if tm_tensor is None:
            return None

        try:
            # Ensure CPU numpy for math. Sposta il tensore su CPU e converte in numpy.
            tm = tm_tensor.detach().cpu().numpy()
        except Exception:
            return None

        # Handle 6D case: (B, K, N, N, 4, 4) -> reduce to (B, N, N, 4, 4)
        # In alcuni casi test_codet aggiunge una dimensione extra K (es. per teacher/student), prendiamo il primo.
        if tm.ndim == 6:
            tm = tm[:, 0]

        # Handle 5D case: (B, N, N, 4, 4)
        if tm.ndim == 5:
            try:
                # tm[b, receiver, sender] is the transform FROM sender TO receiver
                # La matrice di trasformazione contiene la rotazione e la traslazione relativa.
                # Indiciamo con [batch, receiver, sender] per ottenere la posa di sender vista da receiver.
                m = tm[int(b), int(receiver), int(sender)]
                if m.shape == (4, 4):
                    # Estrae il vettore di traslazione (x, y, z) dalla 4a colonna
                    t = m[:3, 3]
                    # Calcola la distanza euclidea (norma L2)
                    return float((t[0] ** 2 + t[1] ** 2 + t[2] ** 2) ** 0.5)
            except Exception:
                pass

        return None

    def wrapped(b, j, agent_idx, local_com_mat, all_warp, device, size, trans_matrices):
        # Estrae il payload (feature map) che l'agente j vuole inviare all'agente agent_idx
        current_payload = local_com_mat[b, j]
        
        # --- GESTIONE BUFFER ---
        # Chiave univoca per identificare la coda di questo specifico agente in questo batch
        # Nota: 'b' è l'indice nel batch corrente, non un ID globale, ma va bene perché il buffer si svuota/riempie sequenzialmente
        # Tuttavia, per sicurezza in test multi-epoch, sarebbe meglio pulire il buffer, ma qui assumiamo inferenza sequenziale.
        buffer_key = f"b{b}_ag{j}"
        
        # Logic to avoid duplicate inserts for the same timestep
        # We use the tensor ID and a checksum to identify unique frames
        mat_id = id(local_com_mat)
        curr_checksum = float(current_payload.sum().item()) # Simple checksum
        
        should_append = True
        if buffer_key in update_tracker:
            last_info = update_tracker[buffer_key]
            # If same tensor object AND same content checksum -> It's a duplicate call for the same step
            if last_info['mat_id'] == mat_id and abs(last_info['checksum'] - curr_checksum) < 1e-4:
                should_append = False
        
        if should_append:
            if buffer_key not in feature_buffer:
                feature_buffer[buffer_key] = []
            
            # OPTIMIZATION: Move to CPU to save VRAM
            feature_buffer[buffer_key].append(current_payload.clone().detach().cpu())
            
            # Update tracker
            update_tracker[buffer_key] = {'mat_id': mat_id, 'checksum': curr_checksum}
            
            # Mantieni il buffer di dimensione fissa
            if len(feature_buffer[buffer_key]) > MAX_BUFFER_LEN:
                feature_buffer[buffer_key].pop(0) # Rimuovi il più vecchio
            
        # -----------------------

        # Calcola la distanza fisica tra i due agenti per passarla al simulatore
        distance_m = _extract_distance_m(trans_matrices, b, j, agent_idx)
        
        # Prepara i metadati per la richiesta al simulatore
        meta = {
            "batch": int(b),
            "sender": int(j),
            "receiver": int(agent_idx),
            "shape": list(current_payload.shape),
            "dtype": str(current_payload.dtype),
        }
        if distance_m is not None:
            meta["distance_m"] = float(distance_m)
            
        # Chiede al bridge OMNeT++ se il pacchetto può essere consegnato e con che ritardo
        decision = bridge.transmit(
            topic="feature_tensor",  # Nota: in det è feature_tensor
            sender=int(j),
            receiver=int(agent_idx),
            size_bytes=int(current_payload.element_size() * current_payload.numel()), # Calcola dimensione in byte
            metadata=meta,
        )

        is_delivered = decision.get("deliver", True)
        sim_delay = decision.get("delay_s", 0.0)

        # Calcolo del frame lag
        # Quanti frame indietro nel tempo dobbiamo andare?
        # Lag = Ritardo (s) * Framerate (Hz)
        # Esempio: Ritardo 0.4s * 5Hz = 2 frame di lag.
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
                # OPTIMIZATION: Move back to device
                final_payload = buffer[target_idx].to(device)
                log_msg = f"[OLD] {j} -> {agent_idx} | Delay: {sim_delay:.3f}s -> Lag: {frames_lag} frames"
            else:
                # Ritardo eccessivo, buffer non sufficiente -> Consideriamo perso o usiamo il più vecchio
                # Qui scegliamo di usare il più vecchio disponibile (best effort)
                final_payload = buffer[0].to(device)
                log_msg = f"[OLD!] {j} -> {agent_idx} | Delay: {sim_delay:.3f}s -> Lag: {frames_lag} frames (Buffer Underflow, using oldest)"

        print(log_msg)

        # OPTIMIZATION: Inline feature_transformation logic to avoid cloning local_com_mat
        # We reimplement DetModelBase.feature_transformation here using final_payload directly.
        
        nb_agent = torch.unsqueeze(final_payload, 0)
        
        tfm_ji = trans_matrices[b, j, agent_idx]
        M = (
            torch.hstack((tfm_ji[:2, :2], -tfm_ji[:2, 3:4])).float().unsqueeze(0)
        )
        
        # Ensure mask is on the same device as M
        mask = torch.tensor([[[1, 1, 4 / 128], [1, 1, 4 / 128]]], device=M.device)
        M *= mask
        
        grid = F.affine_grid(M, size=torch.Size(size))
        warp_feat = F.grid_sample(nb_agent, grid).squeeze()
        
        return warp_feat

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

    # Network coupling options
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
