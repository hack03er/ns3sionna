/*
 * Copyright (c) 2024 Yannik Pilz, Zubow
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Yannik Pilz <y.pilz@campus.tu-berlin.de>
 */

#ifndef SIONNA_HELPER_H
#define SIONNA_HELPER_H

#include "message.pb.h"

#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"

#include <zmq.hpp>

namespace ns3
{

class SionnaHelper
{
public:
  static TypeId GetTypeId();

  SionnaHelper(std::string environment, std::string zmq_url);
  virtual ~SionnaHelper();

  void RandomVariableStreamMessage(ns3sionna::SimInitMessage::NodeInfo::RandomWalkModel::RandomVariableStream* stream,
                                   Ptr<RandomVariableStream> variable);

  void Configure(double frequency, double channel_bw);

  void Start();

  void Destroy();

  void SetMode(int mode);

  void SetSubMode(int sub_mode);

  double GetNoiseFloor();

private:
  void SetFrequency(double frequency);
  void SetChannelBandwidth(double channel_bw);
  void SetFFTSize(int fft_size);

private:
  std::string m_environment;
  int m_mode; // 1=P2P, 2=P2MP, 3=P2MP=LAH
  int m_sub_mode; // used by mode 3
  zmq::context_t m_zmq_context;
  double m_frequency;
  double m_channel_bw;
  int m_fft_size;
  double m_noiseDbm;

public:
  zmq::socket_t m_zmq_socket;

  // possible modes of operation
  static const int MODE_P2P = 1; // only a single P2P is computed within a single Sionna call
  static const int MODE_P2MP = 2; // a full P2MP (TX to all other RX nodes) is computed within a single Sionna call
  static const int MODE_P2MP_LAH = 3; // same as mode 2 but in addition also future not yet needed channels are computed
};
} // namespace ns3

#endif // SIONNA_HELPER_H