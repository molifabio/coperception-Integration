#include "CoPerceptionApp.h"
#include "NetworkManager.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/common/lifecycle/ModuleOperations.h"

Define_Module(CoPerceptionApp);

/**
 * CoPerceptionApp: L'applicazione di rete che gira su OGNI veicolo simulato.
 * 
 * Responsabilità:
 * 1. Gestione UDP: Usa INET UdpSocket per inviare e ricevere pacchetti reali.
 * 2. Frammentazione: Spezza i pacchetti giganti (Feature Maps) in chunk UDP gestibili.
 * 3. Misurazione Ritardo: Calcola il tempo di volo (Arrivo - Creazione) e lo notifica al Manager.
 */
void CoPerceptionApp::initialize(int stage)
{
    // INET usa un'inizializzazione a più stadi.
    ApplicationBase::initialize(stage);
    
    if (stage == INITSTAGE_LOCAL) {
        // Fase 1: Lettura parametri e setup base
        localPort = par("localPort");
        destPort = par("destPort");
        
        // Collega il socket UDP al gate di uscita del modulo
        socket.setOutputGate(gate("socketOut"));
        // Imposta questa classe come gestore degli eventi del socket (callback)
        socket.setCallback(this);
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        // Fase 2: Binding (spostato in handleStartOperation per compatibilità Lifecycle)
    }
}

/**
 * Gestore principale dei messaggi quando il nodo è "UP" (acceso).
 * Smista i messaggi al socket UDP di INET.
 */
void CoPerceptionApp::handleMessageWhenUp(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        // Timer interni (non usati per ora)
        delete msg;
    } else {
        // Messaggio dalla rete (pacchetto UDP in arrivo)
        // Passa il messaggio al socket UDP per la gestione
        // Questo chiamerà socketDataArrived quando arriva un pacchetto
        socket.processMessage(msg);
    }
}

/**
 * Invia un pacchetto dati simulato (Feature Map) a un altro veicolo.
 * 
 * @param destAddrStr Indirizzo/Nome del destinatario (es. "node[1]")
 * @param sizeBytes Dimensione totale del payload (es. 1MB)
 * @param msgId ID univoco del messaggio (es. "0->1") per tracciamento
 */
void CoPerceptionApp::sendDataPacket(const char* destAddrStr, long sizeBytes, const char* msgId)
{
    Enter_Method_Silent(); // Necessario perché chiamato dall'esterno (NetworkManager)

    // Limite sicuro per UDP (MTU Ethernet è 1500, ma IP supporta fino a 64KB frammentato)
    // Usiamo 60KB per stare sicuri sotto il limite di 65535 byte dell'header IP length.
    long maxChunkSize = 60000; 
    long remainingBytes = sizeBytes;
    int fragmentCount = 0;

    // Risoluzione indirizzo IP del destinatario tramite INET
    L3Address destAddr;
    try {
        destAddr = L3AddressResolver().resolve(destAddrStr);
    } catch (std::exception& e) {
        EV << "CoPerceptionApp: Could not resolve address: " << destAddrStr << "\n";
        return;
    }

    // Loop di frammentazione: invia N pacchetti UDP per simulare il carico di rete
    while (remainingBytes > 0) {
        long currentChunkSize = (remainingBytes > maxChunkSize) ? maxChunkSize : remainingBytes;
        
        // Crea il pacchetto INET
        Packet *packet = new Packet(msgId);
        
        // Aggiunge un payload fittizio della dimensione richiesta (ByteCountChunk non occupa RAM reale)
        const auto& payload = makeShared<ByteCountChunk>(B(currentChunkSize));
        packet->insertAtBack(payload);

        // Invia tramite socket UDP, dopo la simulazione, verrà gestito da handleMessageWhenUp sul veicolo destinatario
        socket.sendTo(packet, destAddr, destPort);

        remainingBytes -= currentChunkSize;
        fragmentCount++;
    }
    
    EV << "CoPerceptionApp: Sent " << msgId << " in " << fragmentCount << " fragments to " << destAddrStr << "\n";
}

/**
 * Callback chiamata da INET quando arriva un pacchetto UDP.
 */
void CoPerceptionApp::socketDataArrived(UdpSocket *socket, Packet *packet)
{
    EV << "CoPerceptionApp: Received packet " << packet->getName() << "\n";
    
    if (manager) {
        // Calcola il ritardo End-to-End
        simtime_t creationTime = packet->getCreationTime(); // Quando è stato creato dal mittente
        simtime_t now = simTime();                          // Adesso
        double delay = (now - creationTime).dbl();
        
        // Notifica il NetworkManager (che lo dirà a Python)
        manager->notifyReception(packet->getName(), delay, true);
    }
    
    delete packet;
}

void CoPerceptionApp::socketErrorArrived(UdpSocket *socket, Indication *indication)
{
    EV << "CoPerceptionApp: Socket error arrived\n";
    delete indication;
}

void CoPerceptionApp::socketClosed(UdpSocket *socket)
{
    EV << "CoPerceptionApp: Socket closed\n";
}

void CoPerceptionApp::handleStartOperation(LifecycleOperation *operation)
{
    socket.bind(localPort);
}

void CoPerceptionApp::handleStopOperation(LifecycleOperation *operation)
{
    socket.close();
}

void CoPerceptionApp::handleCrashOperation(LifecycleOperation *operation)
{
    socket.destroy();
}

void CoPerceptionApp::finish()
{
    ApplicationBase::finish();
}
