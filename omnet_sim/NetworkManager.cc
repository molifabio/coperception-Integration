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

void NetworkManager::initialize()
{
    port = par("port");
    pollInterval = 0.001; // Poll every 1ms

    setupServerSocket();

    // BLOCKING WAIT for Python connection
    std::cout << "NetworkManager: Waiting for Python connection on port " << port << "..." << std::endl;
    EV << "NetworkManager: Waiting for Python connection...\n";

    struct sockaddr_in address;
    int addrlen = sizeof(address);
    // Note: server_fd is blocking by default in setupServerSocket now (see change below)
    if ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        throw cRuntimeError("Accept failed: %s", strerror(errno));
    }

    std::cout << "NetworkManager: Python connected! Starting simulation." << std::endl;
    EV << "NetworkManager: Python connected!\n";

    // Set client socket to non-blocking for the event loop
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    discoverApps();

    // Schedule the first poll
    ticker = new cMessage("socketPoll");
    scheduleAt(simTime() + pollInterval, ticker);
}

void NetworkManager::discoverApps()
{
    appRegistry.clear();

    // Iterate over all modules in the simulation to find CoPerceptionApps
    for (int i = 0; i < getSimulation()->getLastComponentId(); ++i) {
        cModule *mod = getSimulation()->getModule(i);
        if (!mod) continue;

        CoPerceptionApp *app = dynamic_cast<CoPerceptionApp*>(mod);
        if (app) {
            cModule *parent = mod->getParentModule();
            if (parent) {
                std::string key;
                if (parent->isVector()) {
                    key = std::to_string(parent->getIndex());
                } else {
                    key = parent->getFullName();
                }
                
                appRegistry[key] = app;
                EV << "NetworkManager: Registered app for vehicle ID '" << key << "'\n";
                
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

void NetworkManager::acceptClient()
{
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    
    if (new_socket >= 0) {
        client_fd = new_socket;
        int flags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
        
        EV << "NetworkManager: Python Client connected!\n";
        discoverApps();
    }
}

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
                processCommand(line);
            }
        }
    }
}

void NetworkManager::processCommand(const std::string& cmd)
{
    EV << "NetworkManager: Received cmd: " << cmd << "\n";
    
    auto getVal = [&](std::string key) -> std::string {
        std::regex rgx("\"" + key + "\"\\s*:\\s*\"?([^\",}]+)\"?");
        std::smatch match;
        if (std::regex_search(cmd, match, rgx) && match.size() > 1)
            return match[1].str();
        return "";
    };

    std::string type = getVal("type");
    
#include "PythonMobility.h"

// ... (inside processCommand)

    if (type == "move") {
        std::string id = getVal("id");
        std::string xStr = getVal("x");
        std::string yStr = getVal("y");
        std::string zStr = getVal("z");
        
        double x = 0, y = 0, z = 0;
        try { x = std::stod(xStr); y = std::stod(yStr); z = std::stod(zStr); } catch(...) {}

        if (appRegistry.find(id) != appRegistry.end()) {
            CoPerceptionApp* app = appRegistry[id];
            cModule* host = app->getParentModule();
            cModule* mobilityMod = host->getSubmodule("mobility");
            PythonMobility* mob = dynamic_cast<PythonMobility*>(mobilityMod);
            
            if (mob) {
                mob->setPosition(x, y, z);
                EV << "NetworkManager: Moved node " << id << " to (" << x << ", " << y << ", " << z << ")\n";
            } else {
                EV << "NetworkManager: Node " << id << " does not have PythonMobility!\n";
            }
        }
    }
    
    if (type == "send") {
        std::string srcId = getVal("src");
        std::string dstId = getVal("dst");
        std::string sizeStr = getVal("size");
        std::string msgId = getVal("id");
        
        long sizeBytes = 1000;
        if (!sizeStr.empty()) {
            try { sizeBytes = std::stol(sizeStr); } catch(...) {}
        }

        if (appRegistry.find(srcId) != appRegistry.end()) {
            CoPerceptionApp* srcApp = appRegistry[srcId];
            
            std::string dstName = dstId; 
            if (appRegistry.find(dstId) != appRegistry.end()) {
                 cModule* dstMod = appRegistry[dstId]->getParentModule();
                 if (dstMod->isVector()) {
                     dstName = std::string(dstMod->getName()) + "[" + std::to_string(dstMod->getIndex()) + "]";
                 } else {
                     dstName = dstMod->getFullName();
                 }
            }
            
            srcApp->sendDataPacket(dstName.c_str(), sizeBytes, msgId.c_str());
        } else {
            EV << "NetworkManager: Source " << srcId << " not found!\n";
        }
    }
}

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
