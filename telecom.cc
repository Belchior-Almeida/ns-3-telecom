#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

#include <iostream>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ns3TelecomIoT");

int main(int argc, char *argv[])
{
    // -----------------------------
    // PARÂMETROS CONFIGURÁVEIS
    // -----------------------------
    uint32_t nSensors       = 30;    // entre 20 e 40
    double   simTime        = 30.0;  // duração mínima 30 s
    double   txPower        = 16.0;  // dBm
    double   packetInterval = 1.0;   // s (intervalo entre pacotes)
    uint32_t packetSize     = 40;    // bytes (tamanho típico de sensor)
    double   areaSize       = 100.0; // lado do quadrado (m)
    double   pathLossExp    = 3.0;   // expoente do modelo LogDistance

    CommandLine cmd;
    cmd.AddValue("nSensors",       "Number of IoT sensor nodes", nSensors);
    cmd.AddValue("simTime",        "Simulation time in seconds", simTime);
    cmd.AddValue("txPower",        "Transmit power in dBm", txPower);
    cmd.AddValue("packetInterval", "Packet generation interval (s)", packetInterval);
    cmd.AddValue("packetSize",     "Packet size in bytes", packetSize);
    cmd.AddValue("areaSize",       "Side of square area (m)", areaSize);
    cmd.AddValue("pathLossExp",    "LogDistance path loss exponent", pathLossExp);
    cmd.Parse(argc, argv);


    // Garante limite solicitado (20–40)
    if (nSensors < 20 || nSensors > 40)
    {
        std::cout << "[AVISO] nSensors deve estar entre 20 e 40. Ajustando para 30.\n";
        nSensors = 30;
    }

    // -----------------------------
    // CRIAÇÃO DOS NÓS
    // -----------------------------
    NodeContainer sinkNode;
    sinkNode.Create(1); // nó coletor

    NodeContainer sensorNodes;
    sensorNodes.Create(nSensors); // sensores IoT

    NodeContainer allNodes;
    allNodes.Add(sinkNode);
    allNodes.Add(sensorNodes);

    // -----------------------------
    // FÍSICO + CANAL (Wi-Fi genérico)
    // -----------------------------
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                               "Exponent", DoubleValue(pathLossExp)); // configurável

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Ajuste de potência de transmissão (parâmetro comparado)
    phy.Set("TxPowerStart", DoubleValue(txPower));
    phy.Set("TxPowerEnd",   DoubleValue(txPower));

    WifiHelper wifi;
    // NÃO usamos wifi.SetStandard() porque sua versão não expõe essas constantes

    // Manager simples com taxa fixa (modo clássico OFDM, bem suportado)
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",   StringValue("OfdmRate24Mbps"),
                                 "ControlMode", StringValue("OfdmRate24Mbps"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("IOT-WIFI-NET");

    NetDeviceContainer sinkDevice;
    NetDeviceContainer sensorDevices;

    // Nó coletor como Access Point (AP)
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    sinkDevice = wifi.Install(phy, mac, sinkNode);

    // Sensores como estações (STA), sem probing ativo
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    sensorDevices = wifi.Install(phy, mac, sensorNodes);

    // -----------------------------
    // MOBILIDADE (topologia estática aleatória)
    // -----------------------------
    MobilityHelper mobility;

    // Posicionamento aleatório uniforme em uma área quadrada de lado areaSize
    std::ostringstream xRange, yRange;
    xRange << "ns3::UniformRandomVariable[Min=0.0|Max=" << areaSize << "]";
    yRange << "ns3::UniformRandomVariable[Min=0.0|Max=" << areaSize << "]";

    mobility.SetPositionAllocator(
        "ns3::RandomRectanglePositionAllocator",
        "X", StringValue(xRange.str()),
        "Y", StringValue(yRange.str()));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // Opcional: fixa o sink no centro (50,50)
    Ptr<MobilityModel> sinkMobility = sinkNode.Get(0)->GetObject<MobilityModel>();
    sinkMobility->SetPosition(Vector(areaSize / 2.0, areaSize / 2.0, 0.0));

    // -----------------------------
    // PILHA IP
    // -----------------------------
    InternetStackHelper internet;
    internet.Install(allNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces =
        ipv4.Assign(NetDeviceContainer(sinkDevice, sensorDevices));

    Ipv4Address sinkAddress = interfaces.GetAddress(0); // primeiro é o sink

    // -----------------------------
    // APLICAÇÕES (UDP: sensores -> sink)
    // -----------------------------
    uint16_t port = 5000;

    // Servidor UDP no sink
    UdpServerHelper serverHelper(port);
    ApplicationContainer serverApp = serverHelper.Install(sinkNode.Get(0));
    serverApp.Start(Seconds(0.5));
    serverApp.Stop(Seconds(simTime));

    // Clientes UDP nos sensores
    ApplicationContainer clientApps;

    for (uint32_t i = 0; i < nSensors; ++i)
    {
        UdpClientHelper clientHelper(sinkAddress, port);
        // Máximo de pacotes (depende da simTime / interval)
        uint32_t maxPackets = static_cast<uint32_t>(simTime / packetInterval) + 10;

        clientHelper.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        clientHelper.SetAttribute("Interval",   TimeValue(Seconds(packetInterval)));
        clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));

        ApplicationContainer app = clientHelper.Install(sensorNodes.Get(i));
        app.Start(Seconds(1.0 + i * 0.01)); // pequenos offsets entre nós
        app.Stop(Seconds(simTime));
        clientApps.Add(app);
    }

    // -----------------------------
    // FLOWMONITOR PARA MÉTRICAS
    // -----------------------------
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();

    double totalTxPackets   = 0.0;
    double totalRxPackets   = 0.0;
    double delaySumSeconds  = 0.0;
    double totalRxBytes     = 0.0;

    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (const auto &flow : stats)
    {
        const FlowMonitor::FlowStats &st = flow.second;
        totalTxPackets   += st.txPackets;
        totalRxPackets   += st.rxPackets;
        delaySumSeconds  += st.delaySum.GetSeconds();
        totalRxBytes     += st.rxBytes;
    }

    double pdr            = 0.0;
    double avgDelay       = 0.0;
    double throughputKbps = 0.0;

    if (totalTxPackets > 0)
    {
        pdr = (totalRxPackets / totalTxPackets) * 100.0;
    }

    if (totalRxPackets > 0)
    {
        avgDelay = delaySumSeconds / totalRxPackets; // em segundos
    }

    if (simTime > 0)
    {
        throughputKbps = (totalRxBytes * 8.0) / (simTime * 1000.0); // kbps
    }

    // -----------------------------
    // IMPRESSÃO DOS RESULTADOS
    // -----------------------------
    std::cout << "========== RESULTADOS ==========\n";
    std::cout << "Sensores:            " << nSensors << "\n";
    std::cout << "Tempo de simulacao:  " << simTime << " s\n";
    std::cout << "TxPower:             " << txPower << " dBm\n";
    std::cout << "Intervalo pacotes:   " << packetInterval << " s\n";
    std::cout << "Tamanho pacote:      " << packetSize << " bytes\n";
    std::cout << "Pacotes transmitidos:" << totalTxPackets << "\n";
    std::cout << "Pacotes recebidos:   " << totalRxPackets << "\n";
    std::cout << "PDR:                 " << pdr << " %\n";
    std::cout << "Atraso medio:        " << avgDelay << " s\n";
    std::cout << "Throughput medio:    " << throughputKbps << " kbps\n";
    std::cout << "================================\n";

    Simulator::Destroy();
    return 0;
}
