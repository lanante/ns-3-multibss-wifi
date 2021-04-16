/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2019 University of Washington
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Sébastien Deronne <sebastien.deronne@gmail.com>
 */

//
//  This example program can be used to experiment with spatial
//  reuse mechanisms of 802.11ax.
//
//  The geometry is as follows:
//
//                STA1          STA1
//                 |              |
//               r |              |r
//                 |       d      |
//                AP1 -----------AP2
//
//  STA1 and AP1 are in one BSS (with color set to 1), while STA2 and AP2 are in
//  another BSS (with color set to 2). The distances are configurable (d1 through d3).
//
//  STA1 is continously transmitting data to AP1, while STA2 is continuously sending data to AP2.
//  Each STA has configurable traffic loads (inter packet interval and packet size).
//  It is also possible to configure TX power per node as well as their CCA-ED tresholds.
//  OBSS_PD spatial reuse feature can be enabled (default) or disabled, and the OBSS_PD
//  threshold can be set as well (default: -72 dBm).
//  A simple Friis path loss model is used and a constant PHY rate is considered.
//
//  In general, the program can be configured at run-time by passing command-line arguments.
//  The following command will display all of the available run-time help options:
//    ./waf --run "wifi-spatial-reuse --help"
//
//  By default, the script shows the benefit of the OBSS_PD spatial reuse script:
//    ./waf --run wifi-spatial-reuse
//    Throughput for BSS 1: 6.6468 Mbit/s
//    Throughput for BSS 2: 6.6672 Mbit/s
//
// If one disables the OBSS_PD feature, a lower throughput is obtained per BSS:
//    ./waf --run "wifi-spatial-reuse --enableObssPd=0"
//    Throughput for BSS 1: 5.8692 Mbit/s
//    Throughput for BSS 2: 5.9364 Mbit/
//
// This difference between those results is because OBSS_PD spatial
// enables to ignore transmissions from another BSS when the received power
// is below the configured threshold, and therefore either defer during ongoing
// transmission or transmit at the same time.
//

#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/string.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-helper.h"
#include "ns3/application-container.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/wifi-net-device.h"
#include "ns3/ap-wifi-mac.h"
#include "ns3/he-configuration.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-client.h"
#include "ns3/packet-socket-server.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

std::vector<uint64_t> bytesReceived (0);
std::vector<uint64_t> packetsReceived (0);
uint32_t
ContextToNodeId (std::string context)
{
  std::string sub = context.substr (10); // skip "/NodeList/"
  uint32_t pos = sub.find ("/Device");
  uint32_t nodeId = atoi (sub.substr (0, pos).c_str ());
  return nodeId;
}

void
SocketRx (std::string context, Ptr<const Packet> p, const Address &addr)
{
  uint32_t nodeId = ContextToNodeId (context);
  bytesReceived[nodeId] += p->GetSize ();
}

void
AddClient (ApplicationContainer &clientApps, Ipv4Address address, Ptr<Node> node, uint16_t port,
           Time interval, uint32_t payloadSize)
{
  UdpClientHelper client (address, port);
  client.SetAttribute ("Interval", TimeValue (interval));
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
  client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  clientApps.Add (client.Install (node));
}
void
AddServer (ApplicationContainer &serverApps, UdpServerHelper &server, Ptr<Node> node)
{
  serverApps.Add (server.Install (node));
}
void
PacketRx (std::string context, const Ptr<const Packet> p, const Address &srcAddress,
          const Address &destAddress)
{

  uint32_t nodeId = ContextToNodeId (context);
  
  uint32_t pktSize = p->GetSize ();
  bytesReceived[nodeId] += pktSize;
  packetsReceived[nodeId]++;

}
int
main (int argc, char *argv[])
{
  double duration = 10.0; // seconds
  double d = 30.0; // meters
  double r = 150.0; // meters
  double powSta1 = 10.0; // dBm
  double powSta2 = 10.0; // dBm
  double powAp1 = 21.0; // dBm
  double powAp2 = 21.0; // dBm
  double ccaEdTrSta1 = -62; // dBm
  double ccaEdTrSta2 = -62; // dBm
  double ccaEdTrAp1 = -62; // dBm
  double ccaEdTrAp2 = -62; // dBm
  uint32_t payloadSize = 1500; // bytes
  uint32_t mcs = 0; // MCS value
    uint32_t n = 1; // MCS value
        uint32_t nBss = 2; // MCS value
 // double interval = 0.00001; // seconds
  bool enableObssPd = false;
  double obssPdThreshold = -72.0; // dBm
  uint32_t  maxAmpduSize=10000;
    bool enablePcap = 0;
  double aggregateUplinkAMbps = 10;
  double aggregateUplinkBMbps = 10;
    double aggregateDownlinkAMbps = 0;
  double aggregateDownlinkBMbps = 0;
    uint32_t maxMissedBeacons = 4294967295;
  
  CommandLine cmd (__FILE__);
  cmd.AddValue ("duration", "Duration of simulation (s)", duration);
 // cmd.AddValue ("interval", "Inter packet interval (s)", interval);
  cmd.AddValue ("enableObssPd", "Enable/disable OBSS_PD", enableObssPd);
  cmd.AddValue ("r", "Distance between STA and AP (m)", r);
  cmd.AddValue ("d", "Distance between AP1 and AP2 (m)", d);
    cmd.AddValue ("n", "Number of STAs", n);
        cmd.AddValue ("nBss", "Number of BSS", nBss);
  cmd.AddValue ("powSta1", "Power of STA1 (dBm)", powSta1);
  cmd.AddValue ("powSta2", "Power of STA2 (dBm)", powSta2);
  cmd.AddValue ("powAp1", "Power of AP1 (dBm)", powAp1);
  cmd.AddValue ("powAp2", "Power of AP2 (dBm)", powAp2);
  cmd.AddValue ("ccaEdTrSta1", "CCA-ED Threshold of STA1 (dBm)", ccaEdTrSta1);
  cmd.AddValue ("ccaEdTrSta2", "CCA-ED Threshold of STA2 (dBm)", ccaEdTrSta2);
  cmd.AddValue ("ccaEdTrAp1", "CCA-ED Threshold of AP1 (dBm)", ccaEdTrAp1);
  cmd.AddValue ("ccaEdTrAp2", "CCA-ED Threshold of AP2 (dBm)", ccaEdTrAp2);
  cmd.AddValue ("mcs", "The constant MCS value to transmit HE PPDUs", mcs);
  cmd.AddValue ("maxAmpduSize", "BE_MaxAmpduSize", maxAmpduSize);
    cmd.AddValue ("enablePcap", "enablePcap", enablePcap);
  cmd.AddValue ("uplinkA", "Aggregate uplink load, BSS-A(Mbps)", aggregateUplinkAMbps);
  cmd.AddValue ("downlinkA", "Aggregate downlink load, BSS-A (Mbps)", aggregateDownlinkAMbps);
      cmd.AddValue ("uplinkB", "Aggregate uplink load, BSS-B(Mbps)", aggregateUplinkBMbps);
  cmd.AddValue ("downlinkB", "Aggregate downlink load, BSS-B (Mbps)", aggregateDownlinkBMbps);
    
  cmd.Parse (argc, argv);

  NodeContainer wifiStaNodes;
  //wifiStaNodes.Create (2);

  NodeContainer wifiApNodes;
  //wifiApNodes.Create (2);
  wifiApNodes.Create (nBss);
  NodeContainer wifiStaNodesA;
  NodeContainer wifiStaNodesB;

  Ssid ssid;
    NetDeviceContainer staDeviceA, apDeviceA;
  NetDeviceContainer staDeviceB, apDeviceB;
   uint32_t numNodes = nBss * (n + 1);
  
    double perNodeUplinkAMbps = aggregateUplinkAMbps / n;
  double perNodeDownlinkAMbps = aggregateDownlinkAMbps / n;
  Time intervalUplinkA = MicroSeconds (payloadSize * 8 / perNodeUplinkAMbps);
  Time intervalDownlinkA = MicroSeconds (payloadSize * 8 / perNodeDownlinkAMbps);

  double perNodeUplinkBMbps = aggregateUplinkBMbps / n;
  double perNodeDownlinkBMbps = aggregateDownlinkBMbps / n;
  Time intervalUplinkB = MicroSeconds (payloadSize * 8 / perNodeUplinkBMbps);
  Time intervalDownlinkB = MicroSeconds (payloadSize * 8 / perNodeDownlinkBMbps);
  
    packetsReceived = std::vector<uint64_t> (numNodes);
  bytesReceived = std::vector<uint64_t> (numNodes);
  
      Config::SetDefault ("ns3::QosTxop::UseExplicitBarAfterMissedBlockAck", BooleanValue (false));
     uint32_t uMaxSlrc = std::numeric_limits<uint32_t>::max ();
    Config::SetDefault ("ns3::WifiRemoteStationManager::MaxSlrc", UintegerValue (uMaxSlrc));
    Config::SetDefault ("ns3::WifiRemoteStationManager::MaxSsrc", UintegerValue (uMaxSlrc));
      Config::SetDefault ("ns3::WifiMacQueue::MaxDelay", TimeValue (MilliSeconds (duration * 1000)));
    
    
    wifiStaNodesA.Create (n);
  
  if (nBss > 1)
    {

      wifiStaNodesB.Create (n);
    }

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

  
  double apPositionX[7] = {0,d,d*2,d*2,d,-d};
 double apPositionY[7] = {0,0,0,d,d,d,d};
   for (uint8_t i = 0; i < nBss; i++)
    {
      positionAlloc->Add (Vector (apPositionX[i], apPositionY[i], 0.0));
    }
    
    
   // Set position for STAs
  int64_t streamNumber = 100;
  Ptr<UniformDiscPositionAllocator> unitDiscPositionAllocator1 =
      CreateObject<UniformDiscPositionAllocator> ();
  unitDiscPositionAllocator1->AssignStreams (streamNumber);
  // AP1 is at origin (x=x1, y=y1), with radius Rho=r
  unitDiscPositionAllocator1->SetX (apPositionX[0]);
  unitDiscPositionAllocator1->SetY (apPositionY[0]);
  unitDiscPositionAllocator1->SetRho (r);
  for (uint32_t i = 0; i < n; i++)
    {
      Vector v = unitDiscPositionAllocator1->GetNext ();
      positionAlloc->Add (v);

    }

  if (nBss > 1)
    {
      Ptr<UniformDiscPositionAllocator> unitDiscPositionAllocator2 =
          CreateObject<UniformDiscPositionAllocator> ();
      unitDiscPositionAllocator2->AssignStreams (streamNumber + 1);
      // AP2 is at origin (x=x2, y=y2), with radius Rho=r
      unitDiscPositionAllocator2->SetX (apPositionX[1]);
      unitDiscPositionAllocator2->SetY (apPositionY[1]);
      unitDiscPositionAllocator2->SetRho (r);
      for (uint32_t i = 0; i < n; i++)
        {
          Vector v = unitDiscPositionAllocator2->GetNext ();
          positionAlloc->Add (v);
        }
    }



  mobility.SetPositionAllocator (positionAlloc);
  NodeContainer allNodes = NodeContainer (wifiApNodes, wifiStaNodesA);
  if (nBss > 1)
    {
      allNodes = NodeContainer (allNodes, wifiStaNodesB);
    }
    mobility.Install (allNodes);
  
  
  
  SpectrumWifiPhyHelper spectrumPhy;
  Ptr<MultiModelSpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel> ();
     		Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel> ();
     // more prominent example values:
  lossModel ->SetAttribute ("ReferenceDistance", DoubleValue (1));
  lossModel ->SetAttribute ("Exponent", DoubleValue (3.5));
 lossModel ->SetAttribute ("ReferenceLoss", DoubleValue (50));
      spectrumChannel->AddPropagationLossModel (lossModel);  
        Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel> ();
  spectrumChannel->SetPropagationDelayModel (delayModel);

  spectrumPhy.SetChannel (spectrumChannel);
  spectrumPhy.SetErrorRateModel ("ns3::YansErrorRateModel");
  spectrumPhy.Set ("Frequency", UintegerValue (5180)); // channel 36 at 20 MHz
  spectrumPhy.SetPreambleDetectionModel ("ns3::ThresholdPreambleDetectionModel");
    spectrumPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);


    
    
    
    
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211ac);
  if (enableObssPd)
    {
      wifi.SetObssPdAlgorithm ("ns3::ConstantObssPdAlgorithm",
                               "ObssPdLevel", DoubleValue (obssPdThreshold));
    }




  WifiMacHelper mac;
  std::ostringstream oss;
  oss << "VhtMcs" << mcs;



  ssid = Ssid ("network-A");

wifiStaNodes=wifiStaNodesA;
double powAp=powAp1;
double ccaEdTrAp=ccaEdTrAp1;
double powSta=powSta1;
double ccaEdTrSta=ccaEdTrSta1;
//STA
  spectrumPhy.Set ("TxPowerStart", DoubleValue (powSta));
  spectrumPhy.Set ("TxPowerEnd", DoubleValue (powSta));
  spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaEdTrSta));
  spectrumPhy.Set ("RxSensitivity", DoubleValue (-92.0));
  mac.SetType ("ns3::StaWifiMac", "MaxMissedBeacons", UintegerValue (maxMissedBeacons), "Ssid",
               SsidValue (ssid));
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue (oss.str ()),
                                    "ControlMode", StringValue ("VhtMcs0"));
  staDeviceA = wifi.Install (spectrumPhy, mac, wifiStaNodes);


//AP
  spectrumPhy.Set ("TxPowerStart", DoubleValue (powAp));
  spectrumPhy.Set ("TxPowerEnd", DoubleValue (powAp));
  spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaEdTrAp));
  spectrumPhy.Set ("RxSensitivity", DoubleValue (-92.0));
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "EnableBeaconJitter", BooleanValue (true));

  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode",
                                   StringValue (oss.str ()), "ControlMode",
                                    StringValue ("VhtMcs0"));
                                    

                                                                            
   apDeviceA = wifi.Install (spectrumPhy, mac, wifiApNodes.Get (0));



 if (nBss > 1)
    {

      // network B
      ssid = Ssid ("network-B");


wifiStaNodes=wifiStaNodesB;
double powAp=powAp2;
double ccaEdTrAp=ccaEdTrAp2;
double powSta=powSta2;
double ccaEdTrSta=ccaEdTrSta2;
//STA
  spectrumPhy.Set ("TxPowerStart", DoubleValue (powSta));
  spectrumPhy.Set ("TxPowerEnd", DoubleValue (powSta));
  spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaEdTrSta));
  spectrumPhy.Set ("RxSensitivity", DoubleValue (-92.0));
  mac.SetType ("ns3::StaWifiMac", "MaxMissedBeacons", UintegerValue (maxMissedBeacons), "Ssid",
               SsidValue (ssid));
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue (oss.str ()),
                                    "ControlMode", StringValue ("VhtMcs0"));                                                        
  staDeviceB = wifi.Install (spectrumPhy, mac, wifiStaNodes);
                                                           

//AP
  spectrumPhy.Set ("TxPowerStart", DoubleValue (powAp));
  spectrumPhy.Set ("TxPowerEnd", DoubleValue (powAp));
  spectrumPhy.Set ("CcaEdThreshold", DoubleValue (ccaEdTrAp));
  spectrumPhy.Set ("RxSensitivity", DoubleValue (-92.0));
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "EnableBeaconJitter", BooleanValue (true));
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode",
                                   StringValue (oss.str ()), "ControlMode",
                                    StringValue ("VhtMcs0"));
   apDeviceB = wifi.Install (spectrumPhy, mac, wifiApNodes.Get (1));
    //  Ptr<WifiNetDevice> apDevice = apDeviceB.Get (0)->GetObject<WifiNetDevice> ();
  //  apDevice->GetMac ()->SetAttribute ("BE_MaxAmpduSize", UintegerValue (maxAmpduSize));
  
}


    // Internet stack
    InternetStackHelper stack;
  stack.Install (allNodes);


  
    Ipv4AddressHelper address; 
    address.SetBase ("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer StaInterfaceA; 
    StaInterfaceA = address.Assign (staDeviceA);    
    Ipv4InterfaceContainer ApInterfaceA;
    ApInterfaceA = address.Assign (apDeviceA);

    address.SetBase ("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer StaInterfaceB;
    StaInterfaceB = address.Assign (staDeviceB);
    Ipv4InterfaceContainer ApInterfaceB;
    ApInterfaceB = address.Assign (apDeviceB);

    // Setting applications
ApplicationContainer uplinkServerApps;
  ApplicationContainer downlinkServerApps;
  ApplicationContainer uplinkClientApps;
  ApplicationContainer downlinkClientApps;

  uint16_t uplinkPortA = 9;
  uint16_t downlinkPortA = 10;
  UdpServerHelper uplinkServerA (uplinkPortA);
  UdpServerHelper downlinkServerA (downlinkPortA);

  for (uint32_t i = 0; i < n; i++)
    {
      if (aggregateUplinkAMbps > 0)
        {
          AddClient (uplinkClientApps, ApInterfaceA.GetAddress (0), wifiStaNodesA.Get (i),
                     uplinkPortA, intervalUplinkA, payloadSize);

        }
      if (aggregateDownlinkAMbps > 0)
        {

          AddClient (downlinkClientApps, StaInterfaceA.GetAddress (i), wifiApNodes.Get (0),
                     downlinkPortA, intervalDownlinkA, payloadSize);
          AddServer (downlinkServerApps, downlinkServerA, wifiStaNodesA.Get (i));
        }
    }
  if (aggregateUplinkAMbps > 0)
    {
      AddServer (uplinkServerApps, uplinkServerA, wifiApNodes.Get (0));
    }

  // BSS 2
  if (nBss > 1)
    {
      uint16_t uplinkPortB = 11;
      uint16_t downlinkPortB = 12;
      UdpServerHelper uplinkServerB (uplinkPortB);
      UdpServerHelper downlinkServerB (downlinkPortB);
                                                                                             
      for (uint32_t i = 0; i < n; i++)
        {
          if (aggregateUplinkBMbps > 0)
            {
              AddClient (uplinkClientApps, ApInterfaceB.GetAddress (0), wifiStaNodesB.Get (i),
                         uplinkPortB, intervalUplinkB, payloadSize);
            }

          if (aggregateDownlinkBMbps > 0)
            {

              AddClient (downlinkClientApps, StaInterfaceB.GetAddress (i), wifiApNodes.Get (1),
                         downlinkPortB, intervalDownlinkB, payloadSize);

              AddServer (downlinkServerApps, downlinkServerB, wifiStaNodesB.Get (i));
            }

        }

      if (aggregateUplinkBMbps > 0)
        {
          AddServer (uplinkServerApps, uplinkServerB, wifiApNodes.Get (1));
        }
    }

  for (uint16_t i = 0; i < ((n + 1) * nBss); i++)
    {
      if (i < (n + 1)) // BSS 1
        {
          std::stringstream stmp;
          stmp << "/NodeList/" << i << "/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_MaxAmpduSize";
          Config::Set (stmp.str (), UintegerValue (std::min (maxAmpduSize, 4194303u)));
        }
      else if (i < (2 * (n + 1))) // BSS 2
        {
          std::stringstream stmp;
          stmp << "/NodeList/" << i << "/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_MaxAmpduSize";
          Config::Set (stmp.str (), UintegerValue (std::min (maxAmpduSize, 4194303u)));
        }
        }
        
        
    if (enablePcap)
      {
          spectrumPhy.EnablePcap ("AP_A", apDeviceA.Get (0));
          spectrumPhy.EnablePcap ("STA_A", staDeviceA.Get (0));
          if (nBss>=2)
          {
                    spectrumPhy.EnablePcap ("AP_B", apDeviceB.Get (0));
          spectrumPhy.EnablePcap ("STA_B", staDeviceB.Get (0));
          }
          
      }

     Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::UdpServer/RxWithAddresses",
                  MakeCallback (&PacketRx));

  Simulator::Stop (Seconds (duration+1));
  Simulator::Run ();

  Simulator::Destroy ();

double rxThroughputPerNode[numNodes];
  // output for all nodes
  for (uint32_t k = 0; k < nBss; k++)
    {
      double bitsReceived = bytesReceived[k] * 8;
      rxThroughputPerNode[k] = static_cast<double> (bitsReceived) / 1e6 / duration;
      std::cout << "Node " << k << ", pkts " << packetsReceived[k] << ", bytes " << bytesReceived[k]
                << ", throughput [MMb/s] " << rxThroughputPerNode[k] << std::endl;
       //   TputFile << rxThroughputPerNode[k] << std::endl;
    }


  return 0;
}
