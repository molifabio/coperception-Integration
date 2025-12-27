// CoSim.ned

// Definizione del modulo che gestirà la connessione TCP
simple NetworkManager
{
    parameters:
    int port = default(5555); // La porta deve corrispondere a quella Python
    double packetLoss = default(0); // Probabilità di perdita
	// Modello di ritardo: base + distanza*coeff + size*coeff + jitter
	volatile double delayBase @unit(s) = default(0.05s);
	volatile double delayPerMeter @unit(s) = default(0s);
	volatile double delayPerByte @unit(s) = default(0s);
	volatile double delayJitter @unit(s) = default(0s);
    @display("i=block/server");
}

// Definizione della rete generale
network CoPerceptionNetwork
{
    submodules:
        manager: NetworkManager;
}
