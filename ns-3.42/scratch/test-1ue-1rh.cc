#include "ns3/applications-module.h"
#include "ns3/buildings-module.h"
#include "ns3/config-store-module.h"
#include "ns3/netanim-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/eps-bearer.h"
#include "ns3/point-to-point-module.h"

/** -------------- Topology --------------
 * 
 * ue0 -   |--- gnB ---|--- pgw ---| - remoteHost
 * 
 */

#define VelocityModel ConstantVelocityMobilityModel

using namespace ns3;

NodeContainer uesContainer;
NodeContainer gnbContainer;
NodeContainer remoteHosts;

Ptr<Node> pgw;
Ptr<Node> sgw;
Ptr<Node> remoteHost;

int numberUes;
int numberGnbs;
int numberRemoteHosts;

int duration = 10;
double enbX = 225.0;
double enbY = 225.0;
double radius = 600;

Time simTime = Seconds(10);

Ptr<UniformRandomVariable> uRand = CreateObject<UniformRandomVariable>();

Ptr<NrHelper> nrHelper;
Ptr<NrPointToPointEpcHelper> core;

void NotifyCqiReport(std::string context, uint16_t cellId, uint16_t rnti, uint8_t cqi);
void SetMobility();
void CheckCourse(std::string context, Ptr<MobilityModel> mob);
void BuildApps(Ipv4InterfaceContainer& ueIpIface);

NS_LOG_COMPONENT_DEFINE("Temp");

int
main(int argc, char* argv[])
{

    LogComponentEnable("NrRlcUm", LOG_LEVEL_ALL);
    // LogComponentEnable("NrGnbMac", LOG_LEVEL_ALL);
    // LogComponentEnable("NrMacSchedulerNs3", LOG_LEVEL_ALL);
    // LogComponentEnable("NrMacSchedulerLCG", LOG_LEVEL_ALL);
    LogComponentEnable("DualQCoupledPiSquareQueueDisc", LOG_LEVEL_INFO);

    numberGnbs = 1;
    numberUes = 1;
    numberRemoteHosts = 1;

    NS_LOG_INFO("Creating " << numberGnbs << " gNBs" <<
                " and " << numberUes << " UEs" << 
                " and " << numberRemoteHosts << " remote hosts");
        
    uesContainer.Create(numberUes);
    gnbContainer.Create(numberGnbs);
    remoteHosts.Create(numberRemoteHosts);

    for (int i = 0; i < numberUes; i++)
        NS_LOG_DEBUG("UE " << i << " -> " << uesContainer.Get(i));

    for (int i = 0; i < numberGnbs; i++)
        NS_LOG_DEBUG("gNB " << i << " -> " << gnbContainer.Get(i));

    for (int i = 0; i < numberRemoteHosts; i++)
        NS_LOG_DEBUG("remoteHost " << i << " -> " << remoteHosts.Get(i));

    uint16_t numerology = 0;
    double centralFrequency = 4e9;
    double bandwidth = 10e6;
    double total = 10;
    int64_t randomStream = 1;

    // Where we will store the output files.
    std::string simTag = "default";
    std::string outputDir = "./";

    nrHelper = CreateObject<NrHelper>();
    core = CreateObject<NrPointToPointEpcHelper>();
    nrHelper->SetEpcHelper(core);

    // Selecting MAC scheduler (implicit default has a bug!)
    nrHelper->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrMacSchedulerTdmaRR"));
    Config::SetDefault("ns3::TcpSocketBase::UseEcn", StringValue("On"));

    pgw = core->GetPgwNode();
    sgw = core->GetSgwNode();
    remoteHost = remoteHosts.Get(0);

    SetMobility();

    BandwidthPartInfoPtrVector allBwps;
    CcBwpCreator ccBwpCreator;
    const uint8_t numCcPerBand = 1; 

    auto bandMask = NrHelper::INIT_PROPAGATION | NrHelper::INIT_CHANNEL;

    // Create the configuration for the CcBwpHelper. SimpleOperationBandConf creates
    // a single BWP per CC
    CcBwpCreator::SimpleOperationBandConf bandConf (centralFrequency,
                                                    bandwidth,
                                                    numCcPerBand,
                                                    BandwidthPartInfo::UMa);

    bandConf.m_numBwp = 1;

    // By using the configuration created, it is time to make the operation band
    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);

    /*
     * The configured spectrum division is:
     * ------------Band1--------------|
     * ------------CC1----------------|
     * ------------BWP1---------------|
     */

    nrHelper->InitializeOperationBand(&band, bandMask);
    allBwps = CcBwpCreator::GetAllBwps({band});

    Packet::EnableChecking();
    Packet::EnablePrinting();

    /*
     * We have configured the attributes we needed. Now, install and get the 
     * pointers to the NetDevices, which contains all the NR stack:
     */

    NetDeviceContainer gnbNetDev =
        nrHelper->InstallGnbDevice(gnbContainer, allBwps);
    NetDeviceContainer ueNetDev = 
        nrHelper->InstallUeDevice(uesContainer, allBwps);

    randomStream += nrHelper->AssignStreams(gnbNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueNetDev, randomStream);

    // Get the first netdevice (enbNetDev.Get (0)) and the first bandwidth part (0)
    // and set the attribute.
    nrHelper->GetGnbPhy(gnbNetDev.Get(0), 0)
        ->SetAttribute("Numerology", UintegerValue(numerology));
    nrHelper->GetGnbPhy(gnbNetDev.Get(0), 0)
        ->SetAttribute("TxPower", DoubleValue(total));

    // When all the configuration is done, explicitly call UpdateConfig ()
    for (auto it = gnbNetDev.Begin(); it != gnbNetDev.End(); ++it)
        DynamicCast<NrGnbNetDevice>(*it)->UpdateConfig();

    for (auto it = ueNetDev.Begin(); it != ueNetDev.End(); ++it)
        DynamicCast<NrUeNetDevice>(*it)->UpdateConfig();

    InternetStackHelper internet;
    internet.Install(remoteHosts);

    // connect the remoteHosts to pgw. Setup routing too
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.000)));

    NetDeviceContainer internetDevices1 = p2ph.Install(pgw, remoteHost);

    Ipv4AddressHelper ipv4h;
    Ipv4StaticRoutingHelper ipv4RoutingHelper;

    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces1 = ipv4h.Assign(internetDevices1);

    Ptr<Ipv4StaticRouting> remoteHostStaticRouting1 =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting1->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    internet.Install(uesContainer);

    Ipv4InterfaceContainer ueIpIface = core->AssignUeIpv4Address(ueNetDev);

    // Set the default gateway for the UEs
    for (uint32_t j = 0; j < uesContainer.GetN(); ++j)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(
            uesContainer.Get(j)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(core->GetUeDefaultGatewayAddress(), 1);
    }

    // attach UEs to the closest gnb
    nrHelper->AttachToClosestGnb(ueNetDev, gnbNetDev);

    // //pcap files and debug for nodeList
    internet.EnablePcapIpv4("debugUe", uesContainer);
    for (uint32_t i = 0; i < NodeList::GetNNodes(); i++)
    {
        NS_LOG_DEBUG("NodeList[" << i << "]: " << NodeList::GetNode(i));
    }

    // ---------------------------- Application ----------------------------

    BuildApps(ueIpIface);

    // ---------------------------- Tracing ----------------------------

    // Config::Connect("/NodeList/*/DeviceList/*/$ns3::NrGnbNetDevice/BandwidthPartMap/*/NrGnbPhy/ReportCqiValues",
    //                 MakeCallback(&NotifyCqiReport));

    // ---------------------------- Flow Monitor ----------------------------

    FlowMonitorHelper flowmonHelper;
    NodeContainer endpointNodes;
    endpointNodes.Add(remoteHost);
    endpointNodes.Add(uesContainer);

    Ptr<ns3::FlowMonitor> monitor = flowmonHelper.Install(endpointNodes);
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));

    Simulator::Stop(simTime);
    Simulator::Run();

    // Print per-flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    std::ofstream outFile;
    std::string filename = outputDir + "/" + simTag;
    outFile.open(filename.c_str(), std::ofstream::out | std::ofstream::trunc);

    if (!outFile.is_open())
    {
        std::cerr << "Can't open file " << filename << std::endl;
        return 1;
    }

    outFile.setf(std::ios_base::fixed);

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin();
         i != stats.end();
         ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        
        outFile << "Flow " << i->first << " (" << t.sourceAddress << ":" << t.sourcePort << " -> "
                << t.destinationAddress << ":" << t.destinationPort << ") - " << "\n";
        outFile << "  Tx Packets: " << i->second.txPackets << "\n";
        outFile << "  Rx Packets: " << i->second.rxPackets << "\n";
        outFile << "  Throughput: " << i->second.rxBytes * 8.0 / simTime.GetSeconds() / 1024 / 1024
                << " Mbps\n";
    }

    outFile.close();

    Simulator::Destroy();

    return 0;
}

void
NotifyCqiReport(std::string context, uint16_t cellId, uint16_t rnti, uint8_t cqi)
{
    NS_LOG_INFO(context << " - CQI report from UE " << rnti << " in cell " << cellId << ": " << +cqi);
}

void
SetMobility()
{
    MobilityHelper uesMobility;
    MobilityHelper nodesMobility;

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

    positionAlloc->Add(Vector(enbX, enbY, 0.0)); // gNB
    positionAlloc->Add(Vector(enbX, enbY - 30.0, 0.0)); //pgw
    positionAlloc->Add(Vector(enbX, enbY - 10.0, 0.0)); //sgw
    positionAlloc->Add(Vector(enbX - 75.0, enbY - 50.0, 0.0)); //remoteHost

    nodesMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    nodesMobility.SetPositionAllocator(positionAlloc);
    nodesMobility.Install(gnbContainer);
    nodesMobility.Install(pgw);
    nodesMobility.Install(sgw);
    nodesMobility.Install(remoteHosts);
    
    uesMobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                    "X", DoubleValue(enbX),
                                    "Y", DoubleValue(enbY),
                                    "Rho", StringValue("ns3::UniformRandomVariable[Min=150]|[Max=600]"));

    uesMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 750, 0, 750)));
    uesMobility.Install(uesContainer);

}

void
CheckCourse(std::string context, Ptr<MobilityModel> mob)
{
    Vector pos = mob->GetPosition();
    NS_LOG_DEBUG("UE position: " << pos);
    Vector vel = mob->GetVelocity();

    double ueX = pos.x;
    double ueY = pos.y;

    double distance = sqrt(pow(ueX - enbX, 2) + pow(ueY - enbY, 2));

    if (distance > radius)
    {
        vel.x = -vel.x;
        vel.y = -vel.y;

        NS_LOG_INFO("UE out of course. Changing direction.");
    }

    Simulator::Schedule(Seconds(1.0), &CheckCourse, "", mob);
}

void
BuildApps(Ipv4InterfaceContainer& ueIps)
{
    uint16_t sinkPort = 1234;
    Address sinkLocalAddressClassic(InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    PacketSinkHelper dlSinkClassic("ns3::TcpSocketFactory", sinkLocalAddressClassic);
    ApplicationContainer sinkAppClassic = dlSinkClassic.Install(uesContainer.Get(0));
    sinkAppClassic.Start(Seconds(1.0));
    sinkAppClassic.Stop(simTime + Seconds(1.0));

    // First application: remoteHost -> ue (using Cubic)
    // Configure the TCP Cubic for the first application
    uint16_t dlPortClassic = 1234;
    Config::Set("/NodeList/1/$ns3::TcpL4Protocol/SocketType", TypeIdValue(TcpCubic::GetTypeId()));
    BulkSendHelper classicClient("ns3::TcpSocketFactory",
                                 InetSocketAddress(ueIps.GetAddress(0), dlPortClassic));
    classicClient.SetAttribute("MaxBytes", UintegerValue(100000000)); // 100MB

    ApplicationContainer clientAppsClassic;
    AddressValue remoteAddressClassic(InetSocketAddress(ueIps.GetAddress(0), dlPortClassic));
    classicClient.SetAttribute("Remote", remoteAddressClassic);
    clientAppsClassic.Add(classicClient.Install(remoteHost));
    clientAppsClassic.Start(Seconds(2.0));
    clientAppsClassic.Stop(simTime);

    // Second application: remoteHost -> ue (using DCTCP)
    // Configure the TCP DCTCP for the second application
    uint16_t dlPortScalable = 1235;
    Config::Set("/NodeList/1/$ns3::TcpL4Protocol/SocketType", TypeIdValue(TcpDctcp::GetTypeId()));
    BulkSendHelper scalableClient("ns3::TcpSocketFactory",
                                  InetSocketAddress(ueIps.GetAddress(0), dlPortScalable));
    scalableClient.SetAttribute("MaxBytes", UintegerValue(100000000)); // 100MB

    ApplicationContainer clientAppsScalable;
    AddressValue remoteAddressScalable(InetSocketAddress(ueIps.GetAddress(0), dlPortScalable));
    scalableClient.SetAttribute("Remote", remoteAddressScalable);
    clientAppsScalable.Add(scalableClient.Install(remoteHost));
    clientAppsScalable.Start(Seconds(2.0));
    clientAppsScalable.Stop(simTime);

    // Restore the default TCP configuration (if needed elsewhere)
    Config::Set("/NodeList/1/$ns3::TcpL4Protocol/SocketType", TypeIdValue(TcpCubic::GetTypeId()));
}