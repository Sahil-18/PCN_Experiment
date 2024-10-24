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

    // Worker send data to PS with 900 Mbps for 0.5 seconds and 2 seconds off
    // Once the PS receives data, it pauses for 0.2 seconds and sends data to 
    // both worker nodes with 900 Mbps for 0.5 seconds and 2 seconds off

    // Worker 1 to PS 
    uint16_t port = 9;
    OnOffHelper onOffHelper1("ns3::TcpSocketFactory", InetSocketAddress(psri.GetAddress(0), port));
    onOffHelper1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=2]"));
    onOffHelper1.SetAttribute("DataRate", StringValue("1Gbps"));
    onOffHelper1.SetAttribute("PacketSize", UintegerValue(1500));

    ApplicationContainer worker1ToPS = onOffHelper1.Install(worker.Get(0));
    worker1ToPS.Start(Seconds(0.0));
    worker1ToPS.Stop(Seconds(10.0));


    PacketSinkHelper sinkHelper1("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp1 = sinkHelper1.Install(ps.Get(0));
    sinkApp1.Start(Seconds(0.0));
    sinkApp1.Stop(Seconds(10.0));

    // Worker 2 to PS
    port++;
    OnOffHelper onOffHelper2("ns3::TcpSocketFactory", InetSocketAddress(psri.GetAddress(0), port));
    onOffHelper2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=2]"));
    onOffHelper2.SetAttribute("DataRate", StringValue("1Gbps"));
    onOffHelper2.SetAttribute("PacketSize", UintegerValue(1500));

    ApplicationContainer worker2ToPS = onOffHelper2.Install(worker.Get(1));
    worker2ToPS.Start(Seconds(0.0));
    worker2ToPS.Stop(Seconds(10.0));

    PacketSinkHelper sinkHelper2("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp2 = sinkHelper2.Install(ps.Get(0));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(10.0));

    // PS to Worker 1
    port++;
    OnOffHelper onOffHelper3("ns3::TcpSocketFactory", InetSocketAddress(w1ri.GetAddress(0), port));
    onOffHelper3.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper3.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=2]"));
    onOffHelper3.SetAttribute("DataRate", StringValue("1Gbps"));
    onOffHelper3.SetAttribute("PacketSize", UintegerValue(1500));

    ApplicationContainer psToWorker1 = onOffHelper3.Install(ps.Get(0));
    psToWorker1.Start(Seconds(1.2));
    psToWorker1.Stop(Seconds(10.0));

    PacketSinkHelper sinkHelper3("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp3 = sinkHelper3.Install(worker.Get(0));
    sinkApp3.Start(Seconds(1.2));
    sinkApp3.Stop(Seconds(10.0));

    // PS to Worker 2
    port++;
    OnOffHelper onOffHelper4("ns3::TcpSocketFactory", InetSocketAddress(w2ri.GetAddress(0), port));
    onOffHelper4.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper4.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=2]"));
    onOffHelper4.SetAttribute("DataRate", StringValue("1Gbps"));
    onOffHelper4.SetAttribute("PacketSize", UintegerValue(1500));

    ApplicationContainer psToWorker2 = onOffHelper4.Install(ps.Get(0));
    psToWorker2.Start(Seconds(1.2));
    psToWorker2.Stop(Seconds(10.0));

    PacketSinkHelper sinkHelper4("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp4 = sinkHelper4.Install(worker.Get(1));
    sinkApp4.Start(Seconds(1.2));
    sinkApp4.Stop(Seconds(10.0));

    // Background sends data to each other continuously for 10 seconds with 100 Mbps
    // port++;
    // OnOffHelper onOffHelper5("ns3::TcpSocketFactory", InetSocketAddress(rb2i.GetAddress(1), port));
    // onOffHelper5.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    // onOffHelper5.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    // onOffHelper5.SetAttribute("DataRate", StringValue("500Mbps"));
    // onOffHelper5.SetAttribute("PacketSize", UintegerValue(1500));

    // ApplicationContainer background1 = onOffHelper5.Install(background.Get(0));
    // background1.Start(Seconds(0.0));
    // background1.Stop(Seconds(10.0));

    // PacketSinkHelper sinkHelper5("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    // ApplicationContainer sinkApp5 = sinkHelper5.Install(background.Get(1));
    // sinkApp5.Start(Seconds(0.0));
    // sinkApp5.Stop(Seconds(10.0));

    // port++;
    // OnOffHelper onOffHelper6("ns3::TcpSocketFactory", InetSocketAddress(rb1i.GetAddress(1), port));
    // onOffHelper6.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    // onOffHelper6.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    // onOffHelper6.SetAttribute("DataRate", StringValue("500Mbps"));
    // onOffHelper6.SetAttribute("PacketSize", UintegerValue(1500));

    // ApplicationContainer background2 = onOffHelper6.Install(background.Get(1));
    // background2.Start(Seconds(0.0));
    // background2.Stop(Seconds(10.0));
    
    // PacketSinkHelper sinkHelper6("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    // ApplicationContainer sinkApp6 = sinkHelper6.Install(background.Get(0));
    // sinkApp6.Start(Seconds(0.0));
    // sinkApp6.Stop(Seconds(10.0));

    queueSizes.open("queueSizes.csv");
    queueSizes << "Time(s),QueueSize(Packets)\n";

    throughput.open("throughput.csv");
    throughput << "Time(s),Source IP, Source Port, Dest IP, Dest Port,Throughput(Mbps)\n";

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