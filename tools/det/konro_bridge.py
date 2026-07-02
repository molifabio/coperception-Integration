"""
Thin Python client for Konro resource manager.

Replicates the konrolib C++ API (sendAddMessage / sendFeedbackMessage)
using plain HTTP POST requests — no C++ bindings needed.
"""

import json
import os
import random
import sys
from typing import Optional

try:
    import requests  # type: ignore
except ImportError:
    requests = None  # fallback to urllib below

import urllib.request
import urllib.error


def _get_server_address() -> str:
    return os.environ.get("KONRO", "http://localhost:8080")


def _parse_cpuset_count(cpuset_str: str) -> int:
    """Count PUs in a cpuset string like '0-3,5,7-8'. Returns -1 if unparseable."""
    count = 0
    for part in cpuset_str.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            try:
                a, b = part.split("-", 1)
                count += int(b) - int(a) + 1
            except ValueError:
                return -1
        elif part.isdigit():
            count += 1
        else:
            return -1
    return count if count > 0 else -1


def _read_num_pus() -> int:
    """Read the number of PUs currently assigned to this process via cgroup cpuset.

    Tries cgroup v2 first, then falls back to cgroup v1.
    Returns -1 if the information is unavailable.
    """
    try:
        # cgroup v2: find the unified hierarchy path in /proc/self/cgroup
        cgroup_path = None
        with open("/proc/self/cgroup", "r") as f:
            for line in f:
                parts = line.strip().split(":", 2)
                if len(parts) == 3 and parts[0] == "0":
                    cgroup_path = parts[2].lstrip("/")
                    break

        if cgroup_path is not None:
            cpuset_file = f"/sys/fs/cgroup/{cgroup_path}/cpuset.cpus.effective"
            if os.path.exists(cpuset_file):
                with open(cpuset_file) as f:
                    return _parse_cpuset_count(f.read().strip())

        # cgroup v2 root-level fallback (process in root cgroup)
        root_cpuset = "/sys/fs/cgroup/cpuset.cpus.effective"
        if os.path.exists(root_cpuset):
            with open(root_cpuset) as f:
                return _parse_cpuset_count(f.read().strip())

        # cgroup v1 fallback
        with open("/proc/self/cgroup", "r") as f:
            for line in f:
                parts = line.strip().split(":", 2)
                if len(parts) == 3 and "cpuset" in parts[1]:
                    v1_path = parts[2].lstrip("/")
                    cpuset_file = f"/sys/fs/cgroup/cpuset/{v1_path}/cpuset.cpus"
                    if os.path.exists(cpuset_file):
                        with open(cpuset_file) as f2:
                            return _parse_cpuset_count(f2.read().strip())
    except OSError:
        pass
    return -1


def _post_json(endpoint: str, payload: dict) -> str:
    url = f"{_get_server_address()}/{endpoint}"
    data = json.dumps(payload).encode("utf-8")

    if requests is not None:
        try:
            resp = requests.post(url, json=payload, timeout=2.0)
            return resp.text
        except requests.ConnectionError:
            return ""
        except requests.Timeout:
            return ""

    # Fallback: stdlib urllib
    req = urllib.request.Request(
        url,
        data=data,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=2.0) as resp:
            return resp.read().decode("utf-8")
    except (urllib.error.URLError, OSError):
        return ""


def _get_pid_namespace() -> int:
    """Read the PID namespace inode, same as konrolib does."""
    try:
        import stat as _stat

        st = os.stat("/proc/self/ns/pid")
        return st.st_ino
    except OSError:
        return 0


def send_add_message() -> str:
    """Register this process with Konro (equivalent to konro::sendAddMessage)."""
    payload = {
        "pid": os.getpid(),
        "namespace": _get_pid_namespace(),
        "name": sys.argv[0] if sys.argv else "coperception",
    }
    return _post_json("add", payload)


def send_feedback_message(feedback: int) -> str:
    """Send a feedback value [0-200] to Konro (equivalent to konro::sendFeedbackMessage)."""
    feedback = max(0, min(200, int(feedback)))
    payload = {
        "pid": os.getpid(),
        "feedback": feedback,
        "namespace": _get_pid_namespace(),
    }
    return _post_json("feedback", payload)


def compute_feedback(current_value: float, target_value: float = None) -> int:
    fb = int(current_value * 200)
    return max(0, min(200, fb))


class PerceptionProxyTracker:
    """
    Computes a per-frame proxy quality metric and sends feedback to Konro.

    Proxy = recall × min(1, num_gts / num_dets)
    Smoothed with exponential moving average (EMA).
    """

    def __init__(
        self,
        target_quality: float = 0.85,
        ema_alpha: float = 0.2,
        feedback_interval: int = 5,
        konro_enabled: bool = True,
        feedback_noise_std: float = 0.0,
    ):
        self.target_quality = target_quality
        self.ema_alpha = ema_alpha
        self.feedback_interval = feedback_interval
        self.konro_enabled = konro_enabled
        self.feedback_noise_std = max(0.0, feedback_noise_std)

        self._ema: Optional[float] = None
        self._frame_count = 0
        self._registered = False
        self._stopped = False
        self._proxy_sum = 0.0
        self._proxy_min = 1.0
        self._proxy_max = 0.0
        self._below_target_count = 0
        self._feedback_count = 0
        self._frames_history: List[dict] = []

    def register(self):
        """Register with Konro. Call once at startup."""
        if self.konro_enabled and not self._registered:
            result = send_add_message()
            self._registered = True
            print(f"[KonroBridge] Registered with Konro: {result}")

    def shutdown(self):
        """Stop sending feedback to Konro. Call before process cleanup to avoid
        in-flight HTTP posts arriving after Konro has already removed this PID."""
        self._stopped = True

    def update(self, num_gts: int, num_dets: int, num_tp: int):
        """
        Update the proxy metric with one frame's results.

        Args:
            num_gts: number of ground-truth objects in this frame
            num_dets: number of detections produced by the model
            num_tp: number of true positives (detections matching a GT with IoU > threshold)
        """
        # Compute per-frame recall and precision (true values, no noise)
        recall = num_tp / max(num_gts, 1)
        precision = num_tp / max(num_dets, 1)
        f1 = (2.0 * precision * recall / (precision + recall)) if (precision + recall) > 0 else 0.0

        # Penalize false positives: if dets >> gts, ratio < 1
        fp_penalty = min(1.0, num_gts / max(num_dets, 1))

        # Proxy quality metric
        proxy = recall * fp_penalty
        if self.feedback_noise_std > 0.0:
            proxy = min(1.0, max(0.0, proxy + random.gauss(0.0, self.feedback_noise_std)))

        self._proxy_sum += proxy
        self._proxy_min = min(self._proxy_min, proxy)
        self._proxy_max = max(self._proxy_max, proxy)
        if proxy < self.target_quality:
            self._below_target_count += 1

        # EMA smoothing
        if self._ema is None:
            self._ema = proxy
        else:
            self._ema = self.ema_alpha * proxy + (1.0 - self.ema_alpha) * self._ema

        self._frame_count += 1

        # Read current PU count from cgroup and store per-frame record
        num_pus = _read_num_pus()
        self._frames_history.append({
            "frame": self._frame_count,
            "num_gts": num_gts,
            "num_dets": num_dets,
            "num_tp": num_tp,
            "recall": round(recall, 6),
            "precision": round(precision, 6),
            "f1": round(f1, 6),
            "proxy": round(proxy, 6),
            "ema": round(self._ema, 6),
            "num_pus": num_pus,
        })

        print(
            f"[Proxy] frame {self._frame_count}: "
            f"gts={num_gts} dets={num_dets} tp={num_tp} "
            f"recall={recall:.3f} precision={precision:.3f} "
            f"proxy={proxy:.3f} ema={self._ema:.3f} pus={num_pus}"
        )

        # Send feedback to Konro at the specified interval
        if self.konro_enabled and not self._stopped and self._frame_count % self.feedback_interval == 0:
            fb = compute_feedback(self._ema, self.target_quality)
            send_feedback_message(fb)
            self._feedback_count += 1
            print(f"[KonroBridge] Sent feedback={fb} (ema={self._ema:.3f}, target={self.target_quality})")

    @property
    def current_ema(self) -> float:
        return self._ema if self._ema is not None else 0.0

    @property
    def frame_count(self) -> int:
        return self._frame_count

    def summary(self) -> dict:
        frames = max(1, self._frame_count)
        return {
            "frames": self._frame_count,
            "target_quality": self.target_quality,
            "proxy_mean": self._proxy_sum / frames,
            "proxy_min": self._proxy_min if self._frame_count > 0 else 0.0,
            "proxy_max": self._proxy_max if self._frame_count > 0 else 0.0,
            "proxy_ema": self.current_ema,
            "below_target_ratio": self._below_target_count / frames,
            "feedback_events": self._feedback_count,
            "frames_history": self._frames_history,
        }
