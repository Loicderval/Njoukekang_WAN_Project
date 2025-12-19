#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ex3DdosIds");

// ---------------- IDS / Eavesdropping Logic ----------------
static uint64_t packetsCaptured = 0;
static uint64_t ddosThreshold = 200; // packets/sec

bool
PromiscEavesdrop(Ptr<NetDevice> device,
                 Ptr<const Packet> packet,
                 uint16_t protocol,
                 const Address &from,
                 const Address &to,
                 NetDevice::PacketType packetType)
{
    packetsCaptured++;

    if (packetsCaptured % 50 == 0)
    {
        std::cout << "[IDS] Time=" << Simulator::Now().GetSeconds()
                  << "s  Captured packets=" << packetsCaptured << std::endl;
    }

    return true; // required
}

// ---------------- Main ----------------
int
main(int argc, char *argv[])
{
    uint32_t numAttackers = 3;
    double simTime = 20.0;

    CommandLine cmd;
    cmd.AddValue("numAttackers", "Number of attacking nodes", numAttackers);
    cmd.Parse(argc, argv);

    // ---------------- Nodes ----------------
    NodeContainer victim;
    victim.Create(1);

    NodeContainer attackers;
    attackers.Create(numAttackers);

    NodeContainer router;
    router.Create(1);

    // ---------------- Internet Stack ----------------
    InternetStackHelper internet;
    internet.Install(victim);
    internet.Install(attackers);
    internet.Install(router);

    // ---------------- Links ----------------
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Victim <-> Router
    NetDeviceContainer vr = p2p.Install(victim.Get(0), router.Get(0));

    // Attackers <-> Router
    std::vector<NetDeviceContainer> attackerDevices(numAttackers);
    for (uint32_t i = 0; i < numAttackers; ++i)
    {
        attackerDevices[i] = p2p.Install(attackers.Get(i), router.Get(0));
    }

    // ---------------- IP Addressing ----------------
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer vrIf = ipv4.Assign(vr);

    std::vector<Ipv4InterfaceContainer> attackerIf(numAttackers);
    for (uint32_t i = 0; i < numAttackers; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.0." << i + 1 << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        attackerIf[i] = ipv4.Assign(attackerDevices[i]);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ---------------- Applications ----------------
    uint16_t victimPort = 9000;

    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          InetSocketAddress(Ipv4Address::GetAny(), victimPort));
    ApplicationContainer sinkApp = sink.Install(victim.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime));

    // DDoS traffic (high-rate UDP flood)
    for (uint32_t i = 0; i < numAttackers; ++i)
    {
        OnOffHelper attack("ns3::UdpSocketFactory",
                            InetSocketAddress(vrIf.GetAddress(0), victimPort));
        attack.SetAttribute("DataRate", DataRateValue(DataRate("3Mbps")));
        attack.SetAttribute("PacketSize", UintegerValue(1024));
        attack.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        attack.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

        ApplicationContainer app = attack.Install(attackers.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(simTime));
    }

    // ---------------- IDS (Promiscuous Mode) ----------------
    for (uint32_t i = 0; i < vr.GetN(); ++i)
    {
        vr.Get(i)->SetPromiscReceiveCallback(MakeCallback(&PromiscEavesdrop));
    }

    // ---------------- NetAnim ----------------
    AnimationInterface anim("ex3-ddos-ids.xml");
    anim.SetConstantPosition(victim.Get(0), 0.0, 10.0);
    anim.SetConstantPosition(router.Get(0), 20.0, 10.0);

    for (uint32_t i = 0; i < numAttackers; ++i)
    {
        anim.SetConstantPosition(attackers.Get(i), 10.0, 20.0 + i * 5);
    }

    // ---------------- Run ----------------
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    std::cout << "\nSimulation finished. Total packets captured by IDS: "
              << packetsCaptured << std::endl;

    return 0;
}
