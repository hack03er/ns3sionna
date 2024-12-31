/*
 * Copyright (c) 2024 Zubow
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#include "lib/sionna-helper.h"
#include "lib/sionna-mobility-model.h"
#include "lib/sionna-propagation-cache.h"
#include "lib/sionna-propagation-delay-model.h"
#include "lib/sionna-propagation-loss-model.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-net-device.h"
#include "../../src/wifi/model/yans-wifi-phy.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ExampleMunichMobilitySionna");

double get_center_freq(Ptr<NetDevice> nd)
{
    Ptr<WifiPhy> wp = nd->GetObject<WifiNetDevice>()->GetPhy();
    return wp->GetObject<YansWifiPhy>()->GetFrequency() * 1e6;
}

double get_channel_width(Ptr<NetDevice> nd)
{
    Ptr<WifiPhy> wp = nd->GetObject<WifiNetDevice>()->GetPhy();
    return wp->GetObject<YansWifiPhy>()->GetChannelWidth() * 1e6;
}

uint32_t
ContextToNodeId(std::string context)
{
    std::string sub = context.substr(10);
    uint32_t pos = sub.find("/Device");
    return std::stoi(sub.substr(0, pos));
}

void
PhyRxOkTrace(std::string context,
             Ptr<const Packet> p,
             double snr,
             WifiMode mode,
             WifiPreamble preamble)
{
    double snrDb = 10 * std::log10(snr);

    std::cout << "PHY-RX-OK time=" << Simulator::Now().As(Time::S) << " node="
              << ContextToNodeId(context) << " size=" << p->GetSize()
              << " snr=" << snrDb << "db, mode=" << mode
              << " preamble=" << preamble << std::endl;
}

void
TracePacketReception(std::string context,
                     Ptr<const Packet> p,
                     uint16_t channelFreqMhz,
                     WifiTxVector txVector,
                     MpduInfo aMpdu,
                     SignalNoiseDbm signalNoise,
                     uint16_t staId)
{
    std::cout << "Trace: nodeId=" << staId << ", signal=" << signalNoise.signal << "dBm " <<
        "noise=" << signalNoise.noise << "dBm" << std::endl;
}

void
RunSimulation(SionnaHelper &sionnaHelper,
              const bool caching,
              const uint32_t seed,
              const int wifi_channel_num,
              const int channel_width,
              const bool verbose)
{
    std::cout << "New simulation with seed " << seed << std::endl << std::endl;
    RngSeedManager::SetSeed(seed);

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel>();

    Ptr<SionnaPropagationCache> propagationCache = CreateObject<SionnaPropagationCache>();
    propagationCache->SetSionnaHelper(sionnaHelper);
    propagationCache->SetCaching(caching);

    Ptr<SionnaPropagationDelayModel> delayModel = CreateObject<SionnaPropagationDelayModel>();
    delayModel->SetPropagationCache(propagationCache);

    Ptr<SionnaPropagationLossModel> lossModel = CreateObject<SionnaPropagationLossModel>();
    lossModel->SetPropagationCache(propagationCache);

    channel->SetPropagationLossModel(lossModel);
    channel->SetPropagationDelayModel(delayModel);

    YansWifiPhyHelper phy;
    phy.SetChannel(channel);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    WifiHelper wifi;

    WifiStandard wifi_standard = WIFI_STANDARD_80211ac;
    wifi.SetStandard(wifi_standard);
    std::string channelStr = "{" + std::to_string(wifi_channel_num) + ", " + std::to_string(channel_width) + ", BAND_5GHZ, 0}";
    phy.Set("ChannelSettings", StringValue(channelStr));
    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");

    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy, mac, wifiStaNode);

    NetDeviceContainer apDevices;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconGeneration", BooleanValue(true), "BeaconInterval", TimeValue(Seconds(5.120)), "EnableBeaconJitter", BooleanValue(false));
    apDevices = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;

    mobility.SetMobilityModel("ns3::SionnaMobilityModel");
    mobility.Install(wifiApNode);

    mobility.SetMobilityModel("ns3::SionnaMobilityModel",
                              "Model",
                              EnumValue(SionnaMobilityModel::MODEL_RANDOM_WALK),
                              "Speed",
                              StringValue("ns3::ConstantRandomVariable[Constant=7.0]"),
                              "Distance",
                              DoubleValue(50));
    mobility.Install(wifiStaNode);

    wifiStaNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(45.0, 90.0, 1.5));
    wifiApNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(8.5, 21.0, 27.0));

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiStaInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer wifiApInterfaces = address.Assign(apDevices);

    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverApps = echoServer.Install(wifiStaNode);
    serverApps.Start(Seconds(0.5));
    serverApps.Stop(Seconds(300.0));

    Config::Connect(
        "/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/MonitorSnifferRx",
        MakeCallback(&TracePacketReception));

    // Trace PHY Rx success events
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/State/RxOk",
                    MakeCallback(&PhyRxOkTrace));

    UdpEchoClientHelper echoClient(Ipv4Address ("255.255.255.255"), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(wifiApNode);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(300.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // set center frequency & bandwidth for Sionna
    sionnaHelper.Configure(get_center_freq(apDevices.Get(0)), get_channel_width(apDevices.Get(0)));

    if (verbose)
    {
        std::cout << "----------Node Information----------" << std::endl;
        NodeContainer c = NodeContainer::GetGlobal();
        for (auto iter = c.Begin(); iter != c.End(); ++iter)
        {
            std::cout << "NodeID: " << (*iter)->GetId() << ", ";

            Ptr<MobilityModel> mobilityModel = (*iter)->GetObject<MobilityModel>();
            if (mobilityModel)
            {
                std::cout << mobilityModel->GetInstanceTypeId().GetName() << " (";
                Vector position = mobilityModel->GetPosition();
                Vector velocity = mobilityModel->GetVelocity();
                std::cout << "Pos: [" << position.x << ", " << position.y << ", " << position.z << "]" << ", ";
                std::cout << "Vel: [" << velocity.x << ", " << velocity.y << ", " << velocity.z << "]";

                Ptr<SionnaMobilityModel> sionnaMobilityModel = DynamicCast<SionnaMobilityModel>(mobilityModel);
                if (sionnaMobilityModel)
                {
                    std::cout << ", " << "Model: " << sionnaMobilityModel->GetModel() << ", ";
                    std::cout << "Mode: " << sionnaMobilityModel->GetMode() << ", ";
                    std::cout << "ModeTime: " << sionnaMobilityModel->GetModeTime().GetSeconds() << ", ";
                    std::cout << "ModeDistance: " << sionnaMobilityModel->GetModeDistance() << ", ";
                    std::cout << "Speed: " << sionnaMobilityModel->GetSpeed()->GetInstanceTypeId().GetName() << ", ";
                    std::cout << "Direction: " << sionnaMobilityModel->GetDirection()->GetInstanceTypeId().GetName();
                }

                std::cout << ")" << std::endl;
            }
            else
            {
                std::cout << "No MobilityModel" << std::endl;
            }
        }
    }

    Simulator::Stop(Seconds(300.0));

    sionnaHelper.Start();

    Simulator::Run();
    Simulator::Destroy();

    std::cout << std::endl << std::endl;
}

int
main(int argc, char* argv[])
{
    bool verbose = true;
    bool caching = true;
    std::string environment = "munich/munich.xml";
    int wifi_channel_num = 40;
    int channel_width = 20;
    uint32_t numseeds = 1;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Enable logging", verbose);
    cmd.AddValue("caching", "Enable caching of propagation delay and loss", caching);
    cmd.AddValue("environment", "Xml file of environment", environment);
    cmd.AddValue("channel", "The WiFi channel number", wifi_channel_num);
    cmd.AddValue("channelWidth", "The WiFi channel width in MHz", channel_width);
    cmd.AddValue("numseeds", "Number of seeds", numseeds);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoClientApplication", LOG_PREFIX_TIME);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_PREFIX_TIME);
        LogComponentEnable("YansWifiChannel", LOG_DEBUG);
        LogComponentEnable("YansWifiChannel", LOG_PREFIX_TIME);
        LogComponentEnable("SionnaPropagationDelayModel", LOG_INFO);
        LogComponentEnable("SionnaPropagationDelayModel", LOG_PREFIX_TIME);
        LogComponentEnable("SionnaPropagationCache", LOG_INFO);
        LogComponentEnable("SionnaPropagationCache", LOG_PREFIX_TIME);
    }

    std::cout << "1 ap and 1 moving sta scenario with sionna" << std::endl << std::endl;
    std::cout << "Config: CH=" << wifi_channel_num << ",BW=" << channel_width << std::endl;

    std::string server_url = "tcp://localhost:5555";
    //std::string server_url = "tcp://localhost:5555";
    SionnaHelper sionnaHelper(environment, server_url);

    for (uint32_t seed = 1; seed <= numseeds; seed++)
    {
        RunSimulation(sionnaHelper, caching, seed, wifi_channel_num, channel_width, verbose);
    }

    sionnaHelper.Destroy();

    return 0;
}
