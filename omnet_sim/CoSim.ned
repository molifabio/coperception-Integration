// CoSim.ned

// Definizione del modulo che gestirà la connessione TCP
simple NetworkManager
{
    parameters:
        int port = default(5555); // La porta deve corrispondere a quella Python
	volatile double delay @unit(s) = default(0.05s);
    double packetLoss = default(0); // Probabilità di perdita 0..1
        @display("i=block/server");
}

// Definizione della rete generale
network CoPerceptionNetwork
{
    submodules:
        manager: NetworkManager;
}
