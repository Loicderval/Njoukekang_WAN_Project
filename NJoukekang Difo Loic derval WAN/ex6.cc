#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-static-routing-helper.h"

#include <sstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("InterAS_BGP");

/* ================= BGP DATA ================= */

struct BgpRoute
{
  Ipv4Address prefix;
  Ipv4Mask mask;
  std::vector<uint32_t> asPath;
};

class BgpSpeaker : public Object
{
public:
  static TypeId GetTypeId()
  {
    static TypeId tid = TypeId("BgpSpeaker")
      .SetParent<Object>()
      .AddConstructor<BgpSpeaker>();
    return tid;
  }

  BgpSpeaker() = default;
  explicit BgpSpeaker(uint32_t asn) : m_as(asn) {}

  void Initialize(Ptr<Node> n, Ipv4Address nextHop)
  {
    m_node = n;
    m_ipv4 = n->GetObject<Ipv4>();
    m_nextHop = nextHop;
  }

  void AddNeighbor(uint32_t asn)
  {
    m_neighbors.push_back(asn);
  }

  void Advertise(const BgpRoute &r)
  {
    NS_LOG_UNCOND("[BGP] AS" << m_as << " advertises "
                  << r.prefix << "/" << r.mask.GetPrefixLength());

    for (auto n : m_neighbors)
      NS_LOG_UNCOND("  -> sent to AS" << n);
  }

  void InstallRoute(Ipv4Address net, Ipv4Mask mask)
  {
    Ipv4StaticRoutingHelper helper;
    Ptr<Ipv4StaticRouting> rt = helper.GetStaticRouting(m_ipv4);
    rt->AddNetworkRouteTo(net, mask, m_nextHop, 1);
  }

private:
  uint32_t m_as{0};
  Ptr<Node> m_node;
  Ptr<Ipv4> m_ipv4;
  Ipv4Address m_nextHop;
  std::vector<uint32_t> m_neighbors;
};

/* ================= MAIN ================= */

int main(int argc, char *argv[])
{
  Time simTime = Seconds(25);
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(6);

  InternetStackHelper internet;
  internet.Install(nodes);

  /* === LINKS === */

  PointToPointHelper p2p, ixp;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  ixp.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
  ixp.SetChannelAttribute("Delay", StringValue("1ms"));

  // Internal links
  auto d01 = p2p.Install(nodes.Get(0), nodes.Get(1));
  auto d02 = p2p.Install(nodes.Get(0), nodes.Get(2));
  auto d34 = p2p.Install(nodes.Get(3), nodes.Get(4));
  auto d35 = p2p.Install(nodes.Get(3), nodes.Get(5));

  // IXPs
  auto ixpA = ixp.Install(nodes.Get(1), nodes.Get(4));
  auto ixpB = ixp.Install(nodes.Get(2), nodes.Get(5));

  /* === ADDRESSING === */

  Ipv4AddressHelper addr;

  addr.SetBase("10.1.1.0", "255.255.255.0");
  addr.Assign(d01);

  addr.SetBase("10.1.2.0", "255.255.255.0");
  addr.Assign(d02);

  addr.SetBase("10.2.1.0", "255.255.255.0");
  addr.Assign(d34);

  addr.SetBase("10.2.2.0", "255.255.255.0");
  addr.Assign(d35);

  addr.SetBase("192.168.1.0", "255.255.255.0");
  addr.Assign(ixpA);

  addr.SetBase("192.168.2.0", "255.255.255.0");
  addr.Assign(ixpB);

  /* === MOBILITY (NetAnim) === */

  MobilityHelper mob;
  mob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mob.Install(nodes);

  nodes.Get(0)->GetObject<MobilityModel>()->SetPosition({20,50,0});
  nodes.Get(1)->GetObject<MobilityModel>()->SetPosition({10,70,0});
  nodes.Get(2)->GetObject<MobilityModel>()->SetPosition({10,30,0});
  nodes.Get(3)->GetObject<MobilityModel>()->SetPosition({80,50,0});
  nodes.Get(4)->GetObject<MobilityModel>()->SetPosition({90,70,0});
  nodes.Get(5)->GetObject<MobilityModel>()->SetPosition({90,30,0});

  /* === BGP === */

  Ptr<BgpSpeaker> as65001 = CreateObject<BgpSpeaker>(65001);
  Ptr<BgpSpeaker> as65002 = CreateObject<BgpSpeaker>(65002);

  as65001->Initialize(nodes.Get(0), Ipv4Address("10.1.1.2"));
  as65002->Initialize(nodes.Get(3), Ipv4Address("10.2.1.2"));

  as65001->AddNeighbor(65002);
  as65002->AddNeighbor(65001);

  Simulator::Schedule(Seconds(2), [&] {
    BgpRoute r{Ipv4Address("10.1.0.0"), Ipv4Mask("255.255.0.0"), {65001}};
    as65001->Advertise(r);
    as65002->InstallRoute(r.prefix, r.mask);
  });

  Simulator::Schedule(Seconds(3), [&] {
    BgpRoute r{Ipv4Address("10.2.0.0"), Ipv4Mask("255.255.0.0"), {65002}};
    as65002->Advertise(r);
    as65001->InstallRoute(r.prefix, r.mask);
  });

  /* === ROUTE LEAK === */

  Simulator::Schedule(Seconds(10), [&] {
    NS_LOG_UNCOND("\n[SECURITY] ROUTE LEAK OCCURRED");
    as65002->InstallRoute(Ipv4Address("10.1.0.0"), Ipv4Mask("255.255.0.0"));
  });

  /* === ROUTING TABLE DUMP === */

  Simulator::Schedule(Seconds(12), [&] {
    NS_LOG_UNCOND("\n=== ROUTING TABLES ===");
    Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt(
      Seconds(12), Create<OutputStreamWrapper>(&std::cout));
  });

  /* === UDP TRAFFIC === */

  uint16_t port = 9000;

  UdpServerHelper server(port);
  server.Install(nodes.Get(3))->Start(Seconds(1));

  UdpClientHelper client(Ipv4Address("10.2.1.1"), port);
  client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("PacketSize", UintegerValue(512));

  client.Install(nodes.Get(0))->Start(Seconds(5));

  /* === OUTPUT === */

  AnimationInterface anim("scratch/ex6-interas.xml");
  anim.EnablePacketMetadata(true);

  p2p.EnablePcapAll("scratch/ex6-internal");
  ixp.EnablePcapAll("scratch/ex6-ixp");

  Simulator::Stop(simTime);
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}
