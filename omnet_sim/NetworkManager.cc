#include "NetworkManager.h"
#include "CoPerceptionApp.h"
#include "PythonMobility.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <regex>
#include <fcntl.h>
#include <errno.h>

Define_Module(NetworkManager);

/**
 * NetworkManager: Il ponte centrale tra Python (Deep Learning) e OMNeT++ (Network Simulation).
 * 
 * Responsabilità:
 * 1. Server TCP: Accetta la connessione dallo script Python.
 * 2. Sincronizzazione: Blocca l'avvio della simulazione finché Python non è connesso.
 * 3. Orchestrazione: Riceve comandi JSON ("move", "send") e li smista ai moduli corretti.
 * 4. Feedback: Raccoglie le notifiche di ricezione pacchetti e le invia a Python.
 */
void NetworkManager::initialize()
{
    port = par("port");
    pollInterval = 0.001; // Poll every 1ms

    setupServerSocket();

    // --- FASE 1: HANDSHAKE SINCRONO ---
    // Blocchiamo l'intera simulazione qui finché Python non si connette.
    // Questo previene che OMNeT++ parta "a vuoto" prima che il modello DL sia pronto.
    std::cout << "NetworkManager: Waiting for Python connection on port " << port << "..." << std::endl;
    EV << "NetworkManager: Waiting for Python connection...\n";

    struct sockaddr_in address;
    int addrlen = sizeof(address);
    // accept() è bloccante qui (default dei socket).
    if ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        throw cRuntimeError("Accept failed: %s", strerror(errno));
    }

    std::cout << "NetworkManager: Python connected! Starting simulation." << std::endl;
    EV << "NetworkManager: Python connected!\n";

    // --- FASE 2: LOOP ASINCRONO ---
    // Ora che siamo connessi, impostiamo il socket come NON BLOCCANTE.
    // Da ora in poi, leggeremo i comandi nel metodo handleMessage() senza fermare la simulazione.
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    discoverApps();

    // Avvia il polling periodico del socket
    // ogni 1ms viene chiamato handleMessage()
    ticker = new cMessage("socketPoll");
    scheduleAt(simTime() + pollInterval, ticker);
}

/**
 * Scansiona l'intera simulazione per trovare tutti i moduli "CoPerceptionApp".
 * 
 * 1. Itera su tutti i componenti della simulazione.
 * 2. Se trova un modulo di tipo CoPerceptionApp, lo registra in una mappa (appRegistry).
 * 3. La chiave della mappa è l'ID del veicolo .
 * 4. Chiama app->registerManager(this) .
 */
void NetworkManager::discoverApps()
{
    appRegistry.clear();

    // Itera su tutti i moduli istanziati nella simulazione (fino all'ultimo ID assegnato)
    for (int i = 0; i < getSimulation()->getLastComponentId(); ++i) {
        cModule *mod = getSimulation()->getModule(i);
        if (!mod) continue;

        // Controlla se il modulo è una CoPerceptionApp
        CoPerceptionApp *app = dynamic_cast<CoPerceptionApp*>(mod);
        if (app) {
            // l'app è dentro un modulo host (il veicolo).
            cModule *parent = mod->getParentModule();
            if (parent) {
                std::string key;
                // Se il parent è un vettore di moduli (es. node[0], node[1]...), usiamo l'indice come ID.
                if (parent->isVector()) {
                    key = std::to_string(parent->getIndex());
                } else {
                    // Altrimenti usiamo il nome completo (es. "rsu")
                    key = parent->getFullName();
                }
                
                // Registra l'app nella mappa per accesso rapido futuro
                appRegistry[key] = app;
                EV << "NetworkManager: Registered app for vehicle ID '" << key << "'\n";
                
                // Link bidirezionale: L'app deve conoscere il manager per inviare notifiche
                app->registerManager(this);
            }
        }
    }
}

void NetworkManager::setupServerSocket()
{
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        throw cRuntimeError("Socket creation failed: %s", strerror(errno));
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    // Set non-blocking REMOVED - we want blocking accept in initialize
    // int flags = fcntl(server_fd, F_GETFL, 0);
    // fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        throw cRuntimeError("Bind failed: %s", strerror(errno));
    }

    if (listen(server_fd, 1) < 0) {
        throw cRuntimeError("Listen failed: %s", strerror(errno));
    }

    // Print to std::cout so it's visible in Cmdenv console immediately
    std::cout << "NetworkManager: Listening on port " << port << " (Non-blocking)" << std::endl;
    EV << "NetworkManager: Listening on port " << port << " (Non-blocking)\n";
}

void NetworkManager::handleMessage(cMessage *msg)
{
    if (msg == ticker) {
        // Client is already connected in initialize
        if (client_fd >= 0) {
            readFromSocket();
        }

        scheduleAt(simTime() + pollInterval, ticker);
    } 
    else {
        delete msg;
    }
}

/**
 * Legge dal socket TCP in modo non bloccante.
 * Accumula i dati nel buffer e processa le righe complete (JSON).
 */
void NetworkManager::readFromSocket()
{
    char buffer[4096] = {0};
    int valread = read(client_fd, buffer, 4096);

    if (valread > 0) {
        std::string payload(buffer, valread);
        std::stringstream ss(payload);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty()) {
                // Processa ogni linea come comando JSON
                processCommand(line);
            }
        }
    }
}

/**
 * Esegue il comando ricevuto da Python.
 * Supporta:
 * - "move": Aggiorna la posizione di un nodo tramite PythonMobility.
 * - "send": Ordina a un nodo di inviare un pacchetto UDP.
 */
void NetworkManager::processCommand(const std::string& cmd)
{
    EV << "NetworkManager: Received cmd: " << cmd << "\n";
    
    // Parsing manuale molto semplice del JSON (per evitare dipendenze esterne pesanti)
    // Cerca "key": "value"
    auto getVal = [&](std::string key) -> std::string {
        std::regex rgx("\"" + key + "\"\\s*:\\s*\"?([^\",}]+)\"?");
        std::smatch match;
        if (std::regex_search(cmd, match, rgx) && match.size() > 1)
            return match[1].str();
        return "";
    };

    std::string type = getVal("type");
    
    if (type == "move") {
        // Comando: Sposta un veicolo
        std::string id = getVal("id");
        std::string xStr = getVal("x");
        std::string yStr = getVal("y");
        std::string zStr = getVal("z");
        
        double x = 0, y = 0, z = 0;


        // cast
        try { 
            x = std::stod(xStr); 
            y = std::stod(yStr); 
            z = std::stod(zStr); 
        } catch(...) {}

        if (appRegistry.find(id) != appRegistry.end()) {
            CoPerceptionApp* app = appRegistry[id];
            cModule* host = app->getParentModule();
            // Cerca il modulo di mobilità associato all'ID
            cModule* mobilityMod = host->getSubmodule("mobility");
            PythonMobility* mob = dynamic_cast<PythonMobility*>(mobilityMod);
            
            if (mob) {
                // Aggiorna la posizione fisica nel simulatore
                mob->setPosition(x, y, z);
                EV << "NetworkManager: Moved node " << id << " to (" << x << ", " << y << ", " << z << ")\n";
            } else {
                EV << "NetworkManager: Node " << id << " does not have PythonMobility!\n";
            }
        }
    }
    
    if (type == "send") {
        // Comando: Invia pacchetto dati
        std::string srcId = getVal("src");
        std::string dstId = getVal("dst");

        if (srcId.empty() || dstId.empty()) {
             EV << "NetworkManager: Missing src or dst in send command: " << cmd << "\n";
             return;
        }
        std::string sizeStr = getVal("size");
        std::string msgId = getVal("id");
        
        long sizeBytes = 1000;
        if (!sizeStr.empty()) {
            try { 
                sizeBytes = std::stol(sizeStr); 
            } catch(...) {}
        }

        // controlla che la sorgente esista
        if (appRegistry.find(srcId) != appRegistry.end()) {
            CoPerceptionApp* srcApp = appRegistry[srcId];
            
            // Risolvi il nome completo del modulo di destinazione per INET
            std::string dstName = dstId; 
            // Se il dstId è un indice , trova il nome completo
            if (appRegistry.find(dstId) != appRegistry.end()) {
                 cModule* dstMod = appRegistry[dstId]->getParentModule();
                 if (dstMod->isVector()) {
                     dstName = std::string(dstMod->getName()) + "[" + std::to_string(dstMod->getIndex()) + "]";
                 } else {
                     dstName = dstMod->getFullName();
                 }
            }
            
            // Ordina all'applicazione del veicolo sorgente di inviare il pacchetto
            srcApp->sendDataPacket(dstName.c_str(), sizeBytes, msgId.c_str());
        } else {
            EV << "NetworkManager: Source " << srcId << " not found!\n";
        }
    }
}

/**
 * Callback chiamata da CoPerceptionApp quando un pacchetto arriva a destinazione.
 * Invia la conferma a Python: {"type": "received", "delay": 0.123, ...}
 */
void NetworkManager::notifyReception(const char* msgId, double delay, bool success)
{
    std::stringstream ss;
    ss << "{\"type\": \"received\", \"id\": \"" << msgId << "\", \"delay\": " << delay << ", \"deliver\": " << (success ? "true" : "false") << "}";
    sendToPython(ss.str());
}

void NetworkManager::sendToPython(const std::string& json)
{
    if (client_fd >= 0) {
        std::string msg = json + "\n";
        ::send(client_fd, msg.c_str(), msg.length(), 0);
    }
}

void NetworkManager::finish()
{
    if (client_fd >= 0) close(client_fd);
    if (server_fd >= 0) close(server_fd);
    cancelAndDelete(ticker);
}
