#include "PythonMobility.h"

Define_Module(PythonMobility);

void PythonMobility::initialize(int stage)
{
    MovingMobilityBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        // Initialize at 0,0,0 or parameter
        lastPosition.x = par("initialX");
        lastPosition.y = par("initialY");
        lastPosition.z = par("initialZ");
        lastVelocity = Coord(0, 0, 0);
        lastOrientation = Quaternion(0, 0, 0, 1); // Identity
    }
}

void PythonMobility::move()
{
    // Do nothing automatically. We move only when setPosition is called.
}

void PythonMobility::setPosition(double x, double y, double z)
{
    lastPosition.x = x;
    lastPosition.y = y;
    lastPosition.z = z;
    
    // Update visualization and notify other modules (radio)
    updateDisplayStringFromMobilityState();
    emitMobilityStateChangedSignal();
}
