// simple5g.cc
#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include <random>
#include <algorithm>
#include <set>
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Simple5g");

int
main(int argc, char* argv[])
{
    // Basic scenario
    uint16_t gNbNum = 1;
    uint16_t ueTotal = 25;
    Time simTime = Seconds(5.0);
    Time appStart = Seconds(0.5);

    // Radio params
    uint16_t numerology = 2;
    double centralFrequency = 28e9;
    double bandwidth = 100e6;
    double totalTxPower = 200.0; // dBm

    // UDP traffic
    uint16_t udpPort = 50000;
    uint32_t packetSize = 512;
    Time packetInterval = Seconds(0.01); // 100 packets/sec

    CommandLine cmd;
    cmd.AddValue("simTime", "Simulation time", simTime);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer gnbNodes;
    gnbNodes.Create(gNbNum);
    NodeContainer ueNodes;
    ueNodes.Create(ueTotal);

    // Mobility: place gNB and UEs
    Ptr<ListPositionAllocator> gnbPos = CreateObject<ListPositionAllocator>();
    gnbPos->Add(Vector(60.0, 60.0, 0.0));
    MobilityHelper gnbMob;
    gnbMob.SetPositionAllocator(gnbPos);
    gnbMob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gnbMob.Install(gnbNodes);

    //UE will be psotioned randomo in 120x120 areea
    // UEs with random initial positions and mobility,, Mobile Nodes
	Ptr<RandomRectanglePositionAllocator> uePosAlloc = CreateObject<RandomRectanglePositionAllocator>();
	uePosAlloc->SetAttribute("X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=120.0]"));
	uePosAlloc->SetAttribute("Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=120.0]"));
	uePosAlloc->SetAttribute("Z", DoubleValue(0.0));

	MobilityHelper ueMob;
	ueMob.SetPositionAllocator(uePosAlloc);

	// Set mobility model: Random walk inside 120x120 region
	ueMob.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
	    "Bounds", RectangleValue(Rectangle(0.0, 120.0, 0.0, 120.0)), // simulation area
	    "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=3.0]"), // speed range
	    "Distance", DoubleValue(3.0)); // step length before changing direction

ueMob.Install(ueNodes);

// Choose mobility model:


    // NR helpers
    Ptr<NrPointToPointEpcHelper> nrEpcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> beamHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetBeamformingHelper(beamHelper);
    nrHelper->SetEpcHelper(nrEpcHelper);

    // Configure spectrum / BWP via CcBwpCreator (single band)
    CcBwpCreator ccBwpCreator;
    const uint8_t numCcPerBand = 1;
    CcBwpCreator::SimpleOperationBandConf bandConf(centralFrequency, bandwidth, numCcPerBand);
    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);

    Ptr<NrChannelHelper> channelHelper = CreateObject<NrChannelHelper>();
    channelHelper->ConfigureFactories("UMi", "Default", "ThreeGpp");
    channelHelper->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(0)));
    channelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));

    // Assign channels to the single band and get all BWPs
    channelHelper->AssignChannelsToBands({band});
    BandwidthPartInfoPtrVector allBwps = CcBwpCreator::GetAllBwps({band});

    // Global NR attributes
    Config::SetDefault("ns3::NrRlcUm::MaxTxBufferSize", UintegerValue(999999999));
    beamHelper->SetAttribute("BeamformingMethod", TypeIdValue(DirectPathBeamforming::GetTypeId()));
    nrEpcHelper->SetAttribute("S1uLinkDelay", TimeValue(MilliSeconds(0)));

    // Antennas
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(2));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(4));
    nrHelper->SetUeAntennaAttribute("AntennaElement",
                                    PointerValue(CreateObject<IsotropicAntennaModel>()));
    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(4));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(8));
    nrHelper->SetGnbAntennaAttribute("AntennaElement",
                                     PointerValue(CreateObject<IsotropicAntennaModel>()));

    // Install devices
    NetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice(gnbNodes, allBwps);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes, allBwps);

    // Streams
    int64_t randomStream = 1;
    randomStream += nrHelper->AssignStreams(gnbDevs, randomStream);
    randomStream += nrHelper->AssignStreams(ueDevs, randomStream);

    // Per-node PHY attributes (numerology, tx power)
    double x = pow(10, totalTxPower / 10.0);
    double totalBandwidth = bandwidth;
    nrHelper->GetGnbPhy(gnbDevs.Get(0), 0)->SetAttribute("Numerology", UintegerValue(numerology));
    nrHelper->GetGnbPhy(gnbDevs.Get(0), 0)
        ->SetAttribute("TxPower", DoubleValue(10 * log10((bandwidth / totalBandwidth) * x)));

    // EPC / IP stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIfaces = nrEpcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to gNB
    nrHelper->AttachToClosestGnb(ueDevs, gnbDevs);

    // 5 radnom servers and 15 random client
    // Random number generation setup
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, ueTotal-1);
	// Select 5 random UEs to act as servers
	std::set<uint32_t> serverUEs;
	while (serverUEs.size() < 5) {
	    serverUEs.insert(dis(gen));
	}
	// Install UDP servers on the selected UEs
	std::map<uint32_t, uint16_t> serverPorts; // Store server UE index -> port mapping
	for (uint32_t serverIndex : serverUEs) {
	    uint16_t serverPort = 1000 + serverIndex; // Unique port for each server
	    serverPorts[serverIndex] = serverPort;
	    
	    UdpServerHelper udpServer(serverPort);
	    ApplicationContainer serverApp = udpServer.Install(ueNodes.Get(serverIndex));
	    serverApp.Start(appStart);
	    serverApp.Stop(simTime);
	    
	    std::cout << "Server installed on UE" << serverIndex << " listening on port " << serverPort << std::endl;
	}
	
		// Select 15 random client UEs (excluding the server UEs)
	std::set<uint32_t> clientUEs;
	std::vector<uint32_t> availableClients;
	for (uint32_t i = 0; i < ueTotal-1; i++) {
	    if (serverUEs.find(i) == serverUEs.end()) {
	        availableClients.push_back(i);
	    }
	}
	 
	 // Shuffle and select 15 clients
	std::shuffle(availableClients.begin(), availableClients.end(), gen);
	for (int i = 0; i < 15 && i < availableClients.size(); i++) {
	    clientUEs.insert(availableClients[i]);
	}
	
	
		// Assign each client to a random server
	for (uint32_t clientIndex : clientUEs) {
	    // Select a random server
	    auto it = serverUEs.begin();
	    std::advance(it, dis(gen) % serverUEs.size());
	    uint32_t serverIndex = *it;
	    
	    uint16_t serverPort = serverPorts[serverIndex];
	    Ipv4Address serverAddress = ueIfaces.GetAddress(serverIndex);
	    
	    // Install UDP client
	    UdpClientHelper udpClient(serverAddress, serverPort);
	    udpClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
	    udpClient.SetAttribute("Interval", TimeValue(packetInterval));
	    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
	    
	    ApplicationContainer clientApp = udpClient.Install(ueNodes.Get(clientIndex));
	    clientApp.Start(appStart);
	    clientApp.Stop(simTime);
	    
	    std::cout << "Client UE" << clientIndex << " -> Server UE" << serverIndex 
		 << " (" << serverAddress << ":" << serverPort << ")" << std::endl;
	}
   

    // Flow monitor on the two UEs (and optionally the gNB)
    FlowMonitorHelper flowmonHelper;
    NodeContainer monitorNodes;
    monitorNodes.Add(ueNodes);
    Ptr<FlowMonitor> monitor = flowmonHelper.Install(monitorNodes);
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));


 //Let add animation to simulation
    // Create the animation file
	AnimationInterface anim("25-nodes-animation-mobile-no-ddos.xml");

	// Optional: customize node appearance
	for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
	    anim.UpdateNodeDescription(ueNodes.Get(i), "UE");  // label
	    anim.UpdateNodeColor(ueNodes.Get(i), 0, 255, 0);   // green
	}

	for (uint32_t i = 0; i < gnbNodes.GetN(); ++i) {
	    anim.UpdateNodeDescription(gnbNodes.Get(i), "gNB");
	    anim.UpdateNodeColor(gnbNodes.Get(i), 255, 255, 0);  // yellow
	}

	
    // Run
    Simulator::Stop(simTime);
    Simulator::Run();

    // Print FlowMonitor stats
   
    // Print FlowMonitor stats
	monitor->CheckForLostPackets();
	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
	FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

	double flowDuration = (simTime - appStart).GetSeconds();

	// Variables for summary statistics
	uint64_t totalTxPackets = 0;
	uint64_t totalRxPackets = 0;
	uint64_t totalTxBytes = 0;
	uint64_t totalRxBytes = 0;
	uint64_t totalLostPackets = 0;
	double totalDelaySum = 0.0;
	double totalJitterSum = 0.0;
	int flowsWithPackets = 0;

	std::cout << "=== PER-FLOW STATISTICS ===" << std::endl;
	for (const auto &entry : stats)
	{
	    FlowId id = entry.first;
	    const FlowMonitor::FlowStats &st = entry.second;
	    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(id);

	    std::cout << "Flow " << id << " (" << t.sourceAddress << ":" << t.sourcePort << " -> "
		 << t.destinationAddress << ":" << t.destinationPort << ") proto "
		 << (t.protocol == 17 ? "UDP" : std::to_string(t.protocol)) << "\n";
	    std::cout << "  Tx Packets: " << st.txPackets << "\n";
	    std::cout << "  Rx Packets: " << st.rxPackets << "\n";
	    std::cout << "  Lost Packets: " << st.lostPackets << "\n";
	    std::cout << "  Packet Loss Rate: " << (st.txPackets > 0 ? (100.0 * st.lostPackets / st.txPackets) : 0) << "%\n";
	    std::cout << "  Tx Bytes:   " << st.txBytes << "\n";
	    std::cout << "  Rx Bytes:   " << st.rxBytes << "\n";
	    double throughputMbps = (st.rxBytes * 8.0) / flowDuration / 1e6;
	    std::cout << "  Throughput: " << throughputMbps << " Mbps\n";
	    if (st.rxPackets > 0)
	    {
	        double meanDelayMs = 1000.0 * st.delaySum.GetSeconds() / st.rxPackets;
	        double meanJitterMs = 1000.0 * st.jitterSum.GetSeconds() / st.rxPackets;
	        std::cout << "  Mean delay: " << meanDelayMs << " ms\n";
	        std::cout << "  Mean jitter: " << meanJitterMs << " ms\n";
	    }
	    std::cout << std::endl;

	    // Accumulate summary statistics
	    totalTxPackets += st.txPackets;
	    totalRxPackets += st.rxPackets;
	    totalTxBytes += st.txBytes;
	    totalRxBytes += st.rxBytes;
	    totalLostPackets += st.lostPackets;
	    
	    if (st.rxPackets > 0) {
	        totalDelaySum += st.delaySum.GetSeconds();
	        totalJitterSum += st.jitterSum.GetSeconds();
	        flowsWithPackets++;
	    }
	}

	// Calculate summary statistics
	double packetLossRate = totalTxPackets > 0 ? (100.0 * totalLostPackets / totalTxPackets) : 0;
	double avgThroughputMbps = (totalRxBytes * 8.0) / flowDuration / 1e6;
	double meanDelayMs = flowsWithPackets > 0 ? (1000.0 * totalDelaySum / totalRxPackets) : 0;
	double meanJitterMs = flowsWithPackets > 0 ? (1000.0 * totalJitterSum / totalRxPackets) : 0;

	// Print summary statistics
	std::cout << "=== SUMMARY STATISTICS ===" << std::endl;
	std::cout << "Total Flows: " << stats.size() << std::endl;
	std::cout << "Total Tx Packets: " << totalTxPackets << std::endl;
	std::cout << "Total Rx Packets: " << totalRxPackets << std::endl;
	std::cout << "Total Lost Packets: " << totalLostPackets << std::endl;
	std::cout << "Overall Packet Loss Rate: " << packetLossRate << "%" << std::endl;
	std::cout << "Total Tx Bytes: " << totalTxBytes << std::endl;
	std::cout << "Total Rx Bytes: " << totalRxBytes << std::endl;
	std::cout << "Average Throughput: " << avgThroughputMbps << " Mbps" << std::endl;
	std::cout << "Mean Delay: " << meanDelayMs << " ms" << std::endl;
	std::cout << "Mean Jitter: " << meanJitterMs << " ms" << std::endl;
	std::cout << "Simulation Duration: " << flowDuration << " seconds" << std::endl;
	std::cout << "==================================" << std::endl;

    Simulator::Destroy();
    return 0;
}

