#ifndef COPERCEPTIONAPP_H
#define COPERCEPTIONAPP_H

#include <omnetpp.h>
#include "inet/applications/base/ApplicationBase.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"

using namespace omnetpp;
using namespace inet;

class NetworkManager; // Forward declaration

class CoPerceptionApp : public ApplicationBase, public UdpSocket::ICallback
{
  protected:
    int localPort;
    int destPort;
    NetworkManager* manager = nullptr;
    UdpSocket socket;

  public:
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;
    
    // Lifecycle methods required by OperationalMixin
    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;

    // UdpSocket::ICallback methods
    virtual void socketDataArrived(UdpSocket *socket, Packet *packet) override;
    virtual void socketErrorArrived(UdpSocket *socket, Indication *indication) override;
    virtual void socketClosed(UdpSocket *socket) override;

    // Called by NetworkManager to trigger sending
    void sendDataPacket(const char* destAddrStr, long sizeBytes, const char* msgId);
    
    void registerManager(NetworkManager* m) { manager = m; }
};

#endif
