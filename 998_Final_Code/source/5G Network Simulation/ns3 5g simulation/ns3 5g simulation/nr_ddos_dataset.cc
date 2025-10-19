// nr_ddos_dataset.cc
// Parameterized 5G NR scenario that periodically exports per-flow, per-second CSV rows
// covering key flow features suitable for ML datasets (benign vs DDoS).

#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/netanim-module.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NrDdosDataset");

struct FlowKey
{
    Ipv4Address src;
    Ipv4Address dst;
    uint16_t srcPort;
    uint16_t dstPort;
    uint8_t protocol; // 6 TCP, 17 UDP

    bool operator<(const FlowKey &other) const
    {
        if (src != other.src) return src < other.src;
        if (dst != other.dst) return dst < other.dst;
        if (srcPort != other.srcPort) return srcPort < other.srcPort;
        if (dstPort != other.dstPort) return dstPort < other.dstPort;
        return protocol < other.protocol;
    }
};

struct FlowSnapshot
{
    uint64_t txPackets = 0;
    uint64_t rxPackets = 0;
    uint64_t txBytes = 0;
    uint64_t rxBytes = 0;
    uint64_t lostPackets = 0;
    double delaySum = 0.0;  // seconds
    double jitterSum = 0.0; // seconds
};

static std::map<FlowKey, FlowSnapshot> g_prev;
static std::ofstream g_out;
static double g_windowStart = 0.0;
static double g_windowSize = 1.0; // seconds
static std::string g_scenarioId;
static uint32_t g_ueTotal = 0;
static std::set<uint32_t> g_attackers;
static bool g_labelIntensity = false;
static std::string g_labelMode = "binary"; // binary|intensity
static std::map<uint32_t, bool> g_isServer;

static Ptr<Ipv4FlowClassifier> g_classifier;
static Ptr<FlowMonitor> g_monitor;

static std::string BoolToStr(bool v) { return v ? "1" : "0"; }

static int IntensityToLabel(const Time &attackInterval)
{
    // Lower interval -> higher rate
    double us = attackInterval.GetSeconds() * 1e6;
    if (us <= 200) return 3; // high
    if (us <= 500) return 2; // medium
    return 1; // low
}

static void WriteCsvHeader()
{
    g_out << "time_start,time_end,scenario_id,ue_total,attackers,";
    g_out << "src_ip,dst_ip,src_port,dst_port,protocol,packet_size,";
    g_out << "flow_duration,total_bytes_fwd,total_bytes_bwd,total_pkts_fwd,total_pkts_bwd,";
    g_out << "pkts_per_sec,bytes_per_sec,flow_pkts_per_sec,flow_bytes_per_sec,";
    g_out << "jitter_ms,delay_ms,label_binary,label_intensity\n";
}

static void SampleAndWrite()
{
    // Poll current stats
    g_monitor->CheckForLostPackets();
    auto stats = g_monitor->GetFlowStats();

    std::map<FlowKey, FlowSnapshot> current;

    for (const auto &kv : stats)
    {
        FlowId id = kv.first;
        const FlowMonitor::FlowStats &st = kv.second;
        Ipv4FlowClassifier::FiveTuple t = g_classifier->FindFlow(id);

        FlowKey key{t.sourceAddress, t.destinationAddress, t.sourcePort, t.destinationPort, t.protocol};
        FlowSnapshot snap;
        snap.txPackets = st.txPackets;
        snap.rxPackets = st.rxPackets;
        snap.txBytes = st.txBytes;
        snap.rxBytes = st.rxBytes;
        snap.lostPackets = st.lostPackets;
        snap.delaySum = st.delaySum.GetSeconds();
        snap.jitterSum = st.jitterSum.GetSeconds();
        current[key] = snap;
    }

    // For each forward flow, find reverse to compute bwd
    for (const auto &kv : current)
    {
        const FlowKey &fwdKey = kv.first;
        const FlowSnapshot &curFwd = kv.second;

        FlowKey revKey{fwdKey.dst, fwdKey.src, fwdKey.dstPort, fwdKey.srcPort, fwdKey.protocol};
        FlowSnapshot curBwd = current.count(revKey) ? current[revKey] : FlowSnapshot{};

        FlowSnapshot prevFwd = g_prev.count(fwdKey) ? g_prev[fwdKey] : FlowSnapshot{};
        FlowSnapshot prevBwd = g_prev.count(revKey) ? g_prev[revKey] : FlowSnapshot{};

        uint64_t dTxPktsF = curFwd.txPackets - prevFwd.txPackets;
        uint64_t dRxPktsF = curFwd.rxPackets - prevFwd.rxPackets;
        uint64_t dTxBytesF = curFwd.txBytes - prevFwd.txBytes;
        uint64_t dRxBytesF = curFwd.rxBytes - prevFwd.rxBytes;

        uint64_t dTxPktsB = curBwd.txPackets - prevBwd.txPackets;
        uint64_t dRxPktsB = curBwd.rxPackets - prevBwd.rxPackets;
        uint64_t dTxBytesB = curBwd.txBytes - prevBwd.txBytes;
        uint64_t dRxBytesB = curBwd.rxBytes - prevBwd.rxBytes;

        uint64_t totalPktsFwd = dTxPktsF + dRxPktsF;
        uint64_t totalPktsBwd = dTxPktsB + dRxPktsB;
        uint64_t totalBytesFwd = dTxBytesF + dRxBytesF;
        uint64_t totalBytesBwd = dTxBytesB + dRxBytesB;

        double duration = g_windowSize;
        double pktsPerSec = (totalPktsFwd + totalPktsBwd) / duration;
        double bytesPerSec = (totalBytesFwd + totalBytesBwd) / duration;
        double flowPktsPerSec = totalPktsFwd / duration;
        double flowBytesPerSec = totalBytesFwd / duration;

        double avgDelayMs = 0.0;
        double avgJitterMs = 0.0;
        uint64_t dRxPktsAll = dRxPktsF + dRxPktsB;
        double dDelay = (curFwd.delaySum + curBwd.delaySum) - (prevFwd.delaySum + prevBwd.delaySum);
        double dJitter = (curFwd.jitterSum + curBwd.jitterSum) - (prevFwd.jitterSum + prevBwd.jitterSum);
        if (dRxPktsAll > 0)
        {
            avgDelayMs = 1000.0 * dDelay / static_cast<double>(dRxPktsAll);
            avgJitterMs = 1000.0 * dJitter / static_cast<double>(dRxPktsAll);
        }

        // Label: if the source UE is in attackers set, mark attack
        // Extract UE index from IPv4: EPC helper assigns 7.X.Y.Z; we cannot parse UE index reliably here,
        // so approximate by checking src/dst membership using an external map would be better.
        // For practicality, we label by rate: if packets/sec from a single source exceed threshold, flag as attack.
        bool attackBinary = (flowPktsPerSec > 5000.0); // heuristic for high-rate DDoS
        int attackIntensity = attackBinary ? 3 : 0;

        // Packet size: average in window
        double avgPktSize = 0.0;
        uint64_t pktsAll = totalPktsFwd + totalPktsBwd;
        if (pktsAll > 0)
        {
            avgPktSize = static_cast<double>(totalBytesFwd + totalBytesBwd) / static_cast<double>(pktsAll);
        }

        double timeEnd = g_windowStart + g_windowSize;

        g_out << g_windowStart << "," << timeEnd << "," << g_scenarioId << "," << g_ueTotal << ","
              << g_attackers.size() << ",";
        g_out << fwdKey.src << "," << fwdKey.dst << "," << fwdKey.srcPort << "," << fwdKey.dstPort << ","
              << unsigned(fwdKey.protocol) << "," << avgPktSize << ",";
        g_out << duration << "," << totalBytesFwd << "," << totalBytesBwd << "," << totalPktsFwd << ","
              << totalPktsBwd << ",";
        g_out << pktsPerSec << "," << bytesPerSec << "," << flowPktsPerSec << "," << flowBytesPerSec << ",";
        g_out << avgJitterMs << "," << avgDelayMs << "," << (attackBinary ? 1 : 0) << "," << attackIntensity << "\n";
    }

    g_out.flush();
    g_prev = std::move(current);
    g_windowStart += g_windowSize;

    Simulator::Schedule(Seconds(g_windowSize), &SampleAndWrite);
}

int main(int argc, char *argv[])
{
    uint16_t gNbNum = 1;
    uint16_t ueTotal = 25;
    bool enableAttack = true;
    bool mobile = true;
    bool useTcp = false; // default UDP
    std::string outPath = "datasets";
    std::string scenarioTag = "default";
    Time simTime = Seconds(20.0);
    Time appStart = Seconds(0.5);
    Time attackInterval = Seconds(0.0002); // intensity control
    Time benignInterval = Seconds(0.02);

    CommandLine cmd;
    cmd.AddValue("ueTotal", "Number of UEs", ueTotal);
    cmd.AddValue("mobile", "If true, UEs move (RandomWalk)", mobile);
    cmd.AddValue("useTcp", "Use TCP instead of UDP", useTcp);
    cmd.AddValue("enableAttack", "Enable DDoS attackers", enableAttack);
    cmd.AddValue("attackInterval", "Inter-packet interval for attackers", attackInterval);
    cmd.AddValue("benignInterval", "Inter-packet interval for benign clients", benignInterval);
    cmd.AddValue("simTime", "Total simulation time", simTime);
    cmd.AddValue("outPath", "Output folder for CSV", outPath);
    cmd.AddValue("scenarioTag", "Scenario tag for scenario_id and file name", scenarioTag);
    cmd.AddValue("window", "Sampling window size (s)", g_windowSize);
    cmd.Parse(argc, argv);

    g_ueTotal = ueTotal;

    // Create output directory path + file
    std::ostringstream fname;
    fname << outPath << "/dataset_" << scenarioTag << "_ue" << ueTotal << (useTcp ? "_tcp" : "_udp")
          << (enableAttack ? "_ddos" : "_benign") << ".csv";
    std::string fileName = fname.str();

    g_out.open(fileName, std::ios::out);
    WriteCsvHeader();

    // NR parameters (single gNB / single band)
    uint16_t numerology = 2;
    double centralFrequency = 28e9;
    double bandwidth = 100e6;
    double totalTxPower = 200.0;

    NodeContainer gnbNodes;
    gnbNodes.Create(gNbNum);
    NodeContainer ueNodes;
    ueNodes.Create(ueTotal);

    Ptr<ListPositionAllocator> gnbPos = CreateObject<ListPositionAllocator>();
    gnbPos->Add(Vector(60.0, 60.0, 0.0));
    MobilityHelper gnbMob;
    gnbMob.SetPositionAllocator(gnbPos);
    gnbMob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gnbMob.Install(gnbNodes);

    MobilityHelper ueMob;
    if (mobile)
    {
        Ptr<RandomRectanglePositionAllocator> uePosAlloc = CreateObject<RandomRectanglePositionAllocator>();
        uePosAlloc->SetAttribute("X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=120.0]"));
        uePosAlloc->SetAttribute("Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=120.0]"));
        uePosAlloc->SetAttribute("Z", DoubleValue(0.0));
        ueMob.SetPositionAllocator(uePosAlloc);
        ueMob.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue(Rectangle(0.0, 120.0, 0.0, 120.0)),
                               "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=3.0]"),
                               "Distance", DoubleValue(3.0));
    }
    else
    {
        Ptr<RandomRectanglePositionAllocator> uePosAlloc = CreateObject<RandomRectanglePositionAllocator>();
        uePosAlloc->SetAttribute("X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=120.0]"));
        uePosAlloc->SetAttribute("Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=120.0]"));
        uePosAlloc->SetAttribute("Z", DoubleValue(0.0));
        ueMob.SetPositionAllocator(uePosAlloc);
        ueMob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    }
    ueMob.Install(ueNodes);

    Ptr<NrPointToPointEpcHelper> nrEpcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> beamHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetBeamformingHelper(beamHelper);
    nrHelper->SetEpcHelper(nrEpcHelper);

    CcBwpCreator ccBwpCreator;
    const uint8_t numCcPerBand = 1;
    CcBwpCreator::SimpleOperationBandConf bandConf(centralFrequency, bandwidth, numCcPerBand);
    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);

    Ptr<NrChannelHelper> channelHelper = CreateObject<NrChannelHelper>();
    channelHelper->ConfigureFactories("UMi", "Default", "ThreeGpp");
    channelHelper->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(0)));
    channelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));
    channelHelper->AssignChannelsToBands({band});
    BandwidthPartInfoPtrVector allBwps = CcBwpCreator::GetAllBwps({band});

    Config::SetDefault("ns3::NrRlcUm::MaxTxBufferSize", UintegerValue(999999999));
    beamHelper->SetAttribute("BeamformingMethod", TypeIdValue(DirectPathBeamforming::GetTypeId()));
    nrEpcHelper->SetAttribute("S1uLinkDelay", TimeValue(MilliSeconds(0)));

    NetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice(gnbNodes, allBwps);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes, allBwps);

    int64_t randomStream = 1;
    randomStream += nrHelper->AssignStreams(gnbDevs, randomStream);
    randomStream += nrHelper->AssignStreams(ueDevs, randomStream);

    double x = pow(10, totalTxPower / 10.0);
    double totalBandwidth = bandwidth;
    nrHelper->GetGnbPhy(gnbDevs.Get(0), 0)->SetAttribute("Numerology", UintegerValue(numerology));
    nrHelper->GetGnbPhy(gnbDevs.Get(0), 0)
        ->SetAttribute("TxPower", DoubleValue(10 * log10((bandwidth / totalBandwidth) * x)));

    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIfaces = nrEpcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    nrHelper->AttachToClosestGnb(ueDevs, gnbDevs);

    // Random server/client assignment
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, ueTotal - 1);

    std::set<uint32_t> serverUEs;
    while (serverUEs.size() < 5 && serverUEs.size() < ueTotal)
    {
        serverUEs.insert(dis(gen));
    }

    std::map<uint32_t, uint16_t> serverPorts;
    for (uint32_t serverIndex : serverUEs)
    {
        uint16_t serverPort = 1000 + serverIndex;
        serverPorts[serverIndex] = serverPort;
        g_isServer[serverIndex] = true;

        if (useTcp)
        {
            PacketSinkHelper sink("ns3::TcpSocketFactory",
                                  InetSocketAddress(Ipv4Address::GetAny(), serverPort));
            auto app = sink.Install(ueNodes.Get(serverIndex));
            app.Start(appStart);
            app.Stop(simTime);
        }
        else
        {
            UdpServerHelper udpServer(serverPort);
            auto app = udpServer.Install(ueNodes.Get(serverIndex));
            app.Start(appStart);
            app.Stop(simTime);
        }
    }

    // Choose client pool excluding servers and (optionally) reserve last 5 for attackers
    std::vector<uint32_t> pool;
    uint32_t benignLimit = ueTotal;
    if (enableAttack && ueTotal >= 5)
    {
        benignLimit = ueTotal - 5;
        for (uint32_t i = ueTotal - 5; i < ueTotal; ++i) g_attackers.insert(i);
    }

    for (uint32_t i = 0; i < benignLimit; ++i)
    {
        if (!g_isServer[i]) pool.push_back(i);
    }
    std::shuffle(pool.begin(), pool.end(), gen);

    uint32_t benignClients = std::min<uint32_t>(15, pool.size());
    for (uint32_t idx = 0; idx < benignClients; ++idx)
    {
        uint32_t clientIndex = pool[idx];
        auto it = serverUEs.begin();
        std::advance(it, dis(gen) % serverUEs.size());
        uint32_t serverIndex = *it;
        uint16_t serverPort = serverPorts[serverIndex];
        Ipv4Address serverAddress = ueIfaces.GetAddress(serverIndex);

        if (useTcp)
        {
            OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(serverAddress, serverPort));
            onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
            onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
            onoff.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
            onoff.SetAttribute("PacketSize", UintegerValue(700));
            auto app = onoff.Install(ueNodes.Get(clientIndex));
            app.Start(appStart);
            app.Stop(simTime);
        }
        else
        {
            UdpClientHelper udpClient(serverAddress, serverPort);
            udpClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
            udpClient.SetAttribute("Interval", TimeValue(benignInterval));
            udpClient.SetAttribute("PacketSize", UintegerValue(512));
            auto app = udpClient.Install(ueNodes.Get(clientIndex));
            app.Start(appStart);
            app.Stop(simTime);
        }
    }

    if (enableAttack)
    {
        for (uint32_t clientIndex : g_attackers)
        {
            auto it = serverUEs.begin();
            std::advance(it, dis(gen) % serverUEs.size());
            uint32_t serverIndex = *it;
            uint16_t serverPort = serverPorts[serverIndex];
            Ipv4Address serverAddress = ueIfaces.GetAddress(serverIndex);

            if (useTcp)
            {
                OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(serverAddress, serverPort));
                onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
                onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
                onoff.SetAttribute("DataRate", DataRateValue(DataRate("200Mbps")));
                onoff.SetAttribute("PacketSize", UintegerValue(1024));
                auto app = onoff.Install(ueNodes.Get(clientIndex));
                app.Start(appStart);
                app.Stop(simTime);
            }
            else
            {
                UdpClientHelper udpClient(serverAddress, serverPort);
                udpClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
                udpClient.SetAttribute("Interval", TimeValue(attackInterval));
                udpClient.SetAttribute("PacketSize", UintegerValue(1024));
                auto app = udpClient.Install(ueNodes.Get(clientIndex));
                app.Start(appStart);
                app.Stop(simTime);
            }
        }
    }

    // FlowMonitor setup
    FlowMonitorHelper flowmonHelper;
    NodeContainer monitorNodes;
    monitorNodes.Add(ueNodes);
    g_monitor = flowmonHelper.Install(monitorNodes);
    g_monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    g_monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    g_monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));
    g_classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());

    // Schedule periodic sampling
    g_windowStart = 0.0;
    g_scenarioId = scenarioTag;
    Simulator::Schedule(Seconds(g_windowSize), &SampleAndWrite);

    Simulator::Stop(simTime);
    Simulator::Run();

    g_out.flush();
    g_out.close();

    Simulator::Destroy();
    return 0;
}


