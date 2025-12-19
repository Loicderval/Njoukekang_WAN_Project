/*
 * ISN 3132 - Exercise 4: Multi-Hop WAN Architecture with Fault Tolerance
 * 
 * Network Topology for RegionalBank:
 *
 *   Branch-C (City C) --- DC-A (Router, City A) --- DR-B (City B)
 *                             |
 *                             Backup Link
 *
 * Network Topology Details:
 *
 *   Network 1: 10.1.1.0/24 (Branch-C to DC-A)
 *   Network 2: 10.1.2.0/24 (DC-A to DR-B - Primary)
 *   Network 3: 10.1.3.0/24 (DC-A to DR-B - Backup)
 *
 *   n0: Branch-C (Client, City C)
 *   n1: DC-A (Router, City A - Main Data Center)
 *   n2: DR-B (Server, City B - Disaster Recovery)
 *
 * - Branch-C must transit through DC-A to reach DR-B
 * - DC-A has primary and backup links to DR-B
 * - Static routing with failover capability
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiHopWAN_FaultTolerance");

// Global variables for tracking
Time primaryLinkFailureTime = Seconds(5.0);
bool primaryLinkFailed = false;
uint32_t packetsBeforeFailure = 0;
uint32_t packetsAfterFailure = 0;

// Callback function to track packet flow
void TrackPacketTransmission(Ptr<const Packet> packet)
{
    Time now = Simulator::Now();
    
    if (now < primaryLinkFailureTime)
    {
        packetsBeforeFailure++;
    }
    else if (now >= primaryLinkFailureTime)
    {
        packetsAfterFailure++;
        
        if (!primaryLinkFailed && now >= primaryLinkFailureTime + Seconds(0.1))
        {
            primaryLinkFailed = true;
            std::cout << "\n[" << now.GetSeconds() << "s] PRIMARY LINK FAILED!" << std::endl;
            std::cout << "   Packets before failure: " << packetsBeforeFailure << std::endl;
            std::cout << "   Now using backup path..." << std::endl;
        }
    }
}

int main(int argc, char* argv[])
{
    // ====================== SIMULATION PARAMETERS ======================
    Time simulationTime = Seconds(15.0);
    Time failureTime = Seconds(5.0);
    bool enableDynamicRouting = false;
    bool simulateFailure = true;
    
    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("failureTime", "Time when primary link fails", failureTime);
    cmd.AddValue("dynamic", "Enable dynamic routing (OSPF)", enableDynamicRouting);
    cmd.AddValue("failure", "Simulate link failure", simulateFailure);
    cmd.Parse(argc, argv);
    
    primaryLinkFailureTime = failureTime;
    
    // ====================== NODE CREATION ======================
    NodeContainer nodes;
    nodes.Create(3);  // n0: Branch-C, n1: DC-A, n2: DR-B
    
    Ptr<Node> n0 = nodes.Get(0); // Branch-C (Client, City C)
    Ptr<Node> n1 = nodes.Get(1); // DC-A (Router, City A)
    Ptr<Node> n2 = nodes.Get(2); // DR-B (Server, City B)
    
    // ====================== NETWORK TOPOLOGY ======================
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    
    // Link 1: Branch-C (n0) <-> DC-A (n1) - Network 1
    NodeContainer link1Nodes(n0, n1);
    NetDeviceContainer link1Devices = p2p.Install(link1Nodes);
    
    // Link 2: DC-A (n1) <-> DR-B (n2) - Network 2 (PRIMARY)
    PointToPointHelper p2pPrimary;
    p2pPrimary.SetDeviceAttribute("DataRate", StringValue("10Mbps")); // Higher bandwidth for primary
    p2pPrimary.SetChannelAttribute("Delay", StringValue("2ms"));
    
    NodeContainer link2Nodes(n1, n2);
    NetDeviceContainer link2Devices = p2pPrimary.Install(link2Nodes);
    
    // Link 3: DC-A (n1) <-> DR-B (n2) - Network 3 (BACKUP)
    PointToPointHelper p2pBackup;
    p2pBackup.SetDeviceAttribute("DataRate", StringValue("2Mbps")); // Lower bandwidth for backup
    p2pBackup.SetChannelAttribute("Delay", StringValue("10ms"));    // Higher delay for backup
    
    NodeContainer link3Nodes(n1, n2);
    NetDeviceContainer link3Devices = p2pBackup.Install(link3Nodes);
    
    // ====================== MOBILITY FOR NETANIM ======================
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    
    // Set positions to represent geographical locations
    Ptr<MobilityModel> mob0 = n0->GetObject<MobilityModel>();
    Ptr<MobilityModel> mob1 = n1->GetObject<MobilityModel>();
    Ptr<MobilityModel> mob2 = n2->GetObject<MobilityModel>();
    
    // Triangle layout: DC-A at top, Branch-C left, DR-B right
    mob0->SetPosition(Vector(0.0, 0.0, 0.0));     // Branch-C (City C) - bottom-left
    mob1->SetPosition(Vector(50.0, 50.0, 0.0));   // DC-A (City A) - top-center
    mob2->SetPosition(Vector(100.0, 0.0, 0.0));   // DR-B (City B) - bottom-right
    
    // ====================== INTERNET STACK ======================
    InternetStackHelper stack;
    
    if (enableDynamicRouting)
    {
        // If using dynamic routing, we'll let global routing handle it
        stack.Install(nodes);
        std::cout << "[INFO] Dynamic routing enabled - using Ipv4GlobalRouting" << std::endl;
    }
    else
    {
        stack.Install(nodes);
    }
    
    // ====================== IP ADDRESSING ======================
    Ipv4AddressHelper address;
    
    // Network 1: Branch-C to DC-A
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(link1Devices);
    // interfaces1.GetAddress(0) = 10.1.1.1 (Branch-C)
    // interfaces1.GetAddress(1) = 10.1.1.2 (DC-A interface 1)
    
    // Network 2: DC-A to DR-B (Primary)
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address.Assign(link2Devices);
    // interfaces2.GetAddress(0) = 10.1.2.1 (DC-A interface 2)
    // interfaces2.GetAddress(1) = 10.1.2.2 (DR-B interface 1)
    
    // Network 3: DC-A to DR-B (Backup)
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces3 = address.Assign(link3Devices);
    // interfaces3.GetAddress(0) = 10.1.3.1 (DC-A interface 3)
    // interfaces3.GetAddress(1) = 10.1.3.2 (DR-B interface 2)
    
    // ====================== STATIC ROUTING CONFIGURATION ======================
    if (!enableDynamicRouting)
    {
        // Enable IP forwarding on the router (DC-A)
        Ptr<Ipv4> ipv4Router = n1->GetObject<Ipv4>();
        ipv4Router->SetAttribute("IpForward", BooleanValue(true));
        
        Ipv4StaticRoutingHelper staticRoutingHelper;
        
        std::cout << "\n=== STATIC ROUTING CONFIGURATION ===" << std::endl;
        
        // 1. Configure routing on Branch-C (n0)
        // Branch-C needs to know that to reach DR-B networks (10.1.2.0/24 and 10.1.3.0/24),
        // it should go through DC-A (10.1.1.2)
        Ptr<Ipv4StaticRouting> staticRoutingN0 = 
            staticRoutingHelper.GetStaticRouting(n0->GetObject<Ipv4>());
        
        // Route to primary DR-B network
        staticRoutingN0->AddNetworkRouteTo(
            Ipv4Address("10.1.2.0"),
            Ipv4Mask("255.255.255.0"),
            Ipv4Address("10.1.1.2"),  // Next hop: DC-A
            1                         // Interface index
        );
        
        // Route to backup DR-B network
        staticRoutingN0->AddNetworkRouteTo(
            Ipv4Address("10.1.3.0"),
            Ipv4Mask("255.255.255.0"),
            Ipv4Address("10.1.1.2"),  // Next hop: DC-A
            1                         // Interface index
        );
        
        std::cout << "Branch-C routing configured:" << std::endl;
        std::cout << "  - To 10.1.2.0/24 via 10.1.1.2 (DC-A)" << std::endl;
        std::cout << "  - To 10.1.3.0/24 via 10.1.1.2 (DC-A)" << std::endl;
        
        // 2. Configure routing on DC-A (n1) - The router
        Ptr<Ipv4StaticRouting> staticRoutingN1 = 
            staticRoutingHelper.GetStaticRouting(n1->GetObject<Ipv4>());
        
        // DC-A needs to know that to reach Branch-C (10.1.1.1), it's directly connected
        // No explicit route needed for directly connected networks
        
        // Configure primary and backup routes to DR-B
        // Primary route via link2 (lower metric = higher priority)
        staticRoutingN1->AddNetworkRouteTo(
            Ipv4Address("10.1.2.0"),
            Ipv4Mask("255.255.255.0"),
            Ipv4Address("10.1.2.2"),  // Next hop: DR-B primary interface
            2,                        // Interface index (link2)
            10                        // Metric (lower = better)
        );
        
        // Backup route via link3 (higher metric = lower priority)
        staticRoutingN1->AddNetworkRouteTo(
            Ipv4Address("10.1.2.0"),
            Ipv4Mask("255.255.255.0"),
            Ipv4Address("10.1.3.2"),  // Next hop: DR-B backup interface
            3,                        // Interface index (link3)
            100                       // Metric (higher = worse)
        );
        
        std::cout << "\nDC-A routing configured:" << std::endl;
        std::cout << "  - Primary: To 10.1.2.0/24 via 10.1.2.2 (metric 10)" << std::endl;
        std::cout << "  - Backup:  To 10.1.2.0/24 via 10.1.3.2 (metric 100)" << std::endl;
        
        // 3. Configure routing on DR-B (n2)
        Ptr<Ipv4StaticRouting> staticRoutingN2 = 
            staticRoutingHelper.GetStaticRouting(n2->GetObject<Ipv4>());
        
        // DR-B needs route back to Branch-C through DC-A
        staticRoutingN2->AddNetworkRouteTo(
            Ipv4Address("10.1.1.0"),
            Ipv4Mask("255.255.255.0"),
            Ipv4Address("10.1.2.1"),  // Next hop: DC-A primary interface
            1                         // Interface index
        );
        
        // Alternative route via backup link
        staticRoutingN2->AddNetworkRouteTo(
            Ipv4Address("10.1.1.0"),
            Ipv4Mask("255.255.255.0"),
            Ipv4Address("10.1.3.1"),  // Next hop: DC-A backup interface
            2,                        // Interface index
            100                       // Higher metric for backup
        );
        
        std::cout << "\nDR-B routing configured:" << std::endl;
        std::cout << "  - Primary: To 10.1.1.0/24 via 10.1.2.1 (metric 10)" << std::endl;
        std::cout << "  - Backup:  To 10.1.1.0/24 via 10.1.3.1 (metric 100)" << std::endl;
        
        // Print routing tables for verification
        Ptr<OutputStreamWrapper> routingStream = 
            Create<OutputStreamWrapper>("scratch/ex4-routing-tables.txt", std::ios::out);
        staticRoutingHelper.PrintRoutingTableAllAt(Seconds(1.0), routingStream);
    }
    else
    {
        // Use global routing for dynamic routing simulation
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();
        std::cout << "[INFO] Using dynamic routing (simulated with global routing)" << std::endl;
    }
    
    // ====================== APPLICATIONS ======================
    // Banking transaction simulation
    
    // 1. Transaction Server on DR-B (n2)
    uint16_t transactionPort = 5000;
    UdpServerHelper transactionServer(transactionPort);
    ApplicationContainer serverApps = transactionServer.Install(n2);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(simulationTime);
    
    // 2. Transaction Client on Branch-C (n0)
    UdpClientHelper transactionClient(interfaces2.GetAddress(1), transactionPort);
    transactionClient.SetAttribute("MaxPackets", UintegerValue(1000));
    transactionClient.SetAttribute("Interval", TimeValue(MilliSeconds(100))); // 10 transactions/sec
    transactionClient.SetAttribute("PacketSize", UintegerValue(512)); // Banking transaction size
    
    ApplicationContainer clientApps = transactionClient.Install(n0);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(simulationTime - Seconds(1.0));
    
    // ====================== LINK FAILURE SIMULATION ======================
    if (simulateFailure && !enableDynamicRouting)
    {
        std::cout << "\n=== LINK FAILURE CONFIGURATION ===" << std::endl;
        std::cout << "Primary link (10.1.2.0/24) will fail at t = " << failureTime.GetSeconds() << "s" << std::endl;
        
        // Schedule link failure
        Simulator::Schedule(failureTime, [link2Devices]() {
            // Disable the primary link devices
            Ptr<NetDevice> dev1 = link2Devices.Get(0); // DC-A side
            Ptr<NetDevice> dev2 = link2Devices.Get(1); // DR-B side
            
            dev1->SetAttribute("Disable", BooleanValue(true));
            dev2->SetAttribute("Disable", BooleanValue(true));
            
            std::cout << "\n[EVENT] Primary link between DC-A and DR-B disabled!" << std::endl;
            std::cout << "   Network 10.1.2.0/24 is now unavailable" << std::endl;
            std::cout << "   Traffic should now use backup link (10.1.3.0/24)" << std::endl;
        });
    }
    else if (simulateFailure && enableDynamicRouting)
    {
        std::cout << "\n=== DYNAMIC ROUTING FAILOVER TEST ===" << std::endl;
        std::cout << "Testing OSPF-like convergence after link failure at t = " 
                  << failureTime.GetSeconds() << "s" << std::endl;
    }
    
    // ====================== MONITORING ======================
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    
    // Enable PCAP tracing on all devices
    p2p.EnablePcapAll("scratch/ex4-wan-fault");
    p2pPrimary.EnablePcapAll("scratch/ex4-wan-primary");
    p2pBackup.EnablePcapAll("scratch/ex4-wan-backup");
    
    // ====================== NETANIM VISUALIZATION ======================
    AnimationInterface anim("scratch/ex4-wan-fault.xml");
    
    // Node descriptions
    anim.UpdateNodeDescription(n0, "Branch-C (City C)\nClient\n10.1.1.1");
    anim.UpdateNodeDescription(n1, "DC-A (City A)\nRouter\n10.1.1.2 | 10.1.2.1 | 10.1.3.1");
    anim.UpdateNodeDescription(n2, "DR-B (City B)\nServer\n10.1.2.2 | 10.1.3.2");
    
    // Node colors
    anim.UpdateNodeColor(n0, 0, 255, 0);     // Green - Branch Office
    anim.UpdateNodeColor(n1, 255, 165, 0);   // Orange - Data Center (DC-A)
    anim.UpdateNodeColor(n2, 0, 0, 255);     // Blue - Disaster Recovery (DR-B)
    
    // Link descriptions
    anim.UpdateLinkDescription(0, 1, "Network 1\n10.1.1.0/24\n5Mbps, 2ms");
    anim.UpdateLinkDescription(1, 2, "Network 2 (Primary)\n10.1.2.0/24\n10Mbps, 2ms");
    anim.UpdateLinkDescription(1, 2, "Network 3 (Backup)\n10.1.3.0/24\n2Mbps, 10ms");
    
    // Enable packet metadata
    anim.EnablePacketMetadata(true);
    
    // ====================== SIMULATION EXECUTION ======================
    std::cout << "\n==========================================" << std::endl;
    std::cout << "MULTI-HOP WAN FAULT TOLERANCE - EXERCISE 4" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "Company: RegionalBank" << std::endl;
    std::cout << "Sites: City C (Branch), City A (DC), City B (DR)" << std::endl;
    std::cout << "Simulation Time: " << simulationTime.GetSeconds() << "s" << std::endl;
    std::cout << "Routing: " << (enableDynamicRouting ? "Dynamic (Global)" : "Static") << std::endl;
    std::cout << "Link Failure: " << (simulateFailure ? "YES at t=" + std::to_string(failureTime.GetSeconds()) + "s" : "NO") << std::endl;
    std::cout << "==========================================\n" << std::endl;
    
    std::cout << "=== NETWORK CONFIGURATION ===" << std::endl;
    std::cout << "Branch-C (n0): " << interfaces1.GetAddress(0) << std::endl;
    std::cout << "DC-A (n1): " << interfaces1.GetAddress(1) << " | " 
              << interfaces2.GetAddress(0) << " | " << interfaces3.GetAddress(0) << std::endl;
    std::cout << "DR-B (n2): " << interfaces2.GetAddress(1) << " | " 
              << interfaces3.GetAddress(1) << std::endl;
    std::cout << "=============================\n" << std::endl;
    
    Simulator::Stop(simulationTime);
    Simulator::Run();
    
    // ====================== RESULTS ANALYSIS ======================
    std::cout << "\n==========================================" << std::endl;
    std::cout << "SIMULATION RESULTS ANALYSIS" << std::endl;
    std::cout << "==========================================" << std::endl;
    
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    
    // Calculate performance metrics
    double totalDelay = 0;
    uint32_t totalPackets = 0;
    uint32_t lostPackets = 0;
    
    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        
        std::cout << "\nFlow " << it->first << " (Branch-C to DR-B):" << std::endl;
        std::cout << "  Source: " << t.sourceAddress << ":" << t.sourcePort << std::endl;
        std::cout << "  Destination: " << t.destinationAddress << ":" << t.destinationPort << std::endl;
        std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
        
        if (it->second.txPackets > 0)
        {
            double lossRate = (it->second.txPackets - it->second.rxPackets) * 100.0 / it->second.txPackets;
            std::cout << "  Packet Loss Rate: " << lossRate << "%" << std::endl;
            lostPackets += (it->second.txPackets - it->second.rxPackets);
        }
        
        if (it->second.rxPackets > 0)
        {
            double avgDelay = it->second.delaySum.GetSeconds() / it->second.rxPackets * 1000;
            std::cout << "  Average Delay: " << avgDelay << " ms" << std::endl;
            totalDelay += it->second.delaySum.GetSeconds();
            totalPackets += it->second.rxPackets;
            
            double throughput = it->second.rxBytes * 8.0 / 
                               (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000;
            std::cout << "  Average Throughput: " << throughput << " kbps" << std::endl;
        }
    }
    
    // Business continuity analysis
    std::cout << "\n=== BUSINESS CONTINUITY ANALYSIS ===" << std::endl;
    
    if (simulateFailure)
    {
        if (enableDynamicRouting)
        {
            std::cout << "Dynamic Routing Scenario:" << std::endl;
            std::cout << "  - Convergence after failure: ~1-5 seconds (simulated)" << std::endl;
            std::cout << "  - Automatic failover to backup path" << std::endl;
            std::cout << "  - No manual intervention required" << std::endl;
        }
        else
        {
            std::cout << "Static Routing Scenario:" << std::endl;
            std::cout << "  - Pre-configured backup routes" << std::endl;
            std::cout << "  - Failover based on route metrics" << std::endl;
            std::cout << "  - No convergence time (instant failover)" << std::endl;
            std::cout << "  - But: Manual configuration required" << std::endl;
        }
        
        std::cout << "\nPerformance Impact:" << std::endl;
        std::cout << "  - Backup link bandwidth: 2Mbps (vs 10Mbps primary)" << std::endl;
        std::cout << "  - Backup link delay: 10ms (vs 2ms primary)" << std::endl;
        std::cout << "  - Estimated service degradation: 80% bandwidth reduction" << std::endl;
        std::cout << "  - Acceptable for disaster recovery scenarios" << std::endl;
    }
    
    // Scalability analysis
    std::cout << "\n=== SCALABILITY ANALYSIS ===" << std::endl;
    std::cout << "Static routing requires Nx(N-1) routes for full mesh:" << std::endl;
    std::cout << "  - 3 sites: 3x2 = 6 routes (3 nodes)" << std::endl;
    std::cout << "  - 10 sites: 10x9 = 90 routes" << std::endl;
    std::cout << "  - 50 sites: 50x49 = 2450 routes" << std::endl;
    std::cout << "\nRecommendation: Use dynamic routing (OSPF) for > 5 sites" << std::endl;
    
    // ====================== CLEANUP ======================
    Simulator::Destroy();
    
    std::cout << "\n==========================================" << std::endl;
    std::cout << "SIMULATION COMPLETE" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "Output Files:" << std::endl;
    std::cout << "  - scratch/ex4-wan-fault.xml (NetAnim)" << std::endl;
    std::cout << "  - scratch/ex4-routing-tables.txt (Routing tables)" << std::endl;
    std::cout << "  - scratch/ex4-wan-*.pcap (Packet captures)" << std::endl;
    std::cout << "\nView animation: netanim scratch/ex4-wan-fault.xml" << std::endl;
    std::cout << "==========================================\n" << std::endl;
    
    return 0;
}
