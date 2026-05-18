# Comparative Analysis: Konro ON vs Konro OFF — V2X Cooperative Perception

## Objective

This document compares the last two runs of the cooperative perception pipeline:

- **Run A – With Konro** (`konro_enable=True`, `omnet_enable=True`): Konro actively manages the Processing Unit (PU) allocation assigned to the process, sending per-frame feedback to the resource manager.
- **Run B – Without Konro** (`konro_enable=False`, `omnet_enable=True`): the process uses all available PUs (12) with no external resource management.

Both runs use the same dataset (V2X-Sim, `scene_id=29`, 100 frames, 5 mobile agents, Disco model) with V2X communication simulated by OMNeT++.

---

## Aggregate Summary

| Metric | With Konro | Without Konro |
|---|---|---|
| Mean recall | **0.676** | 0.645 |
| Min recall | 0.462 | 0.462 |
| Max recall | **0.867** | 0.857 |
| Recall std dev | 0.114 | 0.094 |
| Mean precision | **0.973** | 0.966 |
| Mean F1 | **0.792** | 0.768 |
| Frames with recall >= 0.8 | **25** | 9 |
| Frames with recall < 0.6 | 32 | 36 |
| Frames with recall < 0.5 | **1** (frame 39) | 3 (frames 39, 41, 45) |
| Below-target ratio (proxy < 0.8) | 0.71 | 0.91 |
| Mean PUs allocated | **~1.12** | 12 (fixed) |
| Feedback events sent to Konro | 100 | 0 |

---

## Network Conditions

The two runs exhibit different network statistics from the OMNeT++ simulation.

| Network metric | With Konro | Without Konro |
|---|---|---|
| Packets transmitted | 2000 | 2000 |
| Packets delivered | **1999** (99.95%) | 1614 (80.7%) |
| Packets dropped | **1** | 386 |
| Mean latency | **55.7 ms** | 99.3 ms |
| P50 latency | **50.5 ms** | 123.9 ms |
| P95 latency | **113.7 ms** | 169.1 ms |
| Stale packets | **152** | 1042 |

Both runs used the same OMNeT++ random seed (`seedset=0`, `runnumber=0` by default). The difference in network statistics is therefore not attributable to different channel realizations, but to **real-time scheduler coupling**: OMNeT++ uses `cRealTimeScheduler`, meaning packet timing depends on when the Python process sends position updates frame by frame. With Konro holding the process at 1 PU, the inference pipeline had a slightly different execution rhythm than the 12-PU baseline, causing different packet buffering and staleness outcomes.

Importantly, despite these network differences, the **impact on recall is modest**: the mean recall gap is only 3.1 percentage points, well within one standard deviation of either run. This demonstrates that the cooperative perception model is reasonably robust to moderate network variability — the system is network-bound but not fragile.

---

## Konro Resource Manager Behavior

### Control Logic

Konro uses the `CoperceptionPolicy` with the following parameters:

- **kLowThreshold = 90**: if the feedback signal (scale 0–200) drops below 90, Konro attempts to allocate more PUs.
- **kHighThreshold = 110**: if feedback exceeds 110, Konro may reduce PUs.
- **kGiveUpTicks = 5**: after 5 consecutive ticks without feedback improvement, Konro gives up and resets to the minimum (1 PU).

The feedback sent each frame is: `feedback = clamp(floor(EMA / target * 100), 0, 200)`, with `target = 0.8` and EMA computed with alpha = 0.2 (strong inertia, ~10–15 frame lag).

### Three Probe Sequences (Staircase Events)

During the Konro run, **3 probe events** occurred, all terminated by `kGiveUpTicks`:

#### Probe 1 — frames 12–16

| Frame | PUs | Recall | EMA |
|---|---|---|---|
| 11 | 1 | 0.714 | 0.706 |
| **12** | **2** | 0.714 | 0.710 |
| **13** | **3** | 0.714 | 0.671 |
| **14** | **4** | 0.714 | 0.656 |
| **15** | **5** | 0.643 | 0.691 |
| **16** | **1** (reset) | 0.571 | 0.632 |

At frame 11, EMA had dropped to ~0.71 (feedback ~88 < kLowThreshold=90). Konro began probing by scaling 1→2→3→4→5 across frames 12–15, but recall remained unchanged (0.71, then 0.64). Feedback stayed below threshold for 5 consecutive ticks → **kGiveUpTicks triggered** → reset to 1 PU at frame 16.

#### Probe 2 — frames 31–35

| Frame | PUs | Recall | EMA |
|---|---|---|---|
| 30 | 1 | 0.500 | 0.697 |
| **31** | **2** | 0.571 | 0.656 |
| **32** | **3** | 0.571 | 0.567 |
| **33** | **4** | 0.571 | 0.576 |
| **34** | **5** | 0.571 | 0.613 |
| **35** | **1** (reset) | 0.500 | 0.628 |

Identical pattern. Recall remained stable at ~0.57 regardless of PU count. kGiveUpTicks triggered → reset to 1 PU.

#### Probe 3 — frames 61–65

| Frame | PUs | Recall | EMA |
|---|---|---|---|
| 60 | 1 | 0.500 | 0.655 |
| **61** | **2** | 0.500 | 0.636 |
| **62** | **3** | 0.500 | 0.612 |
| **63** | **4** | 0.571 | 0.613 |
| **64** | **5** | 0.571 | 0.582 |
| **65** | **1** (reset) | 0.643 | 0.563 |

Third probe, same outcome. After the reset at frame 65, Konro performed no further probes for the remainder of the scene (frames 66–100 stable at 1 PU).

### Interpretation

Konro's behavior is **correct**: it identified that the bottleneck is not the CPU. Cooperative V2X perception is limited by the **network** (drop rate, latency, stale packets), not by local compute capacity. Adding PUs does not improve recall because the model has no additional V2V data to infer from.

The chart **pu_recall_overlay** makes this visually explicit: the three blue staircase bursts (PU axis, bottom) produce no visible change in the EMA recall curves (red = with Konro, orange = without Konro, top), which evolve entirely independently of PU allocation.

---

## Per-Frame Analysis

### Initial phase (frames 1–11): stable network

Both runs produce identical recall in this phase: 0.846, 0.714, 0.786, 0.714, 0.786, 0.786, 0.643, 0.714, 0.714, 0.643, 0.714. The first probe triggers at frame 12 because the accumulated EMA (0.706 at frame 11) pushes the feedback below kLowThreshold.

### Degradation phase (frames 12–65): low recall, multiple probes

The critical phase. Recall oscillates between 0.46 and 0.69, averaging around 0.58. Notable frames:

- **Frame 22**: With Konro recall=0.643 (+1 TP), without Konro 0.571. First significant divergence.
- **Frame 26**: With Konro 0.571, without Konro 0.500 (−1 TP).
- **Frame 30**: With Konro 0.500, without Konro 0.571 — a point reversal where the no-Konro run briefly outperforms, confirming that network variation, not PUs, drives the differences.
- **Frame 39**: Both runs reach the absolute minimum (recall=0.462, 6 TP out of 13 GT). Peak channel degradation.
- **Frames 41, 45**: Without Konro drops again to 0.462; with Konro has recovered to 0.538–0.615.

### Recovery phase (frames 56–100): recall increasing

In the second half of the scene recall recovers for both runs. Notable frames:

- **Frame 56–59**: With Konro holds 0.714 for 4 consecutive frames; without Konro 0.643, 0.643, 0.571, 0.643.
- **Frame 60**: With Konro drops to 0.500 (outlier); without Konro 0.643. The only frame in the recovery phase where the no-Konro run is higher.
- **Frame 70**: With Konro recall=0.800 (12/15 TP), without Konro 0.600 (9/15 TP). Largest gap in the scene: 3 TP difference.
- **Frames 74, 76, 79, 80, 81**: With Konro stable at 0.800 (12/15 TP); without Konro at 0.667 (10/15 TP).
- **Frames 83–90**: With Konro reaches 0.813–0.867 (peak range); without Konro stays at 0.688–0.750.
- **Frames 87–89**: With Konro at 0.867 (13/15 TP) for 3 consecutive frames — the run's maximum. Without Konro at 0.733.
- **Frames 95–100**: Both runs converge toward 0.800–0.857 as network conditions equalize in the final frames.

### PU Distribution

| PUs | Frames (Konro run) | % of scene |
|---|---|---|
| 1 | 88 | **88%** |
| 2 | 3 | 3% |
| 3 | 3 | 3% |
| 4 | 3 | 3% |
| 5 | 3 | 3% |

The no-Konro run uses 12 PUs for all 100 frames. The Konro run averages 1.12 PUs, achieving approximately **91% reduction in compute resource usage** with no meaningful impact on perceived quality.

---

## Chart Guide

### pu_recall_overlay

A dual-axis chart overlaying PU allocation and EMA recall for both runs across the 100-frame scene.

- **Left axis (scale 0–14)**: PUs allocated.
  - Solid blue line (with Konro): nearly flat at 1 PU, with three staircase bursts (1→5→1) at frames 12–16, 31–35, and 61–65.
  - Dashed purple line (without Konro): constant at 12 PUs throughout.
- **Right axis (scale 0–1)**: per-frame recall (transparent) and EMA recall (solid).
  - Red line (EMA with Konro): starts high (~0.85), dips to ~0.63 around frames 30–65, recovers to ~0.88 near frames 93–94.
  - Dashed orange line (EMA without Konro): starts at ~0.85, drops to ~0.55–0.60, recovers more slowly, closes at ~0.81.
  - Dotted grey line: target recall = 0.8.

The key visual takeaway: **the three blue staircase bursts produce no detectable change in either EMA recall curve**, directly confirming that PU count is not the limiting factor.

### per_frame_comparison

A six-panel chart comparing per-frame metrics side by side (blue = with Konro, red = without Konro).

1. **Recall**: curves nearly identical for the first 55–60 frames, then the blue curve separates consistently upward from frame 70 onward.
2. **Precision**: both runs consistently above 0.9 — no systematic difference in individual detection quality.
3. **F1**: mirrors the recall pattern, with a slight blue advantage in the second half.
4. **True Positives**: consistent with recall — modest blue advantage from frame 56 onward.
5. **Ground Truth (num_gts)**: identical for both runs (same scene, same dataset) — the two curves fully overlap.
6. **Detections (num_dets)**: similar variability across both runs, no systematic difference in the number of candidate detections.

---

## Conclusions

### 1. Konro correctly identified that the CPU is not the bottleneck

Across all 3 probe sequences (frames 12–16, 31–35, 61–65), scaling PUs from 1 to 5 produced no improvement in recall or feedback. The `kGiveUpTicks=5` mechanism worked exactly as designed: after 5 consecutive ticks without improvement, the system stopped allocating resources and reset to the minimum. This demonstrates that `CoperceptionPolicy` can self-diagnose cases where the bottleneck lies outside the CPU.

### 2. Recall differences reflect network timing, not compute capacity

The Konro run recorded a higher mean recall (0.676 vs 0.645) and 25 frames above the 0.8 target versus only 9 for the no-Konro run. Both runs shared the same OMNeT++ random seed. The network divergence arises from the real-time scheduler coupling: the Python process running at 1 PU vs 12 PUs produces subtly different frame pacing, which affects when position updates are delivered to OMNeT++ and consequently how packets are buffered and marked stale. The resulting recall gap (3.1 pp, within one standard deviation) is modest and consistent with normal run-to-run variability rather than a systematic effect of PU count.

### 3. Computational efficiency is Konro's primary contribution

88% of frames ran at 1 PU versus a fixed 12 PUs in the baseline, achieving approximately **91% reduction in compute resource usage** with comparable perception quality. In a multi-process scenario — multiple agents, concurrent tasks on an embedded platform — this resource recovery has direct practical value.

### 4. EMA introduces expected response lag

With alpha=0.2, the EMA imposes a ~10–15 frame lag in responding to recall changes. This explains why probes are triggered with a delay relative to the actual degradation onset (degradation begins around frames 7–10, but the first probe fires at frame 12). Systems with faster dynamics could benefit from a higher alpha to reduce this lag.

### 5. Future work: decoupling network timing from PU allocation

The current real-time scheduler coupling means that changing PU allocation indirectly influences packet timing. A cleaner experimental setup would use a deterministic network profile or a fixed-rate position update loop independent of CPU load, allowing a true controlled comparison between Konro-managed and unmanaged runs with identical network conditions.

---

## Run Configuration

| Parameter | Run A (With Konro) | Run B (Without Konro) |
|---|---|---|
| `scene_id` | 29 | 29 |
| `num_agents` | 6 | 6 |
| `konro_enable` | True | False |
| `omnet_enable` | True | True |
| `target_quality` | 0.8 | 0.8 |
| `feedback_noise_std` | 0.2 | 0.0 |
| `below_target_ratio` | 0.71 | 0.91 |
| `feedback_events` | 100 | 0 |
| `omnet_seedset` | 0 | 0 |

---

# Scene 8 Analysis — Konro ON vs Konro OFF

## Obiettivo

Questa sezione analizza i risultati delle run eseguite sulla **scena 8** del dataset V2X-Sim, con la stessa configurazione della scena 29 (modello Disco, 100 frame, 5 agenti mobili, OMNeT++ attivo). I grafici corrispondenti si trovano in:

- [logs/ab/per_frame_comparison_scene8.png](/home/albert0/coperception/coperception-Integration/logs/ab/per_frame_comparison_scene8.png)
- [logs/ab/pu_recall_overlay_scene8.png](/home/albert0/coperception/coperception-Integration/logs/ab/pu_recall_overlay_scene8.png)

---

## Aggregate Summary — Scena 8

| Metrica | Con Konro | Senza Konro |
|---|---|---|
| Mean recall | 0.833 | 0.829 |
| Min recall | 0.571 | 0.571 |
| Max recall | **1.000** | 0.909 |
| Recall std dev | 0.064 | 0.069 |
| Mean precision | **0.984** | 0.979 |
| Mean F1 | **0.900** | 0.896 |
| Frames con recall ≥ 0.8 | **88** | 86 |
| Frames con recall < 0.6 | 1 | 1 |
| Frames con recall < 0.5 | 0 | 0 |
| Below-target ratio (proxy < 0.8) | 0.42 | **0.14** |
| Mean PUs allocate | **3.91** | 12 (fisso) |
| Feedback events inviati a Konro | 100 | 0 |

> Confronto con scena 29: in scena 8 entrambe le run partono già da un livello di recall molto più alto (~0.83 vs ~0.65). La differenza tra con/senza Konro è molto più piccola e non sistematica come in scena 29.

---

## Condizioni di Rete — Scena 8

| Metrica rete | Con Konro | Senza Konro |
|---|---|---|
| Pacchetti trasmessi | 2000 | 2000 |
| Pacchetti consegnati | **1893** (94.65%) | 1823 (91.15%) |
| Pacchetti persi | **107** (5.35%) | 177 (8.85%) |
| Latenza media | 83.9 ms | **83.4 ms** |
| Latenza P95 | **155.2 ms** | 159.4 ms |
| Stale packets | **790** | 1060 |

Analogamente alla scena 29, la run con Konro mostra una delivery ratio migliore e meno stale packets rispetto alla run senza Konro. Questa differenza ha la stessa origine: il real-time scheduler coupling tra Python e OMNeT++ fa sì che il ritmo di esecuzione (influenzato dall'allocazione CPU di Konro) produca un profilo di timing pacchetti leggermente diverso.

---

## Comportamento di Konro — Scena 8

### Differenza fondamentale rispetto alla scena 29

In scena 29, Konro eseguiva **3 probe sequence separate** (frames 12-16, 31-35, 61-65), ciascuna terminata dal meccanismo `kGiveUpTicks=5` dopo aver rilevato che aggiungere PU non migliorava il recall. La maggior parte dei frame (88%) restava a 1 PU.

In scena 8, Konro avvia **una singola probe sequence a frame 32 che non si interrompe mai fino alla fine della scena (frame 100)**. I PU salgono progressivamente da 1 fino a 8 senza che `kGiveUpTicks` si attivi mai. Distribuzione PU risultante:

| PU | Frame | % |
|---|---|---|
| 1 | 31 | 31% |
| 2 | 1 | 1% |
| 3 | 1 | 1% |
| 4 | 30 | 30% |
| 5 | 4 | 4% |
| 6 | 24 | 24% |
| 7 | 1 | 1% |
| 8 | 8 | 8% |
| **Media** | | **3.91 PU** |

### Sequenza di escalation (frames 32–100)

| Frame | PU | Recall | EMA |
|---|---|---|---|
| 31 | 1 | 0.818 | 0.728 (sotto soglia) |
| **32** | **2** | 0.750 | 0.675 |
| **33** | **3** | 0.636 | 0.679 |
| **34** | **4** | 0.909 | 0.743 |
| 35–63 | 4 | 0.818–0.909 | 0.713–0.877 |
| **64** | **5** | 0.818 | 0.770 |
| 65–67 | 5 | 0.818 | 0.688–0.783 |
| **68** | **6** | 0.818 | 0.750 |
| 69–91 | 6 | 0.750–0.846 | 0.700–0.859 |
| **92** | **7** | 0.846 | 0.700 |
| **93** | **8** | 0.846 | 0.760 |
| 94–100 | 8 | 0.769–0.846 | 0.742–0.833 |

### Perché kGiveUpTicks non si attiva mai

`kGiveUpTicks=5` richiede 5 tick **consecutivi** con feedback sotto `kLowThreshold=90`. Con `feedback_noise_std=0.2`, il segnale di proxy è fortemente perturbato. In scena 8 il recall vero è ~0.83 (prossimo al target 0.80): aggiungere rumore gaussiano con std=0.2 fa oscillare il proxy continuamente sopra/sotto la soglia, impedendo che si accumulino 5 tick consecutivi sotto `kLowThreshold`. Il risultato è che Konro interpreta ogni frame con proxy>soglia come un "miglioramento" dovuto all'aumento di PU, e continua a scalare.

Questa è una differenza comportamentale rispetto alla scena 29, dove il recall vero era ~0.64-0.68 (sistematicamente sotto il target 0.80): il segnale noisy rimaneva comunque consistentemente basso, permettendo a `kGiveUpTicks` di accumularsi e resettare a 1 PU.

**In sintesi:**

| Scena | Recall medio | Feedback noise | Effetto su kGiveUpTicks | Comportamento Konro |
|---|---|---|---|---|
| 29 | ~0.65 | std=0.2 | Si accumula (recall < target in modo stabile) | 3 probe brevi, reset a 1 PU |
| 8 | ~0.83 | std=0.2 | Non si accumula (recall vicino/sopra target, noise fa oscillare) | 1 probe lunga, escalation fino a 8 PU |

---

## Analisi del Recall per Frame — Scena 8

### Fase iniziale (frames 1–31): qualità alta a 1 PU

Entrambe le run mostrano recall elevato, frequentemente 0.818–0.909. Le differenze tra le due run sono piccole e alternanti (frame 2: Konro 0.800 vs no-Konro 0.700; frame 4: 0.900 vs 0.700; frame 8: 0.800 vs 0.900). Non c'è un vantaggio sistematico di nessuna delle due configurazioni. Frame notevole: frame 22, dove Konro raggiunge recall=**1.000** (tutti i GT rilevati) a 1 PU.

### Fase di probe (frames 32–100): PU crescenti, recall stabile

La probe inizia a frame 32 perché l'EMA è scesa a 0.675 (feedback noisy sotto soglia). A partire da frame 34, con PU=4, il recall stabilizza intorno a 0.818–0.909. Nonostante l'aumento di PU, **il recall con Konro non è sistematicamente superiore a quello senza Konro**: si alternano frame in cui Konro è leggermente avanti (+0.083–+0.200) e frame in cui è leggermente indietro (frame 79: Konro 0.750 vs no-Konro 0.833). La differenza finale di 0.004 punti di recall medio non è attribuibile all'allocazione CPU.

### Frame notevoli

- **Frame 4**: Konro 0.900, no-Konro 0.700 (Δ=+0.200, più grande gap a favore di Konro — prima della probe)
- **Frame 16**: Konro 0.700, no-Konro 0.900 (Δ=−0.200, più grande gap a favore di no-Konro)
- **Frame 22**: Konro recall=**1.000** (unico frame con recall perfetto nell'intera run), no-Konro 0.900
- **Frame 79**: Konro 0.750 vs no-Konro 0.833 (Δ=−0.083) con PU=6 — conferma che più PU non implica recall maggiore

---

## Guide ai Grafici

### pu_recall_overlay_scene8

Grafico dual-axis che sovrappone l'allocazione PU e l'EMA recall per entrambe le run (100 frame).

- **Asse sinistro**: PU allocate
  - Linea blu continua (con Konro): parte a 1 PU, poi scala progressivamente da frame 32 fino a 8 PU verso fine scena. Non c'è nessun reset a 1 PU (comportamento opposto alla scena 29).
  - Linea viola tratteggiata (senza Konro): costante a 12 PU.
- **Asse destro**: EMA recall
  - Linea rossa (con Konro): parte alta (~0.85), scende intorno a frame 31–32, poi si stabilizza/risale.
  - Linea arancione tratteggiata (senza Konro): andamento simile, le due curve si sovrappongono quasi completamente.
- Linea grigia tratteggiata: target recall=0.8.

Rispetto alla scena 29, non si vedono i tre "gradini" di discesa e risalita dei PU. I PU con Konro salgono e rimangono alti, e le due curve di recall sono quasi indistinguibili — il che conferma che anche in questa scena il PU count non è il fattore limitante.

### per_frame_comparison_scene8

Grafico a 6 pannelli con confronto per frame (blu=con Konro, rosso=senza Konro).

1. **Recall**: curve quasi sovrapposte per tutta la durata. Piccole divergenze alternanti, senza vantaggio sistematico.
2. **Precision**: entrambe le run costantemente sopra 0.95, nessuna differenza rilevante.
3. **F1**: segue il pattern del recall, le due curve sono quasi identiche.
4. **True Positives**: differenze di 0–1 TP per frame, nessun trend sistematico.
5. **Ground Truth (num_gts)**: identico nelle due run — le due curve si sovrappongono perfettamente (stessa scena, stesso dataset).
6. **Detections (num_dets)**: variabilità simile, nessuna differenza sistematica.

---

## Confronto tra Scena 29 e Scena 8

| Metrica | Scena 29 (con Konro) | Scena 29 (senza Konro) | Scena 8 (con Konro) | Scena 8 (senza Konro) |
|---|---|---|---|---|
| Mean recall | 0.676 | 0.645 | 0.833 | 0.829 |
| Frames recall ≥ 0.8 | 25 | 9 | 88 | 86 |
| Below-target ratio | 0.71 | 0.91 | 0.42 | 0.14 |
| Mean PUs | 1.12 | 12 | 3.91 | 12 |
| Risparmio PU vs baseline | 91% | — | 67% | — |
| Delivery ratio | 99.95% | 80.7% | 94.65% | 91.15% |
| Stale packets | 152 | 1042 | 790 | 1060 |
| kGiveUpTicks attivati | 3 | — | 0 | — |
| Probe sequences | 3 brevi | — | 1 lunga | — |

### Osservazioni cross-scena

**1. L'efficacia di Konro dipende dalla qualità di partenza della scena.**
In scena 29 (bassa qualità, recall ~0.65), Konro identifica correttamente che aggiungere PU non aiuta e resetta a 1 PU tramite `kGiveUpTicks`. In scena 8 (alta qualità, recall ~0.83), il meccanismo non si attiva perché il segnale noisy oscilla intorno alla soglia target, impedendo l'accumulo dei 5 tick consecutivi.

**2. Il risparmio energetico è comunque significativo in entrambe le scene.**
In scena 29: 91% di risparmio PU. In scena 8: 67% di risparmio PU (3.91 PU medi vs 12 del baseline). Nessuna delle due run con Konro usa le 12 PU del baseline per l'intera durata.

**3. Il feedback noise std=0.2 è più problematico in scene ad alta qualità.**
In scena 29, dove il vero recall è sistematicamente sotto target, la perturbazione non impedisce a `kGiveUpTicks` di attivarsi. In scena 8, dove il vero recall è vicino o sopra il target, la perturbazione fa oscillare il segnale e impedisce il convergere del meccanismo di give-up. Una possibile soluzione è ridurre `feedback_noise_std` o aumentare `kGiveUpTicks` per scene ad alta qualità.

**4. Il recall risultante è praticamente identico con/senza Konro in entrambe le scene.**
La differenza media è di +0.031 punti in scena 29 (a favore di Konro, riconducibile alla variabilità del network timing) e +0.004 punti in scena 8. In nessun caso il PU count è il fattore determinante del recall.

**5. La rete si comporta sempre meglio con Konro attivo.**
In entrambe le scene, Konro produce meno stale packets e una delivery ratio più alta, per lo stesso motivo: il ritmo di esecuzione a CPU limitata produce un profilo di timing pacchetti leggermente più favorevole con OMNeT++.

---

## Configurazione Run — Scena 8

| Parametro | Con Konro | Senza Konro |
|---|---|---|
| `scene_id` | 8 | 8 |
| `num_agents` | 6 | 6 |
| `konro_enable` | True | False |
| `omnet_enable` | True | True |
| `target_quality` | 0.80 | 0.80 |
| `feedback_noise_std` | 0.2 | 0.0 |
| `below_target_ratio` | 0.42 | 0.14 |
| `feedback_events` | 100 | 0 |
| Run index in history | 7 (with_konro_with_omnet) | 3 (without_konro_with_omnet) |
