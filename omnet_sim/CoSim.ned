// CoSim.ned
// package omnet_sim;

import inet.node.inet.AdhocHost;
import inet.physicallayer.wireless.common.contract.packetlevel.IRadioMedium;
import inet.visualizer.common.IntegratedVisualizer;
import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;

// Modulo Controller che gestisce la connessione TCP con Python
simple NetworkManager
{
    parameters:
        int port = default(5555);
        @display("i=block/network2");
}

// Definizione della rete con nodi mobili
network CoPerceptionNetwork
{
    parameters:
        int numHosts = default(5); // Numero di veicoli
        @display("bgb=600,600");

    submodules:
        configurator: Ipv4NetworkConfigurator {
            @display("p=50,50");
        }
        // visualizer: IntegratedVisualizer {
        //    @display("p=100,50");
        // }
        radioMedium: <default("Ieee80211ScalarRadioMedium")> like IRadioMedium {
            @display("p=100,100");
        }
        manager: NetworkManager {
            @display("p=300,50");
        }
        host[numHosts]: AdhocHost {
            @display("p=300,300,m,5,50,50");
        }
        
    connections allowunconnected:
        // Le connessioni wireless sono gestite magicamente da radioMedium
}
