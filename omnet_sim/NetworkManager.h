#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <omnetpp.h>
#include <map>
#include <string>

using namespace omnetpp;

class CoPerceptionApp; // Forward declaration

class NetworkManager : public cSimpleModule
{
  private:
    int server_fd = -1;
    int client_fd = -1;
    int port;
    cMessage *ticker = nullptr;
    simtime_t pollInterval;

    // Registry: "0" -> App*, "1" -> App*
    std::map<std::string, CoPerceptionApp*> appRegistry;

    void setupServerSocket();
    void acceptClient();
    void readFromSocket();
    void processCommand(const std::string& cmd);
    void discoverApps();
    void sendToPython(const std::string& json);

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

  public:
    // Called by CoPerceptionApp when a packet is received
    void notifyReception(const char* msgId, double delay, bool success);
};

#endif
