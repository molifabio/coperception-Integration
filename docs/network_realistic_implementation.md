# Implementazione di un Profilo di Rete Realistico Basato sul Traffico

## Concetto

Un profilo di rete dinamico che degrada in base al traffico effettivo della rete sarebbe più realistico rispetto a un profilo temporale fisso. In particolare:

- Quando più veicoli sono vicini e comunicano, il carico di rete aumenta causando congestione
- Quando i veicoli sono distanti e non scambiano dati, la rete non dovrebbe degradarsi
- Il degrado dovrebbe dipendere dal numero di pacchetti attualmente in transito

## Requisiti per l'Implementazione

### 1. Tracciamento del Traffico
- Contare in tempo reale quanti pacchetti sono in transito tra veicoli
- Misurare la banda occupata istantaneamente
- Mantenere uno stato del carico di rete corrente

### 2. Modifica del Bridge OMNeT++
- Applicare delay e packet loss dinamicamente in base al carico misurato
- Implementare un modello di congestione (es: `delay = base_delay * (1 + packets/bandwidth)`)
- Aggiornare la logica di `OmnetBridge` per supportare parametri variabili nel tempo

### 3. Informazioni dalla Simulazione
- Conoscere quanti veicoli sono nel raggio di comunicazione in ogni frame
- Accedere alle matrici di trasformazione per calcolare distanze
- Determinare quali coppie di veicoli stanno comunicando attivamente

### 4. Modello di Congestione
- Formula matematica per calcolare il degrado basato su:
  - Numero di nodi attivi
  - Numero di pacchetti simultanei
  - Banda disponibile
  - Distanze tra nodi

## Problemi e Complessità

### Duplicazione di Funzionalità
**OMNeT++ già implementa la simulazione di congestione della rete.** La simulazione fisica del canale wireless in OMNeT++ include:
- Modelli di propagazione del segnale
- Interferenza tra trasmissioni simultanee
- Congestione del canale basata sul numero di nodi
- Collision detection e backoff

### Architettura Attuale
Il bridge Python attuale (`OmnetBridge`) è un wrapper semplificato che:
- Bypassa parte della simulazione fisica di OMNeT++
- Applica delay e drop in modo deterministico
- Non ha accesso completo allo stato interno di OMNeT++

### Complessità di Implementazione
Richiederebbe:
- Riscrittura sostanziale dell'interazione bridge-OMNeT++
- Esposizione di metriche di rete da OMNeT++ verso Python
- Sincronizzazione bidirezionale degli stati
- Testing approfondito per evitare race condition

## Soluzione Alternativa Raccomandata

### Configurazione OMNeT++ Nativa

Invece di implementare la congestione lato Python, configurare OMNeT++ per simulare realisticamente la congestione:

1. **File `omnetpp.ini`**: Definire modelli di congestione del canale wireless
2. **Parametri INET**: Configurare il modulo MAC per gestire collisioni e backoff
3. **Modelli di propagazione**: Usare modelli realistici (es: Two-Ray Ground, Log-Distance)
4. **Buffer e code**: Configurare dimensioni dei buffer e politiche di drop

Esempio di configurazione in `omnetpp.ini`:
```ini
# Modello di propagazione realistico
*.radioMedium.pathLossType = "TwoRayGroundReflection"

# Congestione basata su CSMA/CA
*.*.wlan[*].mac.typename = "Ieee80211Mac"
*.*.wlan[*].mac.dcf.channelAccess.cwMin = 15
*.*.wlan[*].mac.dcf.channelAccess.cwMax = 1023

# Buffer limitati per simulare packet loss sotto carico
*.*.wlan[*].mac.dcf.txQueue.packetCapacity = 50
```

### Vantaggi dell'Approccio OMNeT++
- Sfrutta la simulazione fisica già implementata
- Più accurato dal punto di vista della teoria delle reti
- Non richiede modifiche al codice Python
- Scalabile e testato

## Conclusione

Per il progetto corrente, il profilo progressivo fisso è appropriato e dimostra la capacità del sistema di adattarsi a condizioni di rete variabili. L'implementazione di un modello dinamico basato sul traffico sarebbe un'estensione futura significativa che richiederebbe:
- Integrazione più profonda con OMNeT++
- Validazione contro scenari reali
- Confronto con modelli di congestione standard (es: RED, CoDel)

La configurazione di OMNeT++ stesso rimane l'approccio più efficace per simulare congestione realistica senza modificare l'architettura corrente.
