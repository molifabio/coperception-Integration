# Copyright (c) 2020 Mitsubishi Electric Research Laboratories (MERL). All rights reserved.
import argparse
import os
import sys
import time
import json
import socket
import cv2
import numpy as np
import matplotlib.pyplot as plt
import torch
import torch.nn.functional as F
import torch.optim as optim
from tqdm import tqdm
from torch.utils.data import DataLoader
from typing import Any, Dict, Optional

# Imports from coperception
from coperception.datasets import V2XSimSeg
from coperception.configs import Config
from coperception.utils.SegMetrics import ComputeIoU
from coperception.utils.SegModule import SegModule
from coperception.models.seg import *
# Importiamo specificamente la classe base per poterla modificare (patchare)
from coperception.models.seg import SegModelBase
from coperception.utils.data_util import apply_pose_noise


# ==============================================================================
# CLASS: OmnetBridge (Identica a quella usata in detection)
# ==============================================================================
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
    descriptor = SegModelBase.__dict__["feature_transformation"]
    original = descriptor.__func__ if isinstance(descriptor, staticmethod) else descriptor

    # Buffer per memorizzare la storia delle feature map: { (batch_idx, agent_id): [payload_t0, payload_t1, ...] }
    # Usiamo una lista semplice come coda.
    feature_buffer: Dict[str, list] = {}
    
    # Lunghezza massima del buffer (in secondi simulati). 
    # Es. 2 secondi di storia a 5Hz = 10 frame.
    # Questo buffer serve a simulare la "memoria" dei dati passati.
    # Se la rete ha un ritardo di 0.4s, dobbiamo poter recuperare il dato generato 0.4s fa.
    MAX_BUFFER_SEC = 2.0
    MAX_BUFFER_LEN = int(MAX_BUFFER_SEC * dataset_framerate)

    def _extract_distance_m(tm_tensor, b, sender, receiver):
        """Best-effort extraction of relative distance from trans_matrices.

        trans_matrices is typically shaped (B, N, N, 4, 4) with homogeneous transforms.
        We try both [b, receiver, sender] and [b, sender, receiver]; fall back to None.
        """

        if tm_tensor is None:
            return None

        try:
            # Ensure CPU numpy for math
            tm = tm_tensor.detach().cpu().numpy()
        except Exception:
            return None

        if tm.ndim < 3:
            return None

        candidates = []
        # Candidate order: receiver<-sender then sender<-receiver
        if tm.ndim >= 5:
            candidates.append((receiver, sender))
            candidates.append((sender, receiver))
            idx = [int(b)]
        elif tm.ndim == 4:
            # Possibly (N, N, 4, 4) with implicit batch 0
            candidates.append((receiver, sender))
            candidates.append((sender, receiver))
            idx = []
        else:
            return None

        for src, dst in candidates:
            try:
                m = tm[tuple(idx + [src, dst])]
                if m.shape[-2:] != (4, 4):
                    continue
                t = m[:3, 3]
                return float((t[0] ** 2 + t[1] ** 2 + t[2] ** 2) ** 0.5)
            except Exception:
                continue
        return None

    def wrapped(b, j, agent_idx, local_com_mat, size, trans_matrices):
        # Estrae il payload (feature map) che l'agente j vuole inviare all'agente agent_idx
        current_payload = local_com_mat[b, j]
        
        # --- GESTIONE BUFFER ---
        # Chiave univoca per identificare la coda di questo specifico agente in questo batch
        # Nota: 'b' è l'indice nel batch corrente, non un ID globale, ma va bene perché il buffer si svuota/riempie sequenzialmente
        # Tuttavia, per sicurezza in test multi-epoch, sarebbe meglio pulire il buffer, ma qui assumiamo inferenza sequenziale.
        buffer_key = f"b{b}_ag{j}"
        
        if buffer_key not in feature_buffer:
            feature_buffer[buffer_key] = []
        
        # Aggiungi il payload corrente alla testa della lista (il più recente è l'ultimo)
        # Cloniamo il tensore per evitare che venga sovrascritto in-place da operazioni successive
        # OPTIMIZATION: Move to CPU to save VRAM
        feature_buffer[buffer_key].append(current_payload.clone().detach().cpu())
        
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
            topic="feature_tensor_seg",  # Nota: in seg è feature_tensor_seg
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
        
        # Infer device from local_com_mat
        device = local_com_mat.device

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
        # We reimplement SegModelBase.feature_transformation here using final_payload directly.
        
        nb_agent = torch.unsqueeze(final_payload, 0)
        
        tfm_ji = trans_matrices[b, j, agent_idx]
        M = (
            torch.hstack((tfm_ji[:2, :2], -tfm_ji[:2, 3:4])).float().unsqueeze(0)
        )
        M = M.to(device)
        
        # Ensure mask is on the same device as M
        mask = torch.tensor([[[1, 1, 4 / 128], [1, 1, 4 / 128]]], device=M.device)
        M *= mask
        
        grid = F.affine_grid(M, size=torch.Size(size))
        warp_feat = F.grid_sample(nb_agent, grid).squeeze()
        return warp_feat

    # Applica la patch
    SegModelBase.feature_transformation = staticmethod(wrapped)

    # Funzione per ripristinare l'originale alla fine
    def restore():
        SegModelBase.feature_transformation = descriptor

    return restore


# ==============================================================================
# MAIN LOGIC
# ==============================================================================
def check_folder(folder_path):
    if not os.path.exists(folder_path):
        os.mkdir(folder_path)
    return folder_path

@torch.no_grad()
def run_test_logic(config, args):
    batch_size = args.batch_size
    num_workers = args.nworker
    logpath = args.logpath
    pose_noise = args.pose_noise
    compress_level = args.compress_level
    only_v2i = args.only_v2i
    
    config.nepoch = args.nepoch
    config.com = args.com
    config.inference = args.inference
    config.split = "test"

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    device_num = torch.cuda.device_count()
    print("device number", device_num)

    if args.com == "upperbound" or args.com == "lowerbound":
        flag = args.com
        config.com = None
    elif args.com == "when2com":
        flag = "when2com"
        if args.inference == "argmax_test":
            flag = "who2com"
        if args.warp_flag:
            flag = flag + "_warp"
    elif args.com in {"v2v", "disco", "sum", "mean", "max", "cat", "agent"}:
        flag = args.com
    else:
        raise ValueError(f"com: {args.com} is not supported")
    
    config.flag = flag

    num_agent = args.num_agent
    agent_idx_range = range(num_agent) if args.rsu else range(1, num_agent)
    valset = V2XSimSeg(
        dataset_roots=[args.data + "/agent%d" % i for i in agent_idx_range],
        config=config,
        split="val",
        val=True,
        com=args.com,
        bound="upperbound" if args.com == "upperbound" else "lowerbound",
        rsu=args.rsu,
    )
    valloader = DataLoader(
        valset, batch_size=batch_size, shuffle=False, num_workers=num_workers
    )
    print("Validation dataset size:", len(valset))

    # Aggiunto map_location='cpu' per evitare errore CUDA su macchine CPU
    checkpoint = torch.load(args.resume, map_location='cpu')

    # build model
    if not args.rsu:
        num_agent -= 1
    if args.com.startswith("when2com") or args.com.startswith("who2com"):
        model = When2Com_UNet(
            config,
            in_channels=config.in_channels,
            n_classes=config.num_class,
            warp_flag=args.warp_flag,
            num_agent=num_agent,
            compress_level=compress_level,
            only_v2i=only_v2i,
        )
    elif args.com == "v2v":
        model = V2VNet(
            config.in_channels,
            config.num_class,
            num_agent=num_agent,
            compress_level=compress_level,
            only_v2i=only_v2i,
        )
    elif args.com == "mean":
        model = MeanFusion(
            config.in_channels,
            config.num_class,
            num_agent=num_agent,
            compress_level=compress_level,
            only_v2i=only_v2i,
        )
    elif args.com == "max":
        model = MaxFusion(
            config.in_channels,
            config.num_class,
            num_agent=num_agent,
            compress_level=compress_level,
            only_v2i=only_v2i,
        )
    elif args.com == "sum":
        model = SumFusion(
            config.in_channels,
            config.num_class,
            num_agent=num_agent,
            compress_level=compress_level,
            only_v2i=only_v2i,
        )
    elif args.com == "cat":
        model = CatFusion(
            config.in_channels,
            config.num_class,
            num_agent=num_agent,
            compress_level=compress_level,
            only_v2i=only_v2i,
        )
    elif args.com == "agent":
        model = AgentWiseWeightedFusion(
            config.in_channels,
            config.num_class,
            num_agent=num_agent,
            compress_level=compress_level,
            only_v2i=only_v2i,
        )
    elif args.com == "disco":
        model = DiscoNet(
            config.in_channels,
            config.num_class,
            num_agent=num_agent,
            kd_flag=False,
            compress_level=compress_level,
            only_v2i=only_v2i,
        )
    elif args.com == "lowerbound" or args.com == "upperbound":
        model = UNet(
            config.in_channels,
            config.num_class,
            num_agent=num_agent,
            compress_level=compress_level,
        )
    
    model = model.to(device)
    optimizer = optim.Adam(model.parameters(), lr=args.lr)

    segmodule = SegModule(model, model, config, optimizer, False)
    segmodule.model.load_state_dict(checkpoint["model_state_dict"])
    
    # ==== eval ====
    segmodule.model.eval()
    compute_iou = ComputeIoU(num_class=config.num_class)
    os.makedirs(logpath, exist_ok=True)
    logpath = os.path.join(logpath, f"{flag}_eval")
    os.makedirs(logpath, exist_ok=True)
    logpath = os.path.join(logpath, "with_rsu" if args.rsu else "no_rsu")
    os.makedirs(logpath, exist_ok=True)
    print("log path:", logpath)

    for idx, sample in enumerate(tqdm(valloader)):
        if flag != "upperbound" and flag != "lowerbound":
            (
                padded_voxel_points_list,
                padded_voxel_points_teacher_list,
                label_one_hot_list,
                trans_matrices,
                target_agent,
                num_sensor,
            ) = list(zip(*sample))
        else:
            (
                padded_voxel_points_list,
                padded_voxel_points_teacher_list,
                label_one_hot_list,
            ) = list(zip(*sample))

        if flag == "upperbound":
            padded_voxel_points = torch.cat(tuple(padded_voxel_points_teacher_list), 0)
        else:
            padded_voxel_points = torch.cat(tuple(padded_voxel_points_list), 0)

        label_one_hot = torch.cat(tuple(label_one_hot_list), 0)

        data = {}
        data["bev_seq"] = padded_voxel_points.to(device).float()
        data["labels"] = label_one_hot.to(device)
        if flag != "upperbound" and flag != "lowerbound":
            trans_matrices = torch.stack(trans_matrices, 1)

            if pose_noise > 0:
                apply_pose_noise(pose_noise, trans_matrices)

            target_agent = torch.stack(target_agent, 1)
            num_sensor = torch.stack(num_sensor, 1)

            if not args.rsu:
                num_sensor -= 1

            data["trans_matrices"] = trans_matrices.to(device)
            data["target_agent"] = target_agent.to(device)
            data["num_sensor"] = num_sensor.to(device)

        pred, labels = segmodule.step(data, num_agent, batch_size, loss=False)

        if args.rsu:
            pred = pred[1:, :, :, :]
            labels = labels[1:, :, :]

        labels = labels.detach().cpu().numpy().astype(np.int32)

        if args.apply_late_fusion:
            pred = torch.flip(pred, (2,))
            size = (1, *pred[0].shape)

            for ii in range(num_sensor[0, 0]):
                for jj in range(num_sensor[0, 0]):
                    if ii == jj:
                        continue

                    nb_agent = torch.unsqueeze(pred[jj], 0)
                    tfm_ji = trans_matrices[0, jj, ii]
                    M = (
                        torch.hstack((tfm_ji[:2, :2], -tfm_ji[:2, 3:4]))
                        .float()
                        .unsqueeze(0)
                    )

                    mask = torch.tensor(
                        [[[1, 1, 4 / 128], [1, 1, 4 / 128]]], device=M.device
                    )

                    M *= mask
                    grid = F.affine_grid(M, size=torch.Size(size)).to(device)
                    warp_feat = F.grid_sample(nb_agent, grid).squeeze()
                    pred[ii] += warp_feat

            pred = torch.flip(pred, (2,))

        pred = torch.argmax(F.softmax(pred, dim=1), dim=1)
        compute_iou(pred, labels)

        if args.visualization and idx % 50 == 0:
            plt.clf()
            pred_map = np.zeros((256, 256, 3))
            gt_map = np.zeros((256, 256, 3))

            for k, v in config.class_to_rgb.items():
                pred_map[np.where(pred.cpu().numpy()[0] == k)] = v
                gt_map[np.where(label_one_hot.numpy()[0] == k)] = v

            plt.imsave(
                f"{logpath}/{idx}_voxel_points.png",
                np.asarray(
                    np.max(padded_voxel_points.cpu().numpy()[0], axis=2), dtype=np.uint8
                ),
            )
            cv2.imwrite(f"{logpath}/{idx}_pred.png", pred_map[:, :, ::-1])
            cv2.imwrite(f"{logpath}/{idx}_gt.png", gt_map[:, :, ::-1])

    print("iou:", compute_iou.get_ious())
    print("miou:", compute_iou.get_miou(ignore=0))
    log_file = open(f"{logpath}/log.txt", "w")
    log_file.write(f"iou: {compute_iou.get_ious()}\n")
    log_file.write(f"miou: {compute_iou.get_miou(ignore=0)}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-d", "--data", default="./dataset/train", type=str, help="Path to data")
    parser.add_argument("--resume", type=str, help="Path to checkpoint")
    parser.add_argument("--model_only", action="store_true", help="only load model")
    parser.add_argument("--batch_size", default=1, type=int, help="Batch size")
    parser.add_argument("--nepoch", default=10, type=int, help="Number of epochs")
    parser.add_argument("--nworker", default=2, type=int, help="Number of workers")
    parser.add_argument("--lr", default=0.001, type=float, help="Initial learning rate")
    parser.add_argument("--com", default="", type=str, help="Communication mode")
    parser.add_argument("--inference", default="activated")
    parser.add_argument("--warp_flag", default=0, type=int, help="Warp flag")
    parser.add_argument("--visualization", action="store_true")
    parser.add_argument("--logpath", default="", help="Log path")
    parser.add_argument("--rsu", default=0, type=int, help="RSU usage")
    parser.add_argument("--num_agent", default=6, type=int, help="Total agents")
    parser.add_argument("--pose_noise", default=0, type=float, help="Pose noise")
    parser.add_argument("--apply_late_fusion", default=0, type=int, help="Late fusion")
    parser.add_argument("--compress_level", default=0, type=int, help="Compression")
    parser.add_argument("--only_v2i", default=0, type=int, help="Only V2I")

    # Network coupling options (Aggiunte per OMNeT++)
    parser.add_argument("--network_host", default="127.0.0.1", type=str, help="Hostname of OMNeT++ bridge")
    parser.add_argument("--network_port", default=5555, type=int, help="Port of OMNeT++ bridge")
    parser.add_argument("--network_timeout", default=2.0, type=float, help="Socket timeout")
    parser.add_argument("--network_default_delay", default=0.0, type=float, help="Fallback latency")
    parser.add_argument("--network_fail_open", action="store_true", help="Continue if bridge fails")
    parser.add_argument("--network_disable", action="store_true", help="Run without OMNeT++")

    torch.multiprocessing.set_sharing_strategy("file_system")

    args = parser.parse_args()
    print("Arguments:", args)
    
    # Configurazione Bridge
    bridge = None
    restore_hook = lambda: None

    try:
        if not args.network_disable:
            print(f"Connecting to OMNeT++ at {args.network_host}:{args.network_port}...")
            bridge = OmnetBridge(
                host=args.network_host,
                port=args.network_port,
                timeout=args.network_timeout,
                default_delay=args.network_default_delay,
                fail_open=args.network_fail_open,
                enabled=True,
            )
        
        # Applica la patch al modello di segmentazione
        restore_hook = patch_feature_transformation(bridge)
        
        config = Config("train")
        run_test_logic(config, args)

    finally:
        # Ripristina e chiudi tutto pulito
        restore_hook()
        if bridge is not None:
            bridge.close()
            print("OMNeT++ Bridge closed.")