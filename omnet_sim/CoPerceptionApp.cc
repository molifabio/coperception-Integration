#include "CoPerceptionApp.h"
#include "NetworkManager.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/common/lifecycle/ModuleOperations.h"

Define_Module(CoPerceptionApp);

void CoPerceptionApp::initialize(int stage)
{
    ApplicationBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        localPort = par("localPort");
        destPort = par("destPort");
        socket.setOutputGate(gate("socketOut"));
        socket.setCallback(this);
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        // socket.bind(localPort); // Removed explicit bind here as it might conflict with lifecycle
    }
}

void CoPerceptionApp::handleMessageWhenUp(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        delete msg;
    } else {
        socket.processMessage(msg);
    }
}

void CoPerceptionApp::sendDataPacket(const char* destAddrStr, long sizeBytes, const char* msgId)
{
    // Create packet
    Packet *packet = new Packet(msgId);
    
    // Add dummy payload
    const auto& payload = makeShared<ByteCountChunk>(B(sizeBytes));
    packet->insertAtBack(payload);

    // Resolve destination
    L3Address destAddr;
    try {
        destAddr = L3AddressResolver().resolve(destAddrStr);
    } catch (std::exception& e) {
        EV << "CoPerceptionApp: Could not resolve address: " << destAddrStr << "\n";
        delete packet;
        return;
    }

    // Send via UdpSocket
    EV << "CoPerceptionApp: Sending packet " << msgId << " to " << destAddrStr << " (" << destAddr << ")\n";
    socket.sendTo(packet, destAddr, destPort);
}

void CoPerceptionApp::socketDataArrived(UdpSocket *socket, Packet *packet)
{
    EV << "CoPerceptionApp: Received packet " << packet->getName() << "\n";
    
    if (manager) {
        // Calculate delay
        simtime_t creationTime = packet->getCreationTime();
        simtime_t now = simTime();
        double delay = (now - creationTime).dbl();
        
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
