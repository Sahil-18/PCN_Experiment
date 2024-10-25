#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"
#include <iostream>

using namespace ns3;

std::ofstream q1Size;
std::ofstream q2Size;
std::ofstream throughput;
std::map<FlowId, uint32_t> TotalRxBytes;
std::vector<ApplicationContainer> onOffApps;
std::vector<ApplicationContainer> sinkApps;

void createApps(InetSocketAddress sinkAddress, Ptr<Node> source, Ptr<Node> dest, uint32_t dataRate, uint32_t packetSize, double startTime, double stopTime, int onTime, int offTime){
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", sinkAddress);
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(onTime) + "]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(offTime) + "]"));
    onOffHelper.SetAttribute("DataRate", StringValue(std::to_string(dataRate) + "Mbps"));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer app = onOffHelper.Install(source);
    app.Start(Seconds(startTime));
    app.Stop(Seconds(stopTime));

    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkAddress.GetPort()));
    ApplicationContainer sinkApp = sinkHelper.Install(dest);
    sinkApp.Start(Seconds(startTime));
    sinkApp.Stop(Seconds(stopTime));

    onOffApps.push_back(app);
    sinkApps.push_back(sinkApp);
}

void LogQueue1Size(Ptr<QueueDisc> queueDisc){
    uint32_t qsize = queueDisc->GetNPackets();
    q1Size << Simulator::Now().GetMilliSeconds() << "," << qsize << "\n";
    Simulator::Schedule(MilliSeconds(100), &LogQueue1Size, queueDisc);
}

void LogQueue2Size(Ptr<QueueDisc> queueDisc){
    uint32_t qsize = queueDisc->GetNPackets();
    q2Size << Simulator::Now().GetMilliSeconds() << "," << qsize << "\n";
    Simulator::Schedule(MilliSeconds(100), &LogQueue2Size, queueDisc);
}

void LogThroughput(Ptr<FlowMonitor> monitor, Ptr<Ipv4FlowClassifier> classifier){
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for(std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); i++){
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if(TotalRxBytes.find(i->first) == TotalRxBytes.end()){
            TotalRxBytes[i->first] = 0;
        }
        uint32_t rxBytes = i->second.rxBytes - TotalRxBytes[i->first];
        TotalRxBytes[i->first] = i->second.rxBytes;
        // Bytes sent during 100 ms interval, throughput in Mbps
        double throughput_ = (rxBytes * 8.0) / 1e5;
        throughput << Simulator::Now().GetMilliSeconds() << "," << t.sourceAddress << "," << t.sourcePort << "," << t.destinationAddress << "," << t.destinationPort << "," << throughput_ << "\n";
    }

    Simulator::Schedule(MilliSeconds(100), &LogThroughput, monitor, classifier);
}

int main(){
    // Create nodes
    NodeContainer worker, ps, router, background;
    worker.Create(2);
    ps.Create(1);
    router.Create(2);
    background.Create(4);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpCubic"));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(1));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    // Create links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("200us"));
    p2p.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));

    // Connect nodes
    NetDeviceContainer w1r1, w2r1, r1r2, psr2, b1r1, b2r1, b3r2, b4r2;
    w1r1 = p2p.Install(router.Get(0), worker.Get(0));
    w2r1 = p2p.Install(router.Get(0), worker.Get(1));

    r1r2 = p2p.Install(router.Get(0), router.Get(1));

    psr2 = p2p.Install(router.Get(1), ps.Get(0));

    b1r1 = p2p.Install(router.Get(0), background.Get(0));
    b2r1 = p2p.Install(router.Get(0), background.Get(1));

    b3r2 = p2p.Install(router.Get(1), background.Get(2));
    b4r2 = p2p.Install(router.Get(1), background.Get(3));

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(worker);
    stack.Install(ps);
    stack.Install(router);
    stack.Install(background);

    // Install Traffic Control for observing queue sizes
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc", "MaxSize", StringValue("100p"));
    
    QueueDiscContainer qd = tch.Install(r1r2);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0","255.255.255.0");
    Ipv4InterfaceContainer w1r1Iface = address.Assign(w1r1);

    address.SetBase("192.168.2.0","255.255.255.0");
    Ipv4InterfaceContainer w2r1Iface = address.Assign(w2r1);

    address.SetBase("192.168.3.0","255.255.255.0");
    Ipv4InterfaceContainer psr2Iface = address.Assign(psr2);

    address.SetBase("192.168.4.0","255.255.255.0");
    Ipv4InterfaceContainer r1r2Iface = address.Assign(r1r2);

    address.SetBase("192.168.5.0","255.255.255.0");
    Ipv4InterfaceContainer b1r1Iface = address.Assign(b1r1);

    address.SetBase("192.168.6.0","255.255.255.0");
    Ipv4InterfaceContainer b2r1Iface = address.Assign(b2r1);

    address.SetBase("192.168.7.0","255.255.255.0");
    Ipv4InterfaceContainer b3r2Iface = address.Assign(b3r2);

    address.SetBase("192.168.8.0","255.255.255.0");
    Ipv4InterfaceContainer b4r2Iface = address.Assign(b4r2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create flows
    uint16_t port = 9;
    // Worker 1 to PS
    createApps(InetSocketAddress(psr2Iface.GetAddress(1), port), worker.Get(0), ps.Get(0), 900, 1500, 0.0, 10.0, 1, 1);
    // Worker 2 to PS
    createApps(InetSocketAddress(psr2Iface.GetAddress(1), port+1), worker.Get(1), ps.Get(0), 900, 1500, 0.0, 10.0, 1, 1);
    // Background 1 to background 2 and background 3 to background 4
    createApps(InetSocketAddress(b2r1Iface.GetAddress(1), port), background.Get(0), background.Get(1), 100, 1500, 0.5, 10.0, 1, 0);
    createApps(InetSocketAddress(b4r2Iface.GetAddress(1), port), background.Get(3), background.Get(2), 100, 1500, 0.5, 10.0, 1, 0);

    // // Background 1 to background 3, 4
    port++;
    createApps(InetSocketAddress(b3r2Iface.GetAddress(1), port), background.Get(0), background.Get(2), 175, 1500, 0.5, 10.0, 1, 0);
    createApps(InetSocketAddress(b4r2Iface.GetAddress(1), port), background.Get(0), background.Get(3), 175, 1500, 0.5, 10.0, 1, 0);

    // // Background 2 to background 3, 4
    port++;
    createApps(InetSocketAddress(b3r2Iface.GetAddress(1), port), background.Get(1), background.Get(2), 175, 1500, 0.5, 10.0, 1, 0);
    createApps(InetSocketAddress(b4r2Iface.GetAddress(1), port), background.Get(1), background.Get(3), 175, 1500, 0.5, 10.0, 1, 0);


    q1Size.open("q1Size.csv");
    q1Size << "Time(ms),QueueSize(Packets)\n";

    q2Size.open("q2Size.csv");
    q2Size << "Time(ms),QueueSize(Packets)\n";

    throughput.open("throughput.csv");
    throughput << "Time(ms),Source IP, Source Port, Dest IP, Dest Port,Throughput(Mbps)\n";

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());

    Simulator::Schedule(MilliSeconds(100), &LogQueue1Size, qd.Get(0));
    Simulator::Schedule(MilliSeconds(100), &LogQueue2Size, qd.Get(1));
    Simulator::Schedule(MilliSeconds(100), &LogThroughput, monitor, classifier);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    Simulator::Destroy();

    q1Size.close();
    q2Size.close();
    throughput.close();
}