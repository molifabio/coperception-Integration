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
    Enter_Method_Silent();

    long maxChunkSize = 60000; // 60KB safe limit (UDP max is 65535 - headers)
    long remainingBytes = sizeBytes;
    int fragmentCount = 0;

    // Resolve destination once
    L3Address destAddr;
    try {
        destAddr = L3AddressResolver().resolve(destAddrStr);
    } catch (std::exception& e) {
        EV << "CoPerceptionApp: Could not resolve address: " << destAddrStr << "\n";
        return;
    }

    if (destAddr.isUnspecified()) {
        EV << "CoPerceptionApp: Resolved address is unspecified for: " << destAddrStr << "\n";
        return;
    }

    // Fragment loop
    while (remainingBytes > 0) {
        long currentChunkSize = (remainingBytes > maxChunkSize) ? maxChunkSize : remainingBytes;
        
        // Create packet
        Packet *packet = new Packet(msgId);
        
        // Add dummy payload
        const auto& payload = makeShared<ByteCountChunk>(B(currentChunkSize));
        packet->insertAtBack(payload);

        // Send via UdpSocket
        socket.sendTo(packet, destAddr, destPort);

        remainingBytes -= currentChunkSize;
        fragmentCount++;
    }
    
    EV << "CoPerceptionApp: Sent " << msgId << " in " << fragmentCount << " fragments to " << destAddrStr << "\n";
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
