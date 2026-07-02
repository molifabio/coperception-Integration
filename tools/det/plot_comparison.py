#!/usr/bin/env python3
"""
Compare per-frame coperception metrics between two runs (e.g. with vs without Konro).

Reads frames_history from a JSON history file produced by test_codet_network.py
and generates side-by-side comparison charts for:
  - recall
  - precision
  - F1
  - num_tp, num_gts, num_dets

Usage examples
--------------
# Two runs in the same history file (index 0 = with Konro, index 1 = without):
  python plot_comparison.py \\
      --file  logs/ab/with_konro_with_omnet_history.json \\
      --idx-a 0 --label-a "With Konro" \
      --idx-b 1 --label-b "Without Konro" \
      --out   comparison.png

# Two separate history files:
  python plot_comparison.py \
      --file-a logs/ab/with_konro.json   --label-a "With Konro" \
      --file-b logs/ab/no_konro.json     --label-b "Without Konro" \
      --out    comparison.png
"""

import argparse
import json
import os
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional


# ---------------------------------------------------------------------------
# JSON helpers
# ---------------------------------------------------------------------------

def _load_history(path: str) -> Dict[str, Any]:
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def _select_run(data: Dict[str, Any], idx: int) -> Dict[str, Any]:
    """Return a single run dict from a history JSON."""
    if isinstance(data, dict) and isinstance(data.get("runs"), list):
        runs = data["runs"]
        if not runs:
            raise ValueError("History file contains no runs")
        try:
            return runs[idx]
        except IndexError:
            raise ValueError(
                f"Index {idx} out of range — file has {len(runs)} run(s)"
            )
    # Legacy single-run format
    return data


def _get_frames_history(run: Dict[str, Any]) -> List[Dict[str, Any]]:
    history = run.get("proxy", {}).get("frames_history", [])
    if not history:
        raise ValueError(
            "This run has no frames_history. "
            "Re-run with the updated test_codet_network.py to collect per-frame data."
        )
    return history


# ---------------------------------------------------------------------------
# Plot generation
# ---------------------------------------------------------------------------

def _extract(history: List[Dict], key: str) -> List[float]:
    return [float(f.get(key, 0)) for f in history]


def generate_comparison_plot(
    history_a: List[Dict],
    history_b: List[Dict],
    label_a: str,
    label_b: str,
    target_quality: Optional[float],
    out_path: str,
) -> None:
    try:
        import matplotlib          # type: ignore
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt  # type: ignore
        import matplotlib.gridspec as gridspec  # type: ignore
    except ImportError:
        print("[Plot] matplotlib not available — install it with: pip install matplotlib")
        sys.exit(1)

    frames_a = _extract(history_a, "frame")
    frames_b = _extract(history_b, "frame")

    COLOR_A = "#1976D2"   # blue  – run A
    COLOR_B = "#E53935"   # red   – run B
    ALPHA   = 0.8

    # ── panels: recall, precision, F1, counts ─────────────────────────────
    panels = [
        ("recall",    "Recall",     True),
        ("precision", "Precision",  True),
        ("f1",        "F1 score",   True),
        ("num_tp",    "True Positives",  False),
    ]

    n_panels = len(panels)
    fig = plt.figure(figsize=(14, 3.2 * n_panels))
    gs = gridspec.GridSpec(n_panels, 1, hspace=0.45)

    for i, (key, ylabel, is_ratio) in enumerate(panels):
        ax = fig.add_subplot(gs[i])
        vals_a = _extract(history_a, key)
        vals_b = _extract(history_b, key)

        ax.plot(frames_a, vals_a, color=COLOR_A, linewidth=1.4, alpha=ALPHA, label=label_a)
        ax.plot(frames_b, vals_b, color=COLOR_B, linewidth=1.4, alpha=ALPHA, label=label_b)

        if is_ratio and target_quality is not None:
            ax.axhline(y=target_quality, color="gray", linestyle="--",
                       linewidth=0.9, label=f"Target ({target_quality})")
            ax.set_ylim(0.0, 1.05)

        ax.set_xlabel("Frame")
        ax.set_ylabel(ylabel)
        ax.set_title(ylabel)
        ax.legend(fontsize=8, loc="lower right")
        ax.grid(axis="y", linestyle=":", linewidth=0.5, alpha=0.6)

    fig.suptitle(
        f"Per-frame comparison: {label_a} vs {label_b}",
        fontsize=13, fontweight="bold", y=1.005,
    )

    try:
        out_dir = os.path.dirname(out_path)
        if out_dir:
            os.makedirs(out_dir, exist_ok=True)
        plt.savefig(out_path, dpi=150, bbox_inches="tight")
        print(f"[Plot] Saved comparison chart → {out_path}")
    except Exception as exc:
        print(f"[Plot] Failed to save chart: {exc}")
        sys.exit(1)
    finally:
        plt.close(fig)


# ---------------------------------------------------------------------------
# PU + recall overlay (both runs on same dual-axis chart)
# ---------------------------------------------------------------------------

def generate_pu_recall_overlay(
    history_a: List[Dict],
    history_b: List[Dict],
    label_a: str,
    label_b: str,
    target_quality: Optional[float],
    out_path: str,
) -> None:
    """Single-panel chart: PU allocation step-plot + recall lines for both runs."""
    try:
        import matplotlib          # type: ignore
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt  # type: ignore
        import matplotlib.ticker as mticker  # type: ignore
    except ImportError:
        print("[Plot] matplotlib not available")
        sys.exit(1)

    frames_a  = _extract(history_a, "frame")
    frames_b  = _extract(history_b, "frame")
    pus_a     = _extract(history_a, "num_pus")
    pus_b     = _extract(history_b, "num_pus")
    recall_a  = _extract(history_a, "recall")
    recall_b  = _extract(history_b, "recall")
    ema_a     = _extract(history_a, "ema")
    ema_b     = _extract(history_b, "ema")

    fig, ax1 = plt.subplots(figsize=(14, 5))

    # Left axis: PU
    COLOR_PU_A = "#1976D2"
    ax1.set_xlabel("Frame")
    ax1.set_ylabel("Allocated PUs")

    valid_a = [p for p in pus_a if p > 0]
    if valid_a:
        ax1.step(frames_a, pus_a, where="post", color=COLOR_PU_A,
                 linewidth=2.0, label=f"PU — {label_a}", linestyle="-")
        ax1.set_ylim(0, max(valid_a) + 2)
    ax1.tick_params(axis="y")
    ax1.yaxis.set_major_locator(mticker.MaxNLocator(integer=True))

    # Right axis: recall
    COLOR_R_A = "#E53935"
    COLOR_R_B = "#FB8C00"
    ax2 = ax1.twinx()
    ax2.set_ylabel("Recall per frame")
    ax2.plot(frames_a, recall_a, color=COLOR_R_A, linewidth=0.9, alpha=0.35)
    ax2.plot(frames_a, ema_a,    color=COLOR_R_A, linewidth=1.6,
             label=f"EMA recall — {label_a}")
    ax2.plot(frames_b, recall_b, color=COLOR_R_B, linewidth=0.9, alpha=0.35)
    ax2.plot(frames_b, ema_b,    color=COLOR_R_B, linewidth=1.6,
             label=f"EMA recall — {label_b}", linestyle="--")
    if target_quality is not None:
        ax2.axhline(y=target_quality, color="gray", linestyle=":",
                    linewidth=1.0, label=f"Target ({target_quality})")
    ax2.set_ylim(0.0, 1.05)

    plt.title(f"PUs and Recall — {label_a} vs {label_b}")
    lines1, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax2.legend(lines1 + lines2, labels1 + labels2, loc="lower right", fontsize=8)

    fig.tight_layout()
    try:
        out_dir = os.path.dirname(out_path)
        if out_dir:
            os.makedirs(out_dir, exist_ok=True)
        plt.savefig(out_path, dpi=150, bbox_inches="tight")
        print(f"[Plot] Saved overlay chart → {out_path}")
    except Exception as exc:
        print(f"[Plot] Failed to save chart: {exc}")
        sys.exit(1)
    finally:
        plt.close(fig)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Generate per-frame comparison charts from two run entries in "
                    "a coperception history JSON.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )

    # ── Single-file mode: both runs in the same history file ──────────────
    p.add_argument(
        "--file", default="",
        help="History JSON file containing both runs (single-file mode)",
    )
    p.add_argument(
        "--idx-a", type=int, default=0,
        help="Index of run A inside --file (default: 0)",
    )
    p.add_argument(
        "--idx-b", type=int, default=1,
        help="Index of run B inside --file (default: 1)",
    )

    # ── Two-file mode: runs in separate files ─────────────────────────────
    p.add_argument("--file-a", default="", help="History JSON for run A")
    p.add_argument("--file-b", default="", help="History JSON for run B")
    p.add_argument(
        "--idx-file-a", type=int, default=0,
        help="Index inside --file-a (default: 0)",
    )
    p.add_argument(
        "--idx-file-b", type=int, default=0,
        help="Index inside --file-b (default: 0)",
    )

    # ── Labels and output ─────────────────────────────────────────────────
    p.add_argument("--label-a", default="Run A", help="Legend label for run A")
    p.add_argument("--label-b", default="Run B", help="Legend label for run B")
    p.add_argument(
        "--target", type=float, default=None,
        help="Draw a horizontal target line on ratio panels (e.g. 0.85)",
    )
    p.add_argument(
        "--out", default="comparison.png",
        help="Output path for the multi-panel comparison chart (default: comparison.png)",
    )
    p.add_argument(
        "--out-overlay", default="",
        help="If set, also save a PU+recall overlay chart to this path",
    )
    return p


def main() -> None:
    args = _build_parser().parse_args()

    # ── Load run A ────────────────────────────────────────────────────────
    if args.file_a:
        data_a = _load_history(args.file_a)
        run_a = _select_run(data_a, args.idx_file_a)
    elif args.file:
        data_a = _load_history(args.file)
        run_a = _select_run(data_a, args.idx_a)
    else:
        print("Error: provide --file (single-file mode) or --file-a / --file-b")
        sys.exit(1)

    # ── Load run B ────────────────────────────────────────────────────────
    if args.file_b:
        data_b = _load_history(args.file_b)
        run_b = _select_run(data_b, args.idx_file_b)
    elif args.file:
        run_b = _select_run(data_a, args.idx_b)   # same file
    else:
        print("Error: provide --file (single-file mode) or --file-a / --file-b")
        sys.exit(1)

    history_a = _get_frames_history(run_a)
    history_b = _get_frames_history(run_b)

    # ── Infer target quality from the runs if not provided on CLI ─────────
    target_quality = args.target
    if target_quality is None:
        tq_a = run_a.get("proxy", {}).get("target_quality")
        tq_b = run_b.get("proxy", {}).get("target_quality")
        if tq_a is not None:
            target_quality = float(tq_a)
        elif tq_b is not None:
            target_quality = float(tq_b)

    print(
        f"[Info] Run A: {len(history_a)} frames | label='{args.label_a}'\n"
        f"[Info] Run B: {len(history_b)} frames | label='{args.label_b}'\n"
        f"[Info] Target quality: {target_quality}"
    )

    generate_comparison_plot(
        history_a, history_b,
        args.label_a, args.label_b,
        target_quality,
        args.out,
    )

    if args.out_overlay:
        generate_pu_recall_overlay(
            history_a, history_b,
            args.label_a, args.label_b,
            target_quality,
            args.out_overlay,
        )


if __name__ == "__main__":
    main()
