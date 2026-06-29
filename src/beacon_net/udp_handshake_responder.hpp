#pragma once

#include "beacon/agent_registry.hpp"
#include "beacon/config.hpp"

#include <atomic>
#include <thread>

namespace beacon::net {

class UdpHandshakeResponder {
 public:
  UdpHandshakeResponder(NodeConfig config, AgentRegistry& registry);
  ~UdpHandshakeResponder();

  UdpHandshakeResponder(const UdpHandshakeResponder&) = delete;
  UdpHandshakeResponder& operator=(const UdpHandshakeResponder&) = delete;

  void start();
  void stop();

 private:
  void run();

  NodeConfig config_;
  AgentRegistry& registry_;
  std::atomic_bool stopping_{false};
  std::thread worker_;
};

}  // namespace beacon::net

