// package omnet_sim;

import inet.applications.contract.IApp;

simple CoPerceptionApp like IApp
{
    parameters:
        int localPort = default(5000);
        int destPort = default(5000);
        @display("i=block/app");
    gates:
        input socketIn;
        output socketOut;
}
