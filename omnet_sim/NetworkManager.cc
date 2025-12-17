#include <omnetpp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <chrono>
#include <algorithm>

using namespace omnetpp;

class NetworkManager : public cSimpleModule
{
  private:
    int server_fd, client_fd;
    int port;
    double packetLoss;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    
    // Funzione per gestire la comunicazione
    void runServerLoop();
};

Define_Module(NetworkManager);

void NetworkManager::initialize()
{
    port = par("port");
    packetLoss = std::clamp(par("packetLoss").doubleValue(), 0.0, 1.0);
    
    // 1. Creazione Socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        throw cRuntimeError("Socket creation failed");
    }

    // Opzioni per riutilizzare la porta subito se crasha
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // 2. Binding
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        throw cRuntimeError("Bind failed. Is the port 5555 already in use?");
    }

    // 3. Listen
    if (listen(server_fd, 1) < 0) {
        throw cRuntimeError("Listen failed");
    }

    EV << "OMNeT++ Server listening on port " << port << "...\n";
    EV << "Waiting for Python Client to connect...\n";
    
    // 4. Accept (Bloccante: OMNeT aspetta qui che tu lanci Python)
    int addrlen = sizeof(address);
    if ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        throw cRuntimeError("Accept failed");
    }
    
    EV << "Python Client connected! Starting simulation loop.\n";

    // Entriamo nel loop di gestione
    runServerLoop();
}

void NetworkManager::runServerLoop()
{
    char buffer[4096] = {0};
    while (true) {
        memset(buffer, 0, 4096);
        int valread = read(client_fd, buffer, 4096);

        if (valread <= 0) {
            EV << "Client disconnected.\n";
            break;
        }

        double simulated_delay = par("delay").doubleValue();
        bool deliver = uniform(0, 1) > packetLoss;

        std::stringstream response;
        if (deliver) {
            auto now = std::chrono::system_clock::now();
            double ready_at_unix = std::chrono::duration_cast<std::chrono::duration<double>>(now.time_since_epoch()).count() + simulated_delay;
            response << "{\"deliver\": true, \"delay_s\": " << simulated_delay
                     << ", \"ready_at\": " << ready_at_unix << "}\n";
            EV << "Processed packet. Simulated delay: " << simulated_delay << "s\n";
        } else {
            response << "{\"deliver\": false}\n";
            EV << "Packet dropped (simulated loss).\n";
        }

        std::string resp_str = response.str();
        ::send(client_fd, resp_str.c_str(), resp_str.length(), 0);
    }
}

void NetworkManager::handleMessage(cMessage *msg)
{
    // Non usato in questo approccio sincrono socket-based
    delete msg;
}

void NetworkManager::finish()
{
    close(client_fd);
    close(server_fd);
}
//commento per testare modifiche