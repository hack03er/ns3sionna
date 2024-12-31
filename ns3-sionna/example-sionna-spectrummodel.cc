/*
* Copyright (c) 2024 Zubow
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: zubow@tkn.tu-berlin.de
 */

// Sionna models
#include "lib/sionna-helper.h"
#include "lib/sionna-mobility-model.h"
#include "lib/sionna-propagation-cache.h"
#include "lib/sionna-propagation-delay-model.h"
#include "lib/sionna-propagation-loss-model.h"

// Ns-3 modules
#include "../../src/wifi/model/spectrum-wifi-phy.h"
#include "../../src/wifi/model/wifi-spectrum-phy-interface.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/network-module.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/wifi-net-device.h"
#include "ns3/yans-wifi-helper.h"
#include <ns3/spectrum-analyzer-helper.h>
#include <ns3/spectrum-analyzer.h>
#include <ns3/spectrum-model-ism2400MHz-res1MHz.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ExampleSionnaSpectrumModel");

double get_center_freq(Ptr<NetDevice> nd)
{
    Ptr<WifiPhy> wp = nd->GetObject<WifiNetDevice>()->GetPhy();
    return wp->GetObject<SpectrumWifiPhy>()->GetCurrentInterface()->GetCenterFrequency() * 1e6;
}

int
main(int argc, char* argv[])
{
    bool verbose = true;
    bool tracing = true;
    bool caching = true;
    std::string environment = "simple_room/simple_room.xml";
    int wifi_channel_num = 46;
    int channelWidth = 40;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Enable logging", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue("caching", "Enable caching of propagation delay and loss", caching);
    cmd.AddValue("environment", "Xml file of environment", environment);
    cmd.AddValue("channel", "The WiFi channel number", wifi_channel_num);
    cmd.AddValue("channelWidth", "The WiFi channel width in MHz", channelWidth);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
        LogComponentEnable("YansWifiChannel", LOG_DEBUG);
        LogComponentEnable("YansWifiChannel", LOG_PREFIX_TIME);
        LogComponentEnable("SionnaPropagationDelayModel", LOG_INFO);
        LogComponentEnable("SionnaPropagationCache", LOG_INFO);
    }

    std::cout << "Example scenario with sionna" << std::endl << std::endl;

    SionnaHelper sionnaHelper(environment, "tcp://localhost:5555");

    // Create nodes
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (channelWidth));

    Ptr<SionnaPropagationCache> propagationCache = CreateObject<SionnaPropagationCache>();
    propagationCache->SetSionnaHelper(sionnaHelper);
    propagationCache->SetCaching(caching);

    // new
    Ptr<MultiModelSpectrumChannel> spectrumChannel =
        CreateObject<MultiModelSpectrumChannel>();

    Ptr<SionnaPropagationLossModel> lossModel = CreateObject<SionnaPropagationLossModel>();
    lossModel->SetPropagationCache(propagationCache);

    spectrumChannel->AddPropagationLossModel(lossModel);

    Ptr<SionnaPropagationDelayModel> delayModel = CreateObject<SionnaPropagationDelayModel>();
    delayModel->SetPropagationCache(propagationCache);
    spectrumChannel->SetPropagationDelayModel(delayModel);

    SpectrumWifiPhyHelper spectrumPhy;
    spectrumPhy.SetChannel(spectrumChannel);
    spectrumPhy.SetErrorRateModel("ns3::NistErrorRateModel");
    spectrumPhy.Set("TxPowerStart", DoubleValue(1));
    spectrumPhy.Set("TxPowerEnd", DoubleValue(1));

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    WifiHelper wifi;

    WifiStandard wifi_standard = WIFI_STANDARD_80211ac;
    wifi.SetStandard(wifi_standard);

    std::string channelStr = "{" + std::to_string(wifi_channel_num) + ", " + std::to_string(channelWidth) + ", BAND_5GHZ, 0}";

    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    spectrumPhy.Set("ChannelSettings", StringValue(channelStr));
    staDevices = wifi.Install(spectrumPhy, mac, wifiStaNodes);

    NetDeviceContainer apDevices;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconGeneration", BooleanValue(true), "BeaconInterval", TimeValue(Seconds(5.120)), "EnableBeaconJitter", BooleanValue(false));
    spectrumPhy.Set("ChannelSettings", StringValue(channelStr));
    apDevices = wifi.Install(spectrumPhy, mac, wifiApNode);

    // Mobility configuration
    MobilityHelper mobility;

    mobility.SetMobilityModel("ns3::SionnaMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    wifiStaNodes.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(5.0, 2.05, 1.0));
    wifiStaNodes.Get(1)->GetObject<MobilityModel>()->SetPosition(Vector(5.0, 1.95, 1.0));
    wifiApNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(1.0, 2.0, 1.0));

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
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(wifiStaNodes);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // set center frequency for Sionna
    double fc = get_center_freq(apDevices.Get(0));
    std::cout << "fc: " << fc << std::endl;

    // set center frequency & bandwidth for Sionna
    sionnaHelper.Configure(fc, channelWidth * 1e6);

    // Tracing
    if (tracing)
    {
        spectrumPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        spectrumPhy.EnablePcap("example-sionna-spectrummodel", apDevices.Get(0));
        spectrumPhy.EnablePcap("example-sionna-spectrummodel", staDevices.Get(0));
        spectrumPhy.EnablePcap("example-sionna-spectrummodel", staDevices.Get(1));
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
    }

    SpectrumAnalyzerHelper spectrumAnalyzerHelper;
    spectrumAnalyzerHelper.SetChannel(spectrumChannel);
    spectrumAnalyzerHelper.SetRxSpectrumModel(SpectrumModelIsm2400MhzRes1Mhz);
    spectrumAnalyzerHelper.SetPhyAttribute("Resolution", TimeValue(MilliSeconds(2)));
    spectrumAnalyzerHelper.SetPhyAttribute("NoisePowerSpectralDensity",
                                           DoubleValue(1e-15)); // -120 dBm/Hz
    spectrumAnalyzerHelper.EnableAsciiAll("spectrum-analyzer-output");
    NetDeviceContainer spectrumAnalyzerDevices =
        spectrumAnalyzerHelper.Install(wifiApNode);


    // Simulation
    Simulator::Stop(Seconds(10.0));

    sionnaHelper.Start();

    Simulator::Run();
    Simulator::Destroy();

    sionnaHelper.Destroy();

    return 0;
}

