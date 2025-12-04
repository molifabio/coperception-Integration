import argparse
import json
import socket
import sys
import time
from typing import Any, Dict, Optional

import torch

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
        if not self.enabled or self._stream is None:
            return {"deliver": True, "delay_s": self.default_delay}

        payload = {
            "topic": topic,
            "sender": sender,
            "receiver": receiver,
            "size_bytes": size_bytes,
            "timestamp": time.time(),
            "metadata": metadata or {},
        }

        try:
            self._send_json(payload)
            reply = self._recv_json()
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

        deliver = bool(reply.get("deliver", True))
        delay_s = float(reply.get("delay_s", reply.get("delay", self.default_delay)))
        return {"deliver": deliver, "delay_s": max(0.0, delay_s)}

    @staticmethod
    def apply_delay(delay_s: float) -> None:
        if delay_s > 0:
            time.sleep(delay_s)

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


def patch_feature_transformation(bridge: Optional[OmnetBridge]):
    if bridge is None:
        return lambda: None

    descriptor = DetModelBase.__dict__["feature_transformation"]
    original = descriptor.__func__ if isinstance(descriptor, staticmethod) else descriptor

    def wrapped(b, j, agent_idx, local_com_mat, all_warp, device, size, trans_matrices):
        payload = local_com_mat[b, j]
        meta = {
            "batch": int(b),
            "sender": int(j),
            "receiver": int(agent_idx),
            "shape": list(payload.shape),
            "dtype": str(payload.dtype)
        }
        decision = bridge.transmit(
            topic="feature_tensor",
            sender=int(j),
            receiver=int(agent_idx),
            size_bytes=int(payload.element_size() * payload.numel()),
            metadata=meta,
        )
        if not decision.get("deliver", True):
            return torch.zeros_like(payload)
        delay = float(decision.get("delay_s", 0.0))
        OmnetBridge.apply_delay(delay)
        return original(b, j, agent_idx, local_com_mat, all_warp, device, size, trans_matrices)

    DetModelBase.feature_transformation = staticmethod(wrapped)

    def restore():
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

    return parser


def main():
    parser = build_parser()
    args = parser.parse_args()

    bridge = None
    restore_hook = lambda: None

    try:
        if not args.network_disable:
            bridge = OmnetBridge(
                host=args.network_host,
                port=args.network_port,
                timeout=args.network_timeout,
                default_delay=args.network_default_delay,
                fail_open=args.network_fail_open,
                enabled=True,
            )
        restore_hook = patch_feature_transformation(bridge)
        torch.multiprocessing.set_sharing_strategy("file_system")
        print(args)
        test_codet.main(args)
    finally:
        restore_hook()
        if bridge is not None:
            bridge.close()


if __name__ == "__main__":
    main()
