/*
 * ISN 3132 - Exercise 2 Simplified Version
 * Basic Traffic Differentiation without complex QoS
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WAN_QoS_Simple");

int main(int argc, char *argv[])
{
    // Basic parameters
    Time simulationTime = Seconds(10.0);
    bool enableNetAnim = true;
    
    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Simulation time", simulationTime);
    cmd.AddValue("netanim", "Enable NetAnim", enableNetAnim);
    cmd.Parse(argc, argv);
    
    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);
    
    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    
    // Create topology
    // n0 (Client) --- n1 (Router) --- n2 (Server)
    //                     |
    //                     n3 (FTP Server)
    
    NetDeviceContainer d0 = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(1)));
    NetDeviceContainer d1 = p2p.Install(NodeContainer(nodes.Get(1), nodes.Get(2)));
    NetDeviceContainer d2 = p2p.Install(NodeContainer(nodes.Get(1), nodes.Get(3)));
    
    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);
    
    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0 = ipv4.Assign(d0);
    
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1 = ipv4.Assign(d1);
    
    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2 = ipv4.Assign(d2);
    
    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
    // ========== TRAFFIC GENERATION ==========
    
    // 1. UDP traffic (simulating VoIP)
    uint16_t udpPort = 4000;
    
    // UDP Server on n2
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer udpServerApp = udpServer.Install(nodes.Get(2));
    udpServerApp.Start(Seconds(1.0));
    udpServerApp.Stop(simulationTime);
    
    // UDP Client on n0 (small packets, frequent)
    UdpClientHelper udpClient(i1.GetAddress(1), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
    udpClient.SetAttribute("PacketSize", UintegerValue(160));
    
    ApplicationContainer udpClientApp = udpClient.Install(nodes.Get(0));
    udpClientApp.Start(Seconds(2.0));
    udpClientApp.Stop(simulationTime - Seconds(1.0));
    
    // 2. TCP traffic (simulating FTP)
    uint16_t tcpPort = 4001;
    
    // TCP Server on n3
    PacketSinkHelper tcpServer("ns3::TcpSocketFactory", 
                               InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer tcpServerApp = tcpServer.Install(nodes.Get(3));
    tcpServerApp.Start(Seconds(1.0));
    tcpServerApp.Stop(simulationTime);
    
    // TCP Client on n0 (large packets)
    BulkSendHelper tcpClient("ns3::TcpSocketFactory",
                            InetSocketAddress(i2.GetAddress(1), tcpPort));
    tcpClient.SetAttribute("MaxBytes", UintegerValue(5000000));
    
    ApplicationContainer tcpClientApp = tcpClient.Install(nodes.Get(0));
    tcpClientApp.Start(Seconds(3.0));
    tcpClientApp.Stop(simulationTime - Seconds(2.0));
    
    // ========== MONITORING ==========
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    
    // Enable PCAP tracing
    p2p.EnablePcap("simple-wan", d1.Get(0), true);
    
    // ========== NETANIM ==========
    if (enableNetAnim)
    {
        AnimationInterface anim("simple-wan-animation.xml");
        
        // Set positions
        anim.SetConstantPosition(nodes.Get(0), 10, 50);
        anim.SetConstantPosition(nodes.Get(1), 50, 50);
        anim.SetConstantPosition(nodes.Get(2), 90, 30);
        anim.SetConstantPosition(nodes.Get(3), 90, 70);
        
        // Set descriptions
        anim.UpdateNodeDescription(0, "Client");
        anim.UpdateNodeDescription(1, "Router");
        anim.UpdateNodeDescription(2, "UDP Server");
        anim.UpdateNodeDescription(3, "TCP Server");
        
        // Set colors
        anim.UpdateNodeColor(0, 0, 255, 0);
        anim.UpdateNodeColor(1, 255, 255, 0);
        anim.UpdateNodeColor(2, 0, 0, 255);
        anim.UpdateNodeColor(3, 255, 0, 0);
        
        anim.EnablePacketMetadata(true);
    }
    
    // ========== SIMULATION ==========
    std::cout << "\nStarting simulation..." << std::endl;
    std::cout << "Time: " << simulationTime.GetSeconds() << " seconds" << std::endl;
    
    Simulator::Stop(simulationTime);
    Simulator::Run();
    
    // ========== RESULTS ==========
    monitor->CheckForLostPackets();
    
    std::cout << "\n=== Simulation Results ===" << std::endl;
    
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    
    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        
        std::cout << "\nFlow " << it->first << ": ";
        std::cout << t.sourceAddress << ":" << t.sourcePort << " -> ";
        std::cout << t.destinationAddress << ":" << t.destinationPort << std::endl;
        
        std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
        
        if (it->second.rxPackets > 0)
        {
            double delay = it->second.delaySum.GetSeconds() / it->second.rxPackets * 1000;
            std::cout << "  Average Delay: " << delay << " ms" << std::endl;
            
            if (t.destinationPort == udpPort)
            {
                double jitter = it->second.jitterSum.GetSeconds() / it->second.rxPackets * 1000;
                std::cout << "  Average Jitter: " << jitter << " ms" << std::endl;
            }
        }
    }
    
    Simulator::Destroy();
    
    std::cout << "\nSimulation complete!" << std::endl;
    if (enableNetAnim)
    {
        std::cout << "Animation file: simple-wan-animation.xml" << std::endl;
    }
    
    return 0;
}
