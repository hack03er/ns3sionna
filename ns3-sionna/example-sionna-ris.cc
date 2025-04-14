/*
 * Author: Johannes Hack <j.hack@tu-berlin.de>
 */

// Sionna models
#include "lib/sionna-helper.h"
#include "lib/sionna-mobility-model.h"
#include "lib/sionna-propagation-cache.h"
#include "lib/sionna-propagation-delay-model.h"
#include "lib/sionna-propagation-loss-model.h"
#include "lib/sionna-ris-model.h"

// Ns-3 modules
#include "../../src/wifi/model/yans-wifi-phy.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/ssid.h"
#include "ns3/wifi-net-device.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ExampleRISSionna");

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

int
main(int argc, char *argv[]) {
    bool verbose = true;
    bool tracing = true;
    bool caching = true;
    std::string environment = "wall/wall.xml";
    int wifi_channel_num = 6;
    int channel_width = 20; // 802.11g supports only 20MHz

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Enable logging", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue("caching", "Enable caching of propagation delay and loss", caching);
    cmd.AddValue("environment", "Xml file of environment", environment);
    cmd.AddValue("channel", "The WiFi channel number", wifi_channel_num);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
        LogComponentEnable("YansWifiChannel", LOG_DEBUG);
        LogComponentEnable("YansWifiChannel", LOG_PREFIX_TIME);
        LogComponentEnable("SionnaPropagationDelayModel", LOG_INFO);
        LogComponentEnable("SionnaPropagationCache", LOG_INFO);
    }

    std::cout << "Example scenario with sionna and RIS" << std::endl << std::endl;

    SionnaHelper sionnaHelper(environment, "tcp://localhost:5555");

    // Create nodes
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    Vector stat0_position = Vector(10, 3, 2);
    Vector stat1_position = Vector(10, 7, 2);
    Vector ap_position = Vector(6, 5, 2);
     Vector ris_dir = (stat0_position + stat1_position) * 0.5;

    std::vector<std::shared_ptr<ns3::AbstractRisController>> ris_controllers;
    ris_controllers.push_back(std::make_shared<PeriodicRisController>(Vector(14, 5, 2), ris_dir, 0));

    // Create a channel
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

    // WiFi configuration
    YansWifiPhyHelper phy;
    phy.SetChannel(channel);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    WifiHelper wifi;

    WifiStandard wifi_standard = WIFI_STANDARD_80211g;
    wifi.SetStandard(wifi_standard);

    std::string channelStr = "{" + std::to_string(wifi_channel_num) + ", " + std::to_string(channel_width) + ", BAND_2_4GHZ, 0}";
    phy.Set("ChannelSettings", StringValue(channelStr));

    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    NetDeviceContainer apDevices;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconGeneration", BooleanValue(true), "BeaconInterval",
        TimeValue(Seconds(5.120)), "EnableBeaconJitter", BooleanValue(false));
    apDevices = wifi.Install(phy, mac, wifiApNode);

    // Mobility configuration
    MobilityHelper mobility;

    mobility.SetMobilityModel("ns3::SionnaMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    wifiStaNodes.Get(0)->GetObject<MobilityModel>()->SetPosition(stat0_position);
    wifiStaNodes.Get(1)->GetObject<MobilityModel>()->SetPosition(stat1_position);
    wifiApNode.Get(0)->GetObject<MobilityModel>()->SetPosition(ap_position);

    // Set up Internet stack and assign IP addresses
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiStaInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer wifiApInterfaces = address.Assign(apDevices);

    // Set up applications
    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverApps = echoServer.Install(wifiApNode);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(wifiApInterfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(2));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(wifiStaNodes);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    sionnaHelper.Configure(get_center_freq(apDevices.Get(0)), get_channel_width(apDevices.Get(0)));
    sionnaHelper.SetRIS(ris_controllers);

    // Tracing
    if (tracing)
    {
        phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy.EnablePcap("example-sionna", apDevices.Get(0));
        phy.EnablePcap("example-sionna", staDevices.Get(0));
        phy.EnablePcap("example-sionna", staDevices.Get(1));
    }

    if (verbose)
    {
        // Print node information
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
        for (const auto& ris_ptr : ris_controllers)
        {
            // The .name() result might be "mangled" (implementation-specific)
            std::cout << "RIS Type: " << typeid(*ris_ptr).name() << " ";

            ns3::Vector position = ris_ptr->getPosition();
            ns3::Vector lookAt = ris_ptr->getLookAt();

            std::cout << "Pos: [" << position.x << ", " << position.y << ", " << position.z << "]" << ", ";
            std::cout << "Look At: [" << lookAt.x << ", " << lookAt.y << ", " << lookAt.z << "]";
            std::cout << std::endl;
        }
    }

    // Simulation
    Simulator::Stop(Seconds(5.0));

    sionnaHelper.Start();

    Simulator::Run();
    Simulator::Destroy();

    std::cout << "Ns3-sionna: cache hit ratio: " <<  propagationCache->GetStats() << std::endl;

    sionnaHelper.Destroy();

    return 0;
}