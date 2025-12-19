#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ex5PacketClassification");

// ---------------- Packet Classifier ----------------
class PBRPolicyEngine
{
public:
  static uint32_t ClassifyPacket(Ptr<const Packet> packet, const Ipv4Header &ipHeader)
  {
    // Make a copy because PeekHeader advances the buffer
    Ptr<Packet> copy = packet->Copy();

    // -------- UDP --------
    if (ipHeader.GetProtocol() == 17) // UDP
    {
      UdpHeader udp;
      if (copy->PeekHeader(udp))
      {
        uint16_t dport = udp.GetDestinationPort();

        if (dport >= 5000 && dport <= 5010)
          return 1; // High-priority service
        else if (dport >= 6000 && dport <= 6010)
          return 2; // Suspicious / attack traffic
      }
    }

    // -------- TCP --------
    if (ipHeader.GetProtocol() == 6) // TCP
    {
      TcpHeader tcp;
      if (copy->PeekHeader(tcp))
      {
        uint16_t dport = tcp.GetDestinationPort();

        if (dport == 80 || dport == 443)
          return 3; // Web traffic
      }
    }

    return 0; // Default / Best effort
  }
};

// ---------------- Main ----------------
int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  LogComponentEnable("Ex5PacketClassification", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices = p2p.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  ipv4.Assign(devices);

  // Sink
  PacketSinkHelper sink("ns3::UdpSocketFactory",
                        InetSocketAddress(Ipv4Address::GetAny(), 5001));
  sink.Install(nodes.Get(1));

  // Traffic generator
  OnOffHelper onoff("ns3::UdpSocketFactory",
                    InetSocketAddress(Ipv4Address("10.1.1.2"), 5001));
  onoff.SetAttribute("DataRate", StringValue("1Mbps"));
  onoff.SetAttribute("PacketSize", UintegerValue(512));
  onoff.Install(nodes.Get(0));

  Simulator::Stop(Seconds(5.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}
