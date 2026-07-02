#!/usr/bin/env python3
"""Temporary analysis script to extract per-frame comparison data."""
import json
import statistics

with open('/home/albert0/coperception/coperception-Integration/logs/ab/with_konro_with_omnet_history.json') as f:
    da = json.load(f)['runs'][-1]
with open('/home/albert0/coperception/coperception-Integration/logs/ab/without_konro_with_omnet_history.json') as f:
    db = json.load(f)['runs'][-1]

fha = da['proxy']['frames_history']
fhb = db['proxy']['frames_history']

# PU changes
pu_changes = []
for i in range(1, len(fha)):
    if fha[i]['num_pus'] != fha[i-1]['num_pus']:
        pu_changes.append({'frame': fha[i]['frame'], 'from_pu': fha[i-1]['num_pus'], 'to_pu': fha[i]['num_pus']})

print("=== PU CHANGES (konro run) ===")
for c in pu_changes:
    f_idx = c['frame'] - 1
    recall_at = fha[f_idx]['recall']
    ema_at = fha[f_idx]['ema']
    print(f"  frame {c['frame']:3d}: {c['from_pu']} -> {c['to_pu']}  (recall={recall_at:.3f}, ema={ema_at:.3f})")
print(f"Total PU changes: {len(pu_changes)}")
print()

# PU distribution
pus = [f['num_pus'] for f in fha]
pu_vals = sorted(set(pus))
print(f"PU distribution: {pu_vals}")
for p in pu_vals:
    frames_at_p = [f['frame'] for f in fha if f['num_pus'] == p]
    print(f"  PU={p}: {len(frames_at_p)} frames")
print()

# Stats
def stats(vals):
    return {
        'mean': round(statistics.mean(vals), 4),
        'min': round(min(vals), 4),
        'max': round(max(vals), 4),
        'stdev': round(statistics.stdev(vals), 4)
    }

recall_a = [f['recall'] for f in fha]
recall_b = [f['recall'] for f in fhb]
prec_a = [f['precision'] for f in fha]
prec_b = [f['precision'] for f in fhb]
f1_a = [f['f1'] for f in fha]
f1_b = [f['f1'] for f in fhb]
tp_a = [f['num_tp'] for f in fha]
tp_b = [f['num_tp'] for f in fhb]
gts_a = [f['num_gts'] for f in fha]
dets_a = [f['num_dets'] for f in fha]
dets_b = [f['num_dets'] for f in fhb]

print("=== STATS CON KONRO ===")
print(f"  recall:    {stats(recall_a)}")
print(f"  precision: {stats(prec_a)}")
print(f"  f1:        {stats(f1_a)}")
print(f"  num_tp:    {stats(tp_a)}")
print(f"  num_gts:   {stats(gts_a)}")
print(f"  num_dets:  {stats(dets_a)}")
print(f"  proxy_mean:  {da['proxy']['proxy_mean']:.4f}")
print(f"  proxy_ema:   {da['proxy']['proxy_ema']:.4f}")
print(f"  below_target_ratio: {da['proxy']['below_target_ratio']}")

print()
print("=== STATS SENZA KONRO ===")
recall_b2 = [f['recall'] for f in fhb]
print(f"  recall:    {stats(recall_b2)}")
print(f"  precision: {stats(prec_b)}")
print(f"  f1:        {stats(f1_b)}")
print(f"  num_tp:    {stats(tp_b)}")
print(f"  num_gts:   {stats([f['num_gts'] for f in fhb])}")
print(f"  num_dets:  {stats(dets_b)}")
print(f"  proxy_mean:  {db['proxy']['proxy_mean']:.4f}")
print(f"  proxy_ema:   {db['proxy']['proxy_ema']:.4f}")
print(f"  below_target_ratio: {db['proxy']['below_target_ratio']}")

print()
# Bad frames
bad_a = [f for f in fha if f['recall'] < 0.6]
bad_b = [f for f in fhb if f['recall'] < 0.6]
print(f"Frames recall < 0.6:  konro={len(bad_a)}, no_konro={len(bad_b)}")
vbad_a = [f for f in fha if f['recall'] < 0.5]
vbad_b = [f for f in fhb if f['recall'] < 0.5]
print(f"Frames recall < 0.5:  konro={len(vbad_a)}, no_konro={len(vbad_b)}")
print(f"  konro <0.5:    {[(f['frame'], round(f['recall'],3), f['num_pus']) for f in vbad_a]}")
print(f"  no_konro <0.5: {[(f['frame'], round(f['recall'],3)) for f in vbad_b]}")

print()
# Frames above target (0.8)
above_a = [f for f in fha if f['recall'] >= 0.8]
above_b = [f for f in fhb if f['recall'] >= 0.8]
print(f"Frames recall >= 0.8: konro={len(above_a)}, no_konro={len(above_b)}")

print()
print("=== PER-FRAME TABLE ===")
print("FRAME | recall_k | pu_k | ema_k  | recall_nk | pu_nk | ema_nk | num_gts | tp_k | tp_nk")
for i in range(100):
    a = fha[i]
    b = fhb[i]
    pu_mark = "*" if a['num_pus'] != (fha[i-1]['num_pus'] if i > 0 else a['num_pus']) else " "
    print(f"  {a['frame']:3d}  | {a['recall']:.4f}   | {a['num_pus']:4d}{pu_mark}| {a['ema']:.4f} | {b['recall']:.4f}    | {b['num_pus']:5d} | {b['ema']:.4f} | {a['num_gts']:7d} | {a['num_tp']:4d} | {b['num_tp']:5d}")

print()
print("=== NETWORK COMPARISON ===")
print("With Konro:    ", da['network'])
print("Without Konro: ", db['network'])
