package omnet_sim;

import inet.mobility.base.MovingMobilityBase;

simple PythonMobility extends MovingMobilityBase
{
    parameters:
        @class(PythonMobility);
        double initialX @unit(m) = default(0m);
        double initialY @unit(m) = default(0m);
        double initialZ @unit(m) = default(0m);
}
