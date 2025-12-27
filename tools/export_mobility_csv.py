import argparse
import csv
import os
from typing import List

from nuscenes import NuScenes
from pyquaternion import Quaternion


def export_trajectories(dataroot: str, version: str, channels: List[str], out_dir: str) -> None:
    nusc = NuScenes(version=version, dataroot=dataroot, verbose=False)
    os.makedirs(out_dir, exist_ok=True)

    for agent_id, channel in enumerate(channels):
        rows = []
        for sd in nusc.sample_data:
            if sd["channel"] != channel:
                continue
            ep = nusc.get("ego_pose", sd["ego_pose_token"])
            x, y, z = ep["translation"]
            q = Quaternion(ep["rotation"])
            yaw = q.yaw_pitch_roll[0]
            rows.append((ep["timestamp"] * 1e-6, x, y, z, yaw))

        rows.sort(key=lambda r: r[0])
        out_path = os.path.join(out_dir, f"agent{agent_id}.csv")
        with open(out_path, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["time_s", "x_m", "y_m", "z_m", "yaw_rad"])
            w.writerows(rows)
        print(f"agent {agent_id}: {len(rows)} poses -> {out_path}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Export V2X-Sim nuScenes-style poses to mobility CSVs")
    parser.add_argument("--dataroot", default="data/V2X-Sim-mini", help="nuScenes-style dataroot")
    parser.add_argument("--version", default="v2.0-mini", help="Dataset version")
    parser.add_argument(
        "--channels",
        nargs="+",
        default=[
            "LIDAR_TOP_id_0",
            "LIDAR_TOP_id_1",
            "LIDAR_TOP_id_2",
            "LIDAR_TOP_id_3",
            "LIDAR_TOP_id_4",
            "LIDAR_TOP_id_5",
        ],
        help="Channels to export, order defines agent ids",
    )
    parser.add_argument("--out", default="data/V2X-Sim-mini/mobility", help="Output directory for CSVs")
    args = parser.parse_args()

    export_trajectories(args.dataroot, args.version, args.channels, args.out)


if __name__ == "__main__":
    main()
