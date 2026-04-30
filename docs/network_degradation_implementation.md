# Implementazione del Degrado Progressivo della Rete

## Indice
1. [Panoramica del Sistema](#panoramica-del-sistema)
2. [Architettura del Profilo di Rete](#architettura-del-profilo-di-rete)
3. [Modifiche Implementate](#modifiche-implementate)
4. [Flusso di Esecuzione Completo](#flusso-di-esecuzione-completo)
5. [Strutture Dati](#strutture-dati)
6. [Esempi di Utilizzo](#esempi-di-utilizzo)

---

## Panoramica del Sistema

Il sistema simula condizioni di rete variabili durante l'esecuzione di test di detection cooperativa. La simulazione di rete si basa su due componenti:

1. **OMNeT++**: Simulatore di rete che calcola delay realistici basati su posizioni dei veicoli
2. **Profilo temporale**: Overlay che definisce fasi progressive di degrado della rete

Il profilo temporale agisce come un "piano di degrado" che si sovrappone alle simulazioni fisiche di OMNeT++, aggiungendo ritardi minimi garantiti e probabilità di packet loss.

---

## Architettura del Profilo di Rete

### Strutture Dati Principali

#### `NetworkPhase` (linee 62-66)

```python
@dataclass
class NetworkPhase:
    name: str              # Nome descrittivo della fase (es: "good", "medium", "bad")
    frames: int            # Durata della fase in numero di frame
    delay_floor_s: float   # Ritardo minimo garantito in secondi
    drop_prob: float       # Probabilità di packet loss (0.0-1.0)
```

Questa struttura definisce una singola fase della rete. Ogni fase ha:
- Un **nome** identificativo per il logging e la diagnostica
- Una **durata** espressa in frame
- Un **delay_floor_s** che rappresenta il ritardo minimo che il canale aggiunge (si somma o sostituisce il delay di OMNeT++ se maggiore)
- Una **drop_prob** che determina la probabilità che un pacchetto venga perso indipendentemente dalle condizioni fisiche simulate da OMNeT++

#### `NetworkRuntimeStats` (linee 69-78)

```python
@dataclass
class NetworkRuntimeStats:
    tx_total: int = 0           # Totale pacchetti trasmessi
    delivered: int = 0          # Pacchetti consegnati con successo
    dropped: int = 0            # Pacchetti persi (drop)
    live_packets: int = 0       # Pacchetti consegnati in tempo reale (lag 0)
    delayed_packets: int = 0    # Pacchetti consegnati con ritardo
    stale_packets: int = 0      # Pacchetti troppo vecchi per essere utili
    underflow_packets: int = 0  # Pacchetti per cui il buffer non aveva dati sufficienti
    delays_s: List[float]       # Lista di tutti i delay misurati
    phase_hits: Dict[str, int]  # Conteggio frame per ogni fase
```

Questa struttura raccoglie statistiche di runtime per analizzare le prestazioni della rete.

---

## Modifiche Implementate

### 1. Funzione `_phase_for_frame()` (linee 158-171)

**PRIMA (comportamento ciclico):**
```python
def _phase_for_frame(phases: List[NetworkPhase], frame_idx: int) -> NetworkPhase:
    total_cycle = sum(p.frames for p in phases)
    if total_cycle <= 0:
        return phases[0]
    pos = frame_idx % total_cycle  # CICLO: usa modulo per ripetere
    acc = 0
    for phase in phases:
        acc += phase.frames
        if pos < acc:
            return phase
    return phases[-1]
```

**DOPO (comportamento progressivo):**
```python
def _phase_for_frame(phases: List[NetworkPhase], frame_idx: int) -> NetworkPhase:
    """Return the network phase for a given frame index.
    Phases progress sequentially without cycling - once the last phase is reached, it persists."""
    if not phases:
        raise ValueError("phases list cannot be empty")
    
    acc = 0
    for phase in phases:
        if frame_idx < acc + phase.frames:
            return phase
        acc += phase.frames
    
    # Once all phases are completed, stay in the last phase
    return phases[-1]  # PERSISTENZA: rimane nell'ultima fase
```

**Cambiamenti chiave:**
- **Rimosso l'operatore modulo `%`**: Eliminato il comportamento ciclico
- **Aggiunta persistenza**: Una volta completate tutte le fasi, il sistema rimane nell'ultima fase indefinitamente
- **Validazione input**: Aggiunto controllo per lista vuota

**Comportamento:**
- Frame 0-19: fase "good"
- Frame 20-44: fase "medium"  
- Frame 45-69: fase "bad"
- Frame 70+: fase "worst" (persiste per sempre)

### 2. Default di `--network_profile` (linee 697-707)

**PRIMA:**
```python
default="steady:1000000:0.00:0.00"
```
Rete perfetta (0ms delay, 0% loss) per 1 milione di frame.

**DOPO:**
```python
default="good:20:0.02:0.00,medium:25:0.08:0.03,bad:25:0.15:0.08,worst:10000:0.25:0.15"
```

Profilo di degrado progressivo:
- **good** (20 frame): 20ms delay, 0% packet loss
- **medium** (25 frame): 80ms delay, 3% packet loss
- **bad** (25 frame): 150ms delay, 8% packet loss
- **worst** (10000 frame): 250ms delay, 15% packet loss

**Motivazione del cambiamento:**
Scene tipiche durano ~100 frame. Il profilo precedente con fasi da 100 frame ciascuna non permetteva di vedere il degrado all'interno di una singola scena. Il nuovo profilo completa il degrado in ~70 frame, rendendo visibile l'effetto anche in una singola scena.

### 3. Aggiornamento Help Text (linee 701-707)

Aggiunto nella documentazione del parametro:
```
"Phases progress sequentially without cycling. After all phases, stays in the last phase."
```

Per chiarire il nuovo comportamento agli utenti.

---

## Flusso di Esecuzione Completo

### Fase 1: Inizializzazione (linee 830-855)

```python
# Parsing del profilo dalla stringa
phases = _parse_network_profile(args.network_profile)

# Connessione al bridge OMNeT++ (se non disabilitato)
if not args.network_disable:
    bridge = OmnetBridge(
        host=args.network_host,
        port=args.network_port,
        timeout=args.network_timeout,
        default_delay=args.network_default_delay,
        fail_open=args.network_fail_open,
        enabled=True,
    )

# Applicazione della patch al modello
restore_hook, network_stats = patch_feature_transformation(
    bridge, 
    dataset_framerate=args.dataset_framerate,
    phases=phases,
    rng_seed=args.network_profile_seed,
)
```

**Cosa accade:**
1. Il profilo di rete viene parsato da stringa a lista di `NetworkPhase` tramite `_parse_network_profile()`
2. Se OMNeT++ è abilitato, viene creata la connessione TCP
3. La funzione `patch_feature_transformation()` viene chiamata per intercettare le comunicazioni tra veicoli

### Fase 2: Parsing del Profilo (linee 131-155)

La funzione `_parse_network_profile()` converte la stringa del profilo in oggetti strutturati:

```python
Input:  "good:20:0.02:0.00,medium:25:0.08:0.03"
Output: [
    NetworkPhase(name="good", frames=20, delay_floor_s=0.02, drop_prob=0.0),
    NetworkPhase(name="medium", frames=25, delay_floor_s=0.08, drop_prob=0.03)
]
```

**Validazione:**
- Ogni fase deve avere esattamente 4 campi separati da `:`
- `frames` viene limitato a minimo 1
- `delay_floor_s` viene limitato a minimo 0.0
- `drop_prob` viene clampato tra 0.0 e 1.0

### Fase 3: Monkey-Patching del Modello (funzione `patch_feature_transformation`)

Questa funzione sostituisce la trasformazione delle feature nel modello per intercettare ogni comunicazione tra veicoli.

#### Variabili di Closure

```python
frame_counter = 0                    # Contatore globale dei frame processati
feature_buffer = {}                  # Buffer storico delle feature per ogni agente
rng = random.Random(rng_seed)        # RNG per simulazione packet loss
MAX_BUFFER_LEN = 100                 # Lunghezza massima del buffer storico
```

### Fase 4: Intercettazione delle Comunicazioni (linee 485-600)

Per ogni trasmissione tra veicoli, la funzione wrappata esegue:

#### Step 1: Aggiornamento dello Stato (linee 492-493)
```python
bridge.update_state()  # Legge messaggi da OMNeT++
```

#### Step 2: Conteggio Frame (linee 495-500)
```python
if int(b) == 0 and int(agent_idx) == 0 and int(j) == 1:
    frame_counter += 1
current_phase = _phase_for_frame(phases, frame_counter)
```

**Logica del conteggio:**
- Incrementa solo una volta per frame (quando elabora la coppia batch=0, receiver=0, sender=1)
- Evita di contare più volte lo stesso frame per ogni combinazione agente-agente
- Determina la fase corrente chiamando `_phase_for_frame()` con il contatore

#### Step 3: Aggiornamento Posizioni (linee 503-507)
```python
pos_j = _extract_position(trans_matrices, b, j)
pos_i = _extract_position(trans_matrices, b, agent_idx)

bridge.update_position(int(j), pos_j[0], pos_j[1], pos_j[2])
bridge.update_position(int(agent_idx), pos_i[0], pos_i[1], pos_i[2])
```

Invia le posizioni correnti a OMNeT++ per calcolare il delay basato sulla distanza fisica.

#### Step 4: Gestione Buffer (linee 512-524)
```python
buffer_key = f"b{b}_ag{j}"

if buffer_key not in feature_buffer:
    feature_buffer[buffer_key] = []

# Aggiungi payload corrente
feature_buffer[buffer_key].append(current_payload.clone().detach())

# Mantieni buffer di dimensione fissa
if len(feature_buffer[buffer_key]) > MAX_BUFFER_LEN:
    feature_buffer[buffer_key].pop(0)  # Rimuovi il più vecchio
```

**Buffer storico:**
- Ogni agente ha un buffer separato identificato da batch e ID agente
- Il buffer mantiene le ultime 100 feature map
- Serve per recuperare dati ritardati quando il delay è significativo

#### Step 5: Trasmissione e Decisione (linee 526-543)
```python
decision = bridge.transmit(
    topic="feature_tensor",
    sender=int(j),
    receiver=int(agent_idx),
    size_bytes=int(current_payload.element_size() * current_payload.numel()),
    metadata=meta,
)

is_delivered = decision.get("deliver", True)
sim_delay = decision.get("delay_s", 0.0)

# Overlay del profilo temporale
sim_delay = max(float(sim_delay), current_phase.delay_floor_s)
if rng.random() < current_phase.drop_prob:
    is_delivered = False
```

**Meccanismo di overlay:**
1. OMNeT++ calcola il delay basato sulla fisica della rete (distanza, interferenze)
2. Il `delay_floor_s` della fase corrente garantisce un delay minimo
3. Il delay finale è il massimo tra i due: `max(omnet_delay, phase_delay)`
4. La probabilità di drop della fase viene applicata indipendentemente da OMNeT++

#### Step 6: Calcolo del Lag e Recupero Dati (linee 558-595)
```python
frames_lag = int(round(sim_delay * dataset_framerate))

if not is_delivered:
    # CASO 1: Pacchetto perso
    final_payload = torch.zeros_like(current_payload)
    log_msg = "[XXX] PACKET LOST"
    
elif frames_lag == 0:
    # CASO 2: Nessun ritardo significativo
    final_payload = current_payload
    log_msg = "[OK] Live"
    
else:
    # CASO 3: Ritardo significativo
    buffer = feature_buffer[buffer_key]
    target_idx = -1 - frames_lag  # -1 è corrente, -2 è lag=1, etc.
    
    if abs(target_idx) <= len(buffer):
        final_payload = buffer[target_idx]
        log_msg = f"[OLD] Lag: {frames_lag} frames"
    else:
        # Buffer underflow: usa il più vecchio disponibile
        final_payload = buffer[0]
        underflow = True
        log_msg = f"[OLD!] Buffer Underflow"
```

**Conversione delay → lag:**
- Formula: `frames_lag = delay_seconds * framerate_hz`
- Esempio: 0.2s delay × 5 fps = 1 frame di lag
- Se lag=1, recupera il dato di 1 frame fa dal buffer

**Gestione dei casi:**
1. **Packet loss**: Sostituisce i dati con zeri (nessuna informazione)
2. **Live (lag=0)**: Usa i dati correnti
3. **Delayed (lag>0)**: Recupera dati storici dal buffer
4. **Buffer underflow**: Il ritardo richiede dati più vecchi di quelli disponibili, usa il dato più vecchio nel buffer

#### Step 7: Aggiornamento Statistiche (linee 597-607)
```python
network_stats.tx_total += 1
if is_delivered:
    network_stats.delivered += 1
    if frames_lag == 0:
        network_stats.live_packets += 1
    else:
        network_stats.delayed_packets += 1
    if underflow:
        network_stats.underflow_packets += 1
else:
    network_stats.dropped += 1

network_stats.delays_s.append(sim_delay)
network_stats.phase_hits[current_phase.name] = \
    network_stats.phase_hits.get(current_phase.name, 0) + 1
```

Raccoglie metriche per l'analisi post-esecuzione.

---

## Strutture Dati

### Buffer delle Feature

```python
feature_buffer = {
    "b0_ag0": [tensor_0, tensor_1, ..., tensor_99],  # Ultimi 100 frame agente 0
    "b0_ag1": [tensor_0, tensor_1, ..., tensor_99],  # Ultimi 100 frame agente 1
    # ...
}
```

**Organizzazione:**
- Chiave: `f"b{batch}_ag{agent_id}"`
- Valore: Lista FIFO di tensori (index -1 = più recente, index 0 = più vecchio)
- Dimensione massima: 100 frame
- Quando pieno: Rimuove il più vecchio (pop dal front)

### Timeline delle Fasi

```
Frame:  0  10  20  30  40  50  60  70  80  90  100 ... 200 ... 1000
Phase:  [--- good ---][---- medium ----][---- bad ----][-------- worst --------]
Delay:  20ms          80ms              150ms          250ms (persiste)
Loss:   0%            3%                8%             15%
```

---

## Esempi di Utilizzo

### Esempio 1: Esecuzione con Profilo Default

```bash
python test_codet_network.py \
  --com disco \
  --data ~/data/V2X-Sim/test \
  --resume ~/checkpoints/epoch_100.pth \
  --network_host 127.0.0.1 \
  --network_port 5555
```

Usa il profilo di default: degrado progressivo in 70 frame.

### Esempio 2: Profilo Personalizzato

```bash
python test_codet_network.py \
  --com disco \
  --data ~/data/V2X-Sim/test \
  --resume ~/checkpoints/epoch_100.pth \
  --network_host 127.0.0.1 \
  --network_port 5555 \
  --network_profile "excellent:50:0.01:0.00,degraded:50:0.20:0.10,critical:10000:0.50:0.30"
```

Profilo personalizzato con 3 fasi: excellent (50 frame), degraded (50 frame), critical (resto).

### Esempio 3: Rete Costantemente Degradata

```bash
python test_codet_network.py \
  --com disco \
  --data ~/data/V2X-Sim/test \
  --resume ~/checkpoints/epoch_100.pth \
  --network_host 127.0.0.1 \
  --network_port 5555 \
  --network_profile "stable_poor:10000:0.15:0.10"
```

Una singola fase: rete costantemente degradata (150ms, 10% loss) senza variazioni.

### Esempio 4: Disabilitare Completamente la Simulazione di Rete

```bash
python test_codet_network.py \
  --com disco \
  --data ~/data/V2X-Sim/test \
  --resume ~/checkpoints/epoch_100.pth \
  --network_disable
```

Esegue i test senza alcuna simulazione di rete (condizioni ideali).

---

## Vantaggi del Sistema Attuale

1. **Scalabilità**: Funziona con scene di qualsiasi lunghezza (l'ultima fase persiste)
2. **Semplicità**: Configurazione tramite stringa facilmente modificabile
3. **Realismo**: Combina simulazione fisica (OMNeT++) con profili temporali
4. **Flessibilità**: Possibilità di disabilitare o personalizzare completamente
5. **Debugging**: Logging dettagliato per ogni pacchetto (deliver/drop/lag)
6. **Statistiche**: Raccolta automatica di metriche per analisi post-esecuzione

---

## Limitazioni e Considerazioni

1. **Profilo Fisso**: Le fasi sono temporali, non basate sul traffico effettivo
2. **Dipendenza da Framerate**: Il calcolo del lag dipende dal framerate del dataset
3. **Buffer Finito**: Con delay molto elevati, possibile buffer underflow
4. **RNG Seed**: Il comportamento stocastico (packet loss) è riproducibile ma deterministico con lo stesso seed

---

## File Modificati

1. **tools/det/test_codet_network.py**
   - Linee 158-171: `_phase_for_frame()` - rimosso comportamento ciclico
   - Linee 697-707: Default di `--network_profile` - cambiato profilo
   
2. **start_up_procedure.txt**
   - Aggiunta documentazione del profilo di rete
   - Rimossi parametri `--network_profile` obsoleti dai comandi

---

## Riferimenti Tecnici

- **NetworkPhase**: Struttura dati per definire una fase (linee 62-66)
- **_parse_network_profile()**: Parsing da stringa a oggetti (linee 131-155)
- **_phase_for_frame()**: Selezione fase per frame index (linee 158-171)
- **patch_feature_transformation()**: Intercettazione comunicazioni (linee 350+)
- **Gestione buffer**: Storico feature per recovery (linee 512-524)
- **Overlay delay/drop**: Applicazione profilo temporale (linee 544-558)
- **Recovery con lag**: Recupero dati ritardati da buffer (linee 558-595)
