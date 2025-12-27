#include <omnetpp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <regex>
#include <chrono>
#include <algorithm>

using namespace omnetpp;

class NetworkManager : public cSimpleModule
{
  private:
    int server_fd, client_fd;
    int port;
    double packetLoss;
        double delayBase;
        double delayPerMeter;
        double delayJitter;
        double delayPerByte;

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
    double val = par("packetLoss").doubleValue();
    if (val < 0.0) val = 0.0;
    if (val > 1.0) val = 1.0;
    packetLoss = val;
    delayBase = par("delayBase").doubleValue();
    delayPerMeter = par("delayPerMeter").doubleValue();
    delayJitter = par("delayJitter").doubleValue();
    delayPerByte = par("delayPerByte").doubleValue();
    
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

    auto extractDouble = [](const std::string &s, const std::string &key, double def) {
        try {
            std::regex rgx("\"" + key + "\"\\s*:\\s*([-+]?([0-9]*[.])?[0-9]+([eE][-+]?[0-9]+)?)");
            std::smatch match;
            if (std::regex_search(s, match, rgx) && match.size() > 1)
                return std::stod(match[1]);
        } catch (...) {}
        return def;
    };

    while (true) {
        memset(buffer, 0, 4096);
        int valread = read(client_fd, buffer, 4096);

        if (valread <= 0) {
            EV << "Client disconnected.\n";
            break;
        }

        std::string payload(buffer, valread);

        double distance_m = extractDouble(payload, "distance_m", -1.0);
        double size_bytes = extractDouble(payload, "size_bytes", 0.0);

        // Base delay + distance contribution + size contribution + jitter
        double simulated_delay = delayBase;
        if (distance_m > 0.0)
            simulated_delay += distance_m * delayPerMeter;
        if (size_bytes > 0.0)
            simulated_delay += size_bytes * delayPerByte;
        if (delayJitter != 0.0)
            simulated_delay += uniform(-fabs(delayJitter), fabs(delayJitter));

        if (simulated_delay < 0.0)
            simulated_delay = 0.0;
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