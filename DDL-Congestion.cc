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

std::ofstream queueSizes;
std::ofstream throughput;
std::map<FlowId, uint32_t> TotalRxBytes;
std::vector<ApplicationContainer> onOffApps;
std::vector<ApplicationContainer> sinkApps;

void createApps(uint32_t port, Ipv4Address destIP, Ptr<Node> source, Ptr<Node> dest, uint32_t dataRate, uint32_t packetSize, double startTime, double stopTime, int onTime, int offTime){
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", InetSocketAddress(destIP, port));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(onTime) + "]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(offTime) + "]"));
    onOffHelper.SetAttribute("DataRate", StringValue(std::to_string(dataRate) + "Mbps"));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer app = onOffHelper.Install(source);
    app.Start(Seconds(startTime));
    app.Stop(Seconds(stopTime));

    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(dest);
    sinkApp.Start(Seconds(startTime));
    sinkApp.Stop(Seconds(stopTime));

    onOffApps.push_back(app);
    sinkApps.push_back(sinkApp);
}

void LogQueueSize(Ptr<QueueDisc> queueDisc){
    uint32_t qsize = queueDisc->GetNPackets();
    queueSizes << Simulator::Now().GetMilliSeconds() << "," << qsize << "\n";
    Simulator::Schedule(MilliSeconds(100), &LogQueueSize, queueDisc);
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
    NodeContainer worker, ps, router, background;
    worker.Create(2);
    ps.Create(1);
    router.Create(1);
    background.Create(2);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpCubic"));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(1));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    Config::SetDefault("ns3::RedQueueDisc::UseEcn", BooleanValue(true));
    Config::SetDefault("ns3::RedQueueDisc::UseHardDrop", BooleanValue(true));
    Config::SetDefault("ns3::RedQueueDisc::MeanPktSize", UintegerValue(1500));
    Config::SetDefault("ns3::RedQueueDisc::MaxSize", QueueSizeValue(QueueSize("100p")));
    Config::SetDefault("ns3::RedQueueDisc::MinTh", DoubleValue(30));
    Config::SetDefault("ns3::RedQueueDisc::MaxTh", DoubleValue(60));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10us"));

    NetDeviceContainer w1r, w2r, psr, rb1, rb2;
    w1r = p2p.Install(worker.Get(0), router.Get(0));
    w2r = p2p.Install(worker.Get(1), router.Get(0));
    psr = p2p.Install(ps.Get(0), router.Get(0));
    rb1 = p2p.Install(router.Get(0), background.Get(0));
    rb2 = p2p.Install(router.Get(0), background.Get(1));

    InternetStackHelper stack;
    stack.Install(worker);
    stack.Install(ps);
    stack.Install(router);
    stack.Install(background);

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::RedQueueDisc",
                         "MinTh", DoubleValue(30),
                         "MaxTh", DoubleValue(60),
                         "LinkBandwidth", StringValue("1Gbps"),
                         "LinkDelay", StringValue("10us"));
    
    QueueDiscContainer qd = tch.Install(psr);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer w1ri = address.Assign(w1r);

    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer w2ri = address.Assign(w2r);

    address.SetBase("192.168.3.0", "255.255.255.0");
    Ipv4InterfaceContainer psri = address.Assign(psr);

    address.SetBase("192.168.4.0", "255.255.255.0");
    Ipv4InterfaceContainer rb1i = address.Assign(rb1);

    address.SetBase("192.168.5.0", "255.255.255.0");
    Ipv4InterfaceContainer rb2i = address.Assign(rb2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();



    queueSizes.open("queueSizes.csv");
    queueSizes << "Time(ms),QueueSize(Packets)\n";

    throughput.open("throughput.csv");
    throughput << "Time(ms),Source IP, Source Port, Dest IP, Dest Port,Throughput(Mbps)\n";

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());

    Simulator::Schedule(MilliSeconds(100), &LogQueueSize, qd.Get(1));
    Simulator::Schedule(MilliSeconds(100), &LogThroughput, monitor, classifier);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    Simulator::Destroy();

    queueSizes.close();
    throughput.close();
}